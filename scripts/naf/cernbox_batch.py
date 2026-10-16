#!/usr/bin/env python3
"""Opt-in NAF local-scratch -> CERNBox staging; no credential contents in logs."""
import argparse
import base64
import datetime
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import signal
import subprocess
import sys
import time
import urllib.parse
import zlib

BASE = '/eos/user/l/lobisapa/IAXO/NAF/'
CAMPAIGNS_BASE = BASE + 'campaigns/'
HOST = 'roots://eoshome-l.cern.ch'
REVISION = 'pinned-rest-runtime-v4'
LOCAL_FILESYSTEMS = {'xfs', 'ext4', 'ext3', 'ext2', 'tmpfs', 'btrfs', 'overlay', 'zfs'}


class StageError(RuntimeError):
    pass


def safe_name(value):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,119}', value) or value in ('.', '..'):
        raise StageError('Campaign and job names must use letters, digits, dot, underscore or hyphen')
    return value


def remote_base(record):
    # Missing field denotes an existing pre-campaigns/ configuration or receipt.
    base = record.get('remote_base', BASE)
    if base not in (BASE, CAMPAIGNS_BASE):
        raise StageError('Unknown campaign output base; refusing relocation')
    return base


def remote_prefix(receipt):
    return remote_base(receipt) + safe_name(receipt['campaign']) + '/' + safe_name(receipt['job']) + '/'


def write_json(path, data):
    tmp = path.with_name(path.name + '.new')
    with tmp.open('w') as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write('\n')
        f.flush()
        os.fsync(f.fileno())
    tmp.replace(path)


def fingerprint(path):
    s = path.stat()
    return s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns


def checksums(path):
    before = fingerprint(path)
    adler, sha = 1, hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
            adler = zlib.adler32(block, adler)
            sha.update(block)
    if fingerprint(path) != before:
        raise StageError('Source changed while computing checksums')
    return {'bytes': before[2], 'adler32': f'{adler & 0xffffffff:08x}', 'sha256': sha.hexdigest()}, before


def token_claims(token):
    def vint(b, i):
        n, shift = 0, 0
        while True:
            x = b[i]; i += 1; n |= (x & 127) << shift
            if x < 128:
                return n, i
            shift += 7
            if shift > 70:
                raise StageError('Invalid token encoding')
    def wire(b):
        fields, i = {}, 0
        while i < len(b):
            tag, i = vint(b, i); typ = tag & 7
            if typ == 0:
                v, i = vint(b, i)
            elif typ == 2:
                n, i = vint(b, i); v = b[i:i+n]; i += n
            elif typ in (1, 5):
                n = 8 if typ == 1 else 4; v = b[i:i+n]; i += n
            else:
                raise StageError('Invalid token encoding')
            fields.setdefault(tag >> 3, []).append(v)
        return fields
    try:
        if not token.startswith('zteos64:') or '\n' in token:
            raise ValueError()
        blob = base64.urlsafe_b64decode(urllib.parse.unquote(token.split(':', 1)[1]))
        inner = wire(wire(zlib.decompress(blob[8:]))[1][0])
        claims = {k: inner[k][0] for k in (1, 2, 3, 4, 6, 7)}
        for k in (1, 3, 4, 6):
            claims[k] = claims[k].decode()
        if claims[6] != BASE or claims[7] != 1 or claims[3] not in ('lobisapa', '133153'):
            raise StageError('Token identity or subtree does not match this workflow')
        if claims[2] <= time.time() + 3600:
            raise StageError('Token expires in less than one hour; replace it before submission')
        return claims
    except StageError:
        raise
    except Exception:
        raise StageError('Cannot decode native EOS token') from None


