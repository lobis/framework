#!/usr/bin/env python3
"""Read-only aggregate admission check; does not submit, reserve, or edit jobs."""
import argparse
import datetime
import json
from pathlib import Path
import re
import subprocess

import cernbox_batch as batch

GIB = 1024 ** 3
LIVE = {1, 2, 6, 7}
RUN_PARENT = Path('/data/dust/user/lobis/cernbox-batch')


def reservations(jobs, factories, budget_lookup):
    groups = {}
    for job in jobs:
        groups.setdefault(job['ClusterId'], []).append(job)
    ads = {item['ClusterId']: item for item in factories}
    result = []
    for cluster in sorted(set(groups) | set(ads)):
        rows = groups.get(cluster, [])
        factory = ads.get(cluster, {})
        live = sum(item['JobStatus'] in LIVE for item in rows)
        held = sum(item['JobStatus'] == 5 for item in rows)
        possible = live
        idle_limit = factory.get('JobMaterializeMaxIdle', 0)
        if idle_limit > held:
            limit = factory.get('JobMaterializeLimit', 0)
            if limit <= 0:
                raise ValueError('Unbounded foreign factory: ' + str(cluster))
            possible = max(live, limit - held)
        if not possible:
            continue
        working = factory.get('Iwd') or rows[0].get('Iwd')
        command = factory.get('Cmd') or (rows[0].get('Cmd') if rows else '')
        budget = budget_lookup(working, command)
        if budget is None:
            continue
        if type(budget) is not int or budget <= 0:
            raise ValueError('Unknown foreign output budget: ' + str(cluster))
        result.append(dict(cluster=cluster, possible_jobs=possible, live_jobs=live,
                           output_budget_bytes=budget, reserved_bytes=2 * possible * budget))
    return result


def quota_free(text):
    match = re.search(r'^user\.af-axion\.lobis\s+([0-9.]+)\s+([0-9.]+)', text, re.M)
    if not match:
        raise ValueError('Actual DESY user quota is unavailable')
    used, limit = map(float, match.groups())
    if used < 0 or limit <= 0 or used > limit:
        raise ValueError('Invalid or exhausted DESY user quota')
    return int((limit - used) * 1e12)


def budget_lookup(working, command):
    if not working:
        raise ValueError('Missing foreign working directory')
    path = Path(working)
    if path.is_relative_to(RUN_PARENT):
        for candidate in (path, *path.parents):
            if candidate == RUN_PARENT:
                break
            config = candidate / 'job.json'
            if config.is_file():
                return json.loads(config.read_text())['max_output_bytes']
        raise ValueError('Unregistered CERNBox working directory: ' + working)
    # Other active DUST writers must be registered or separately accounted for.
    if path.is_relative_to('/data/dust/user/lobis') or path.is_relative_to('/data/dust/group/iaxo/scratch/lobis'):
        raise ValueError('Unregistered active DUST writer: ' + working)
    if command and 'cernbox_batch' in command:
        raise ValueError('CERNBox factory outside the common reservation parent: ' + working)
    return None


def command(args):
    return subprocess.check_output(args, text=True, timeout=45)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, required=True)
    parser.add_argument('--output-mb', type=int, required=True)
    parser.add_argument('--report', required=True)
    parser.add_argument('--credentials', default='/afs/desy.de/user/l/lobis/.config/iaxo-cernbox')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8 or args.output_mb <= 0:
        parser.error('Generic admission permits 1..8 materialized jobs and a positive output budget')
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), admitted=False,
                  read_only=True, storage_reserved=False, jobs=args.jobs, output_mb=args.output_mb)
    try:
        attrs = 'ClusterId,ProcId,JobStatus,Iwd,Cmd'
        jobs = json.loads(command(['/usr/bin/condor_q', '-json', '-attributes', attrs]))
        attrs = 'ClusterId,JobMaterializeLimit,JobMaterializeMaxIdle,Iwd,Cmd'
        factories = json.loads(command(['/usr/bin/condor_q', '-factory', '-json', '-attributes', attrs]))
        foreign = reservations(jobs, factories, budget_lookup)
        quota = command(['my-dust-quota', '-g'])
        free = quota_free(quota)
        credentials = Path(args.credentials)
        eos = batch.EOS(credentials / 'naf-upload.token', credentials / 'cern-ca-bundle.pem', 90)
        status, space = eos.execute(['xrdfs', batch.HOST, 'query', 'space', eos.opaque(batch.BASE)])
        match = re.search(r'oss\.free=(\d+)', space)
        if status or not match:
            raise ValueError('Actual CERNBox space is unavailable')
        cernbox_free = int(match.group(1))
        wanted = args.jobs * 2 * args.output_mb * 1024 ** 2
        foreign_bytes = sum(item['reserved_bytes'] for item in foreign)
        required = wanted + foreign_bytes + 28 * GIB
        report.update(quota_observation=quota, free_dust_bytes=free, free_cernbox_bytes=cernbox_free,
                      foreign_factories=foreign, foreign_reserved_bytes=foreign_bytes,
                      requested_recovery_bytes=wanted, required_free_dust_bytes=required,
                      dust_floor_bytes=20 * GIB, other_activity_reserve_bytes=8 * GIB)
        if free < required or cernbox_free < max(150 * GIB, wanted):
            raise ValueError('Insufficient aggregate quota headroom')
        report['admitted'] = True
    except Exception as error:
        report['error'] = str(error)
    Path(args.report).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    return 0 if report['admitted'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