class EOS:
    def __init__(self, token_file, ca_file, timeout=1800):
        p = Path(token_file)
        if p.stat().st_mode & 0o077:
            raise StageError('Token file must have mode 0600')
        self.token = p.read_text().strip()
        self.claims = token_claims(self.token)
        self.timeout = timeout
        self.deadline = None
        self.env = os.environ.copy()
        for k in ('EOSAUTHZ', 'BEARER_TOKEN', 'BEARER_TOKEN_FILE', 'XrdSecsssENDORSEMENT',
                  'OAUTH2_TOKEN', 'XrdSecSSSKT', 'X509_USER_PROXY', 'X509_USER_CERT',
                  'X509_USER_KEY', 'X509_CERT_DIR', 'X509_CERT_FILE',
                  'XRD_LOGLEVEL', 'XRD_LOGFILE', 'XRD_DEBUG', 'XrdSecDEBUG'):
            self.env.pop(k, None)
        self.env.update(XrdSecPROTOCOL='unix', X509_CERT_FILE=str(Path(ca_file).resolve()),
                        XRD_NOTLSOK='0', XRD_TLSNODATA='0', XRD_CONNECTIONWINDOW='15', XRD_CONNECTIONRETRY='2',
                        XRD_REQUESTTIMEOUT='60', XRD_CPINITTIMEOUT='90',
                        XRD_CPTIMEOUT=str(timeout), KRB5CCNAME='FILE:/nonexistent/iaxo-no-kerberos')
        if not Path(ca_file).is_file():
            raise StageError('CA trust bundle does not exist')
        for binary in ('xrdcp', 'xrdfs'):
            if not shutil.which(binary):
                raise StageError(f'{binary} is missing from PATH')

    def opaque(self, remote):
        if not remote.startswith(BASE) or '..' in Path(remote).parts:
            raise StageError('Remote path escapes the delegated subtree')
        return urllib.parse.quote(remote, safe='/._-') + '?authz=' + self.token

    def execute(self, command):
        timeout = self.timeout
        if getattr(self, 'deadline', None) is not None:
            timeout = min(timeout, self.deadline - time.monotonic())
            if timeout <= 0:
                raise StageError('Whole upload time budget exceeded; remaining output retained')
        try:
            p = subprocess.run(command, env=self.env, capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            raise StageError('Transfer operation timed out; local output retained') from None
        output = (p.stdout + '\n' + p.stderr).replace(self.token, '[TOKEN REDACTED]')
        output = re.sub(r'zteos64(?:%3[Aa]|:)[A-Za-z0-9_%~+/=\-]+', '[TOKEN REDACTED]', output)
        return p.returncode, output

    def metadata(self, remote):
        rc, out = self.execute(['xrdfs', HOST, 'stat', self.opaque(remote)])
        if rc:
            if re.search(r'\[3011\]|[Nn]o such file|[Nn]ot found', out):
                return None
            raise StageError('Remote stat failed: ' + out[-1200:])
        m = re.search(r'^\s*Size:\s*(\d+)', out, re.M)
        if not m:
            raise StageError('Remote stat did not return a file size')
        return int(m.group(1))

    def checksum(self, remote):
        rc, out = self.execute(['xrdfs', HOST, 'query', 'checksum', self.opaque(remote)])
        if rc:
            raise StageError('Explicit checksum query failed: ' + out[-1200:])
        m = re.search(r'\badler32\s+([0-9a-fA-F]{1,8})\b', out)
        if not m:
            raise StageError('Remote checksum response is not Adler-32')
        return m.group(1).lower().zfill(8)

    def upload(self, source, remote):
        rc, out = self.execute(['xrdfs', HOST, 'mkdir', '-p', self.opaque(str(Path(remote).parent) + '/')])
        if rc:
            raise StageError('Remote directory creation failed: ' + out[-1200:])
        # --cksum loses authz on some clients. Query the checksum explicitly below.
        rc, out = self.execute(['xrdcp', '--nopbar', '--posc', str(source), HOST + '/' + self.opaque(remote)])
        if rc:
            raise StageError('Upload failed: ' + out[-1200:])


def stage_file(eos, source, remote, record, remove=False, attempts=3):
    if source.is_symlink() or not source.is_file():
        raise StageError('Only regular output files may be uploaded')
    sums, original = checksums(source)
    last = None
    for attempt in range(attempts):
        try:
            size = eos.metadata(remote)
            if size is None:
                eos.upload(source, remote)
                size = eos.metadata(remote)
            if size != sums['bytes'] or eos.checksum(remote) != sums['adler32']:
                raise StageError('Remote size/checksum mismatch; local file retained; remote not overwritten')
            if fingerprint(source) != original:
                raise StageError('Source changed during transfer; local file retained')
            entry = dict(sums, remote=remote)
            record(entry)  # Durable receipt before removing this closed local file.
            if remove:
                if fingerprint(source) != original:
                    raise StageError('Source changed before cleanup; local file retained')
                source.unlink()
            return entry
        except StageError as e:
            last = e
            if attempt + 1 < attempts:
                time.sleep(min(15, 2 ** attempt))
    raise last


def scratch_root():
    for k in ('_CONDOR_SCRATCH_DIR', 'TMPDIR', 'TMP'):
        if os.environ.get(k):
            p = Path(os.environ[k]).resolve()
            if any(p == Path(x) or Path(x) in p.parents for x in ('/data', '/afs', '/pnfs', '/eos', '/storage')):
                continue
            if p.is_dir() and filesystem_type(p) in LOCAL_FILESYSTEMS:
                return p
    raise StageError('No worker-local scratch directory found; refusing to write simulation output to DUST/AFS')


def filesystem_type(path):
    """Use the longest matching Linux mount, including escaped mount names."""
    matches = []
    for line in Path('/proc/mounts').read_text().splitlines():
        fields = line.split()
        mount = Path(re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), fields[1]))
        if path == mount or mount in path.parents:
            matches.append((len(mount.parts), fields[2]))
    if not matches:
        raise StageError('Cannot establish worker-local scratch filesystem')
    return max(matches)[1]


def verified_copy(source, target, expected):
    if source.is_symlink() or not source.is_file():
        raise StageError('Pinned input must be a regular file: ' + str(source))
    sums, before = checksums(source)
    if sums['sha256'] != expected:
        raise StageError('Changed pinned input: ' + str(source))
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    if fingerprint(source) != before or checksums(target)[0]['sha256'] != expected:
        raise StageError('Pinned input changed during copy: ' + str(source))


def rest_environment(root, recovery, config, env):
    """Fresh writable state only; never recursively copy a user's .rest cache."""
    expected_install = config['rest'].get('installation_files')
    if expected_install is not None:
        observed = rest_installation_files(Path(config['rest']['install']))
        if observed != expected_install:
            raise StageError('Selected REST installation changed after preparation; physics was not started')
    runtime = recovery / '.worker-runtime'
    runtime.mkdir()
    home = runtime / 'home'
    for relative in ('home/.rest/gdml', 'home/.rest/download', 'home/.rest/AddonDict', 'cache', 'tmp'):
        (runtime / relative).mkdir(parents=True, exist_ok=True)
    resources = config['rest'].get('resources', {})
    for relative, item in resources.items():
        safe_resource(relative)
        verified_copy(root / 'rest-resources' / relative, home / '.rest' / relative, item['sha256'])
    if 'runNumber' not in resources:
        (home / '.rest/runNumber').write_text('1\n')
    env.update(HOME=str(home), REST_HOME=str(home), XDG_CACHE_HOME=str(runtime / 'cache'),
               TMPDIR=str(runtime / 'tmp'), TMP=str(runtime / 'tmp'),
               USER=env.get('USER') or pwd.getpwuid(os.getuid()).pw_name,
               PYTHONDONTWRITEBYTECODE='1', IAXO_REST_INSTALL=config['rest']['install'],
               IAXO_GSL_PREFIX=config['rest']['gsl_prefix'], IAXO_PRIVATE_HOME=str(home),
               IAXO_JOB_CPUS=str(config.get('cpus', 1)))
    for key in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS'):
        env[key] = str(config.get('cpus', 1))
    return runtime


def rest_installation_files(install):
    """Bind the installed code, headers, setup and data, including symlink targets.

    Use a versioned prefix: verification detects accidental mutation before a
    job starts, but cannot make a mutable installation immutable while it runs.
    """
    files = {}
    for path in sorted(install.rglob('*')):
        if path.is_symlink() and path.is_dir():
            raise StageError('REST installation must not contain directory symlinks: ' + str(path))
        if path.is_symlink() and not path.is_file():
            raise StageError('Broken REST installation symlink: ' + str(path))
        if path.is_file():
            item = checksums(path)[0]
            if path.is_symlink():
                item['symlink'] = os.readlink(path)
            files[str(path.relative_to(install))] = item
    if not files:
        raise StageError('Selected REST installation is empty')
    return files


def safe_resource(relative):
    path = Path(relative)
    if (not relative or path.is_absolute() or '..' in path.parts or str(path) != relative
            or path.parts[0] in ('gdml', 'AddonDict') or relative == 'download'):
        raise StageError('Unsafe or disposable REST resource path: ' + relative)


REST_BOOTSTRAP = '''#!/usr/bin/env bash
set -eo pipefail
# thisREST.sh references unset variables and resets REST_HOME to HOME.
source "$IAXO_REST_INSTALL/thisREST.sh" >/dev/null
export HOME="$IAXO_PRIVATE_HOME" REST_HOME="$IAXO_PRIVATE_HOME"
export REST_PATH="$IAXO_REST_INSTALL"
export LD_LIBRARY_PATH="$IAXO_GSL_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
ulimit -c 0
for binary in restG4 restRoot restManager; do
    test -x "$REST_PATH/bin/$binary"
done
for dataset in G4NEUTRONHPDATA G4LEDATA G4LEVELGAMMADATA G4RADIOACTIVEDATA; do
    if [[ -z "${!dataset}" || ! -d "${!dataset}" ]]; then
        echo "Missing Geant4 dataset: $dataset" >&2
        exit 69
    fi
done
cat > "$TMPDIR/iaxo_runtime_preflight.C" <<'IAXO_RUNTIME_PREFLIGHT'
#include <TRestTools.h>
void iaxo_runtime_preflight() {
    if (gSystem->Load("libgslcblas.so.0") < 0 || gSystem->Load("libgsl.so.27") < 0 || gSystem->Load("libRestGeant4.so") < 0 || gSystem->Load("libRestDetector.so") < 0) gSystem->Exit(70);
    if (REST_USER_PATH != std::string(gSystem->Getenv("IAXO_PRIVATE_HOME")) + "/.rest") gSystem->Exit(71);
    std::cout << "IAXO_PRIVATE_REST_PREFLIGHT_OK " << REST_USER_PATH << std::endl;
}
IAXO_RUNTIME_PREFLIGHT
preflight=$("$REST_PATH/bin/restRoot" -b -q "$TMPDIR/iaxo_runtime_preflight.C")
printf '%s\\n' "$preflight"
printf '%s\\n' "$preflight" | grep -Fqx "IAXO_PRIVATE_REST_PREFLIGHT_OK $IAXO_PRIVATE_HOME/.rest"
exec bash "$1"
'''


def output_size(directory):
    """Count stable output while the simulation removes disposable caches.

    Ignore only paths disappearing during the walk. Permission and I/O errors
    still fail staging; a missing output root is never a successful empty run.
    """
    if not directory.is_dir():
        raise StageError('Output directory disappeared')
    def onerror(error):
        if not isinstance(error, FileNotFoundError):
            raise error
    total = 0
    for parent, folders, files in os.walk(directory, onerror=onerror):
        for name in files:
            try:
                total += (Path(parent) / name).stat().st_size
            except FileNotFoundError:
                continue
    return total


def run_worker(config_path, job):
    config = json.loads(Path(config_path).read_text())
    safe_name(config['campaign']); safe_name(job)
    root = scratch_root()
    if Path.cwd().resolve() != root:
        raise StageError('Run from the Condor scratch directory')
    recovery = root / 'recovery'; recovery.mkdir(exist_ok=True)
    receipt_path = root / 'receipt.json'
    receipt = json.loads(receipt_path.read_text()) if receipt_path.exists() else {
        'campaign': config['campaign'], 'remote_base': remote_base(config),
        'job': job, 'state': 'starting', 'files': [], 'simulation_complete': False}
    if receipt['job'] != job or receipt['campaign'] != config['campaign']:
        raise StageError('Scratch receipt belongs to a different job')
    if remote_base(receipt) != remote_base(config):
        raise StageError('Scratch receipt and configuration use different output bases')
    receipt['helper_revision'] = REVISION
    def save():
        write_json(receipt_path, receipt)
    def record(entry):
        by_path = {x['remote']: x for x in receipt['files']}
        by_path[entry['remote']] = entry
        receipt['files'] = list(by_path.values()); save()
    save()
    try:
        eos = EOS(root / 'naf-upload.token', root / 'cern-ca-bundle.pem', config['transfer_timeout'])
        if receipt.get('state') == 'complete':
            return 0
        if not receipt['simulation_complete']:
            if any(recovery.iterdir()):
                raise StageError('Recovered incomplete simulation output exists; review it rather than overwrite it')
            receipt['state'] = 'simulating'; save()
            env = os.environ.copy()
            env['IAXO_OUTPUT_DIR'] = str(recovery)
            env['IAXO_JOB_ID'] = job
            env['IAXO_CAMPAIGN'] = config['campaign']
            runtime = None
            command = ['bash', str(root / 'simulation.sh')]
            # Configurations prepared before v3 retain their original runtime.
            for name, expected in config.get('input_sha256', {}).items():
                if checksums(root / name)[0]['sha256'] != expected:
                    raise StageError('Changed prepared input: ' + name)
            if 'rest' in config:
                runtime = rest_environment(root, recovery, config, env)
                command = ['bash', str(root / 'rest-bootstrap.sh'), str(root / 'simulation.sh')]
            with (recovery / 'simulation.log').open('wb') as logfile:
                proc = subprocess.Popen(command, cwd=root, env=env,
                                        stdout=logfile, stderr=subprocess.STDOUT, start_new_session=True)
                started = time.monotonic()
                try:
                    while proc.poll() is None:
                        total = output_size(recovery)
                        if total > config['max_output_bytes']:
                            raise StageError('Output size budget exceeded; remaining files retained in recovery')
                        if time.monotonic() - started > config['simulation_timeout']:
                            raise StageError('Simulation time budget exceeded; remaining files retained in recovery')
                        time.sleep(2)
                except BaseException:
                    try:
                        os.killpg(proc.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    try:
                        proc.wait(timeout=20)
                    except subprocess.TimeoutExpired:
                        os.killpg(proc.pid, signal.SIGKILL); proc.wait()
                    raise
            if proc.returncode != 0:
                raise StageError(f'Simulation exited {proc.returncode}; output retained in recovery')
            if runtime is not None:
                shutil.rmtree(runtime)
            files = [f for f in recovery.rglob('*') if f.is_file() and f.name != 'simulation.log']
            if not files:
                raise StageError('Simulation produced no data files in IAXO_OUTPUT_DIR')
            total = output_size(recovery)
            if total > config['max_output_bytes']:
                raise StageError('Output size budget exceeded; output retained in recovery')
            if any(f.name == '_manifest.json' for f in recovery.rglob('*')):
                raise StageError('_manifest.json is reserved for the verified upload manifest')
            # Small, non-secret prepared controls travel with the physics output.
            if config.get('input_sha256'):
                provenance = recovery / '_batch'
                if provenance.exists():
                    raise StageError('_batch is reserved for prepared runtime provenance')
                provenance.mkdir()
                for name in config['input_sha256']:
                    shutil.copyfile(root / name, provenance / name)
                if 'rest' in config and config['rest'].get('resources'):
                    shutil.copytree(root / 'rest-resources', provenance / 'rest-resources')
                write_json(provenance / 'prepared-job.json', config)
                if output_size(recovery) > config['max_output_bytes']:
                    raise StageError('Output plus provenance exceeds size budget; retained in recovery')
            receipt['simulation_complete'] = True; save()
        receipt['state'] = 'uploading'; save()
        if 'upload_timeout' in config:
            eos.deadline = time.monotonic() + config['upload_timeout']
        finish_upload(eos, recovery, receipt, save, record)
        return 0
    except (StageError, OSError) as error:
        receipt['state'] = 'failed'; receipt['error'] = str(error); save()
        print('CERNBox staging failed; recovery files retained. ' + str(error), file=sys.stderr)
        return 73


def finish_upload(eos, recovery, receipt, save, record):
    config = {'campaign': receipt['campaign']}
    job = receipt['job']
    prefix = remote_prefix(receipt)
    if any(not row['remote'].startswith(prefix) for row in receipt['files']):
        raise StageError('Existing receipt files belong to a different output prefix')
    for source in sorted(recovery.rglob('*')):
        if source.is_symlink():
            raise StageError('Output contains a symlink; refusing automatic cleanup')
        if source.is_file() and source.name != '_manifest.json':
            remote = prefix + source.relative_to(recovery).as_posix()
            stage_file(eos, source, remote, record, remove=True)
    manifest = recovery / '_manifest.json'
    if not manifest.exists():
        write_json(manifest, {'campaign': config['campaign'], 'job': job, 'remote_base': remote_base(receipt),
                             'files': sorted(receipt['files'], key=lambda x: x['remote'])})
    stage_file(eos, manifest, prefix + '_manifest.json', lambda x: None, remove=True)
    for directory in sorted((p for p in recovery.rglob('*') if p.is_dir()), reverse=True):
        directory.rmdir()
    receipt['state'] = 'complete'; receipt['completed_at'] = datetime.datetime.now(datetime.timezone.utc).isoformat(); save()
    print(json.dumps({'state': 'complete', 'job': job, 'remote_manifest': prefix + '_manifest.json',
                      'data_files': len(receipt['files']), 'local_output_files': len(list(recovery.rglob('*')))}))


def retry_upload(args):
    if getattr(args, 'transfer_minutes', 30) < 1:
        raise StageError('Transfer budget must be positive')
    directory = Path(args.job_dir).expanduser().resolve()
    path = directory / 'receipt.json'
    receipt = json.loads(path.read_text())
    if not receipt.get('simulation_complete'):
        raise StageError('Simulation did not complete; review partial files rather than upload as final output')
    safe_name(receipt['campaign']); safe_name(receipt['job'])
    recovery = directory / 'recovery'
    if not recovery.is_dir():
        raise StageError('Recovery directory is missing')
    def save():
        write_json(path, receipt)
    def record(entry):
        entries = {r['remote']: r for r in receipt['files']}
        entries[entry['remote']] = entry
        receipt['files'] = list(entries.values()); save()
    credentials = Path(args.credentials).expanduser().resolve()
    eos = EOS(credentials / 'naf-upload.token', credentials / 'cern-ca-bundle.pem')
    eos.deadline = time.monotonic() + getattr(args, 'transfer_minutes', 30) * 60
    if receipt.get('state') == 'complete' and not any(recovery.rglob('*')):
        print('Already complete; nothing to transfer')
        return 0
    receipt['state'] = 'uploading'; save()
    finish_upload(eos, recovery, receipt, save, record)
    return 0


def prepare(args):
    safe_name(args.campaign)
    if not 1 <= args.jobs <= 10000 or not 1 <= args.max_running <= 8:
        raise StageError('Use 1..10000 jobs and 1..8 materialized jobs')
    if min(args.max_output_gb, args.memory_mb, args.runtime_minutes, args.transfer_minutes) < 1:
        raise StageError('Output, memory and runtime budgets must be positive')
    output_bytes = args.max_output_gb * 1024 ** 3
    if getattr(args, 'max_output_mb', None) is not None:
        if args.max_output_mb < 1:
            raise StageError('Output budget must be positive')
        output_bytes = args.max_output_mb * 1024 ** 2
    cpus = getattr(args, 'cpus', 1)
    if not 1 <= cpus <= 64:
        raise StageError('Use 1..64 requested CPUs; pass the same thread count to restG4')
    credentials = Path(args.credentials).expanduser().resolve()
    eos = EOS(credentials / 'naf-upload.token', credentials / 'cern-ca-bundle.pem')
    root = Path(args.run_dir).expanduser().resolve()
    if root.exists():
        raise StageError('Run directory already exists; choose a new campaign directory')
    source = Path(args.script).expanduser().resolve()
    if not source.is_file():
        raise StageError('Simulation script does not exist')
    syntax = subprocess.run(['bash', '-n', str(source)], capture_output=True, text=True)
    if syntax.returncode:
        raise StageError('Simulation script has invalid Bash syntax: ' + syntax.stderr[-1200:])
    executable = root / 'cernbox_batch.py'
    inputs = [credentials / 'naf-upload.token', credentials / 'cern-ca-bundle.pem', root / 'job.json', root / 'simulation.sh']
    rest = None
    resources = {}
    if getattr(args, 'rest_resources', None) and not getattr(args, 'rest_install', None):
        raise StageError('--rest-resources requires --rest-install')
    if getattr(args, 'rest_install', None):
        install = Path(args.rest_install).expanduser().resolve()
        gsl = Path(args.gsl_prefix).expanduser().resolve()
        if not (install / 'thisREST.sh').is_file() or not (gsl / 'lib').is_dir():
            raise StageError('REST setup or GSL library directory is missing')
        for name in ('restG4', 'restRoot', 'restManager'):
            if not os.access(install / 'bin' / name, os.X_OK):
                raise StageError('Missing executable in selected REST install: ' + name)
        if getattr(args, 'rest_resources', None):
            resources = json.loads(Path(args.rest_resources).read_text())
            if not isinstance(resources, dict):
                raise StageError('REST resources must map relative .rest paths to path/sha256 records')
            for relative, item in resources.items():
                safe_resource(relative)
                if not isinstance(item, dict) or not {'path', 'sha256'} <= item.keys():
                    raise StageError('Resource requires path and sha256: ' + relative)
                if not re.fullmatch('[0-9a-f]{64}', item['sha256']):
                    raise StageError('Invalid resource SHA256: ' + relative)
                path = Path(item['path'])
                if not path.is_absolute() or path.is_symlink() or not path.is_file():
                    raise StageError('Resource must name an absolute regular file: ' + relative)
                if checksums(path)[0]['sha256'] != item['sha256']:
                    raise StageError('Changed preserved REST resource: ' + relative)
        rest = {'install': str(install), 'gsl_prefix': str(gsl), 'resources': resources,
                'installation_files': rest_installation_files(install)}
        inputs.append(root / 'rest-bootstrap.sh')
        if resources:
            inputs.append(root / 'rest-resources')
    # Reject invalid paths before leaving a half-prepared campaign behind.
    for path in inputs + [executable, root]:
        if re.search(r'[\s,;"\\$#]', str(path)):
            raise StageError('Submit file paths must not contain whitespace or Condor delimiters')
    # Recovery in another campaign need not block an explicitly authorized
    # independent run. Preserve it and record the outstanding storage instead.
    recovered_outputs = []
    for recovered in root.parent.glob('*/jobs/*/recovery'):
        files = [p for p in recovered.rglob('*') if p.is_file()]
        if files and not getattr(args, 'allow_existing_recovery', False):
            raise StageError('Unresolved recovery output exists at ' + str(recovered))
        if files:
            recovered_outputs.append({'path': str(recovered), 'files': len(files),
                                      'bytes': sum(p.stat().st_size for p in files)})
    root.mkdir(parents=True)
    (root / 'logs').mkdir()
    for i in range(args.jobs):
        (root / 'jobs' / str(i)).mkdir(parents=True)
    source_sha = checksums(source)[0]['sha256']
    verified_copy(source, root / 'simulation.sh', source_sha)
    shutil.copyfile(Path(__file__).resolve(), executable)
    executable.chmod(0o755)
    pinned = {'simulation.sh': source_sha, 'cernbox_batch.py': checksums(executable)[0]['sha256']}
    if rest is not None:
        (root / 'rest-bootstrap.sh').write_text(REST_BOOTSTRAP)
        pinned['rest-bootstrap.sh'] = checksums(root / 'rest-bootstrap.sh')[0]['sha256']
        for relative, item in resources.items():
            verified_copy(Path(item['path']), root / 'rest-resources' / relative, item['sha256'])
    config = {'campaign': args.campaign, 'remote_base': CAMPAIGNS_BASE,
              'max_output_bytes': output_bytes,
              'simulation_timeout': args.runtime_minutes * 60,
              'transfer_timeout': args.transfer_minutes * 60,
              'upload_timeout': args.transfer_minutes * 60,
              'helper_revision': REVISION, 'cpus': cpus, 'input_sha256': pinned}
    if rest is not None:
        config['rest'] = rest
    if recovered_outputs:
        config['preexisting_recovery'] = recovered_outputs
        print(json.dumps({'warning': 'Existing recovery preserved; independent preparation allowed',
                          'preexisting_recovery': recovered_outputs}))
    write_json(root / 'job.json', config)
    # A held job counts toward max_idle and pauses further materialization.
    text = f'''universe = vanilla
executable = {executable}
arguments = run --config job.json --job $(ClusterId).$(ProcId)
initialdir = {root}/jobs/$(ProcId)
output = {root}/logs/$(ProcId).out
error = {root}/logs/$(ProcId).err
log = {root}/events.log
should_transfer_files = YES
when_to_transfer_output = ON_EXIT_OR_EVICT
transfer_input_files = {','.join(map(str, inputs))}
encrypt_input_files = {credentials / 'naf-upload.token'}
transfer_output_files = receipt.json,recovery
request_cpus = {cpus}
request_memory = {args.memory_mb}MB
request_disk = {2 * output_bytes // 1024 + 2 * 1024 ** 2 + sum(Path(item['path']).stat().st_size // 1024 + 1 for item in resources.values())}KB
+RequestRuntime = {(args.runtime_minutes + args.transfer_minutes + 10) * 60}
getenv = False
on_exit_hold = (ExitBySignal == True) || (ExitCode != 0)
on_exit_hold_reason = "Simulation or verified CERNBox upload failed; inspect receipt.json and recovery before release"
max_materialize = {args.max_running}
max_idle = 1
queue {args.jobs}
'''
    (root / 'jobs.sub').write_text(text)
    print(json.dumps({'submit_file': str(root / 'jobs.sub'), 'jobs': args.jobs,
                      'remote_base': CAMPAIGNS_BASE,
                      'max_materialized': args.max_running, 'token_expiry': eos.claims[2]}))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action', required=True)
    r = sub.add_parser('run'); r.add_argument('--config', required=True); r.add_argument('--job', required=True)
    t = sub.add_parser('retry'); t.add_argument('--job-dir', required=True); t.add_argument('--credentials', default='~/.config/iaxo-cernbox')
    t.add_argument('--transfer-minutes', type=int, default=30)
    b = sub.add_parser('prepare'); b.add_argument('--campaign', required=True); b.add_argument('--script', required=True)
    b.add_argument('--run-dir', required=True); b.add_argument('--jobs', type=int, default=1)
    b.add_argument('--credentials', default='~/.config/iaxo-cernbox')
    b.add_argument('--max-running', type=int, default=2)
    budget = b.add_mutually_exclusive_group()
    budget.add_argument('--max-output-gb', type=int, default=4)
    budget.add_argument('--max-output-mb', type=int, help='MiB budget for measured small-output jobs; same monitoring and recovery rules')
    b.add_argument('--memory-mb', type=int, default=2048); b.add_argument('--runtime-minutes', type=int, default=60)
    b.add_argument('--transfer-minutes', type=int, default=30)
    b.add_argument('--cpus', type=int, default=1, help='Reserved CPUs; also bounds common numerical-library threads with --rest-install')
    b.add_argument('--rest-install', help='Enable private worker-local REST state and ROOT/GSL preflight for this install')
    b.add_argument('--gsl-prefix', default='/data/dust/group/iaxo/software/lobis/gsl/gsl-2.7.1/install')
    b.add_argument('--rest-resources', help='JSON mapping relative .rest paths to absolute path/sha256 records; never copies shared GDML caches')
    b.add_argument('--allow-existing-recovery', action='store_true',
                   help='Permit an independent campaign while preserving and recording other recovery folders')
    args = p.parse_args()
    try:
        if args.action == 'run':
            def interrupted(signum, frame):
                raise StageError(f'Worker interrupted by signal {signum}; output retained for review')
            signal.signal(signal.SIGTERM, interrupted)
            signal.signal(signal.SIGINT, interrupted)
            return run_worker(args.config, args.job)
        if args.action == 'retry':
            return retry_upload(args)
        prepare(args)
        return 0
    except (StageError, OSError, ValueError) as e:
        print(str(e), file=sys.stderr)
        return 73


if __name__ == '__main__':
    sys.exit(main())
