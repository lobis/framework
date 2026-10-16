"""Exercise real shell workers with in-memory storage; no credentials or physics."""
import argparse
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "batch", Path(__file__).with_name("cernbox_batch.py")
)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class MemoryEOS:
    def __init__(self, *args):
        self.files = {}
        self.claims = {2: 2000000000}

    def metadata(self, name):
        return len(self.files[name]) if name in self.files else None

    def checksum(self, name):
        return "%08x" % (m.zlib.adler32(self.files[name]) & 0xFFFFFFFF)

    def upload(self, source, name):
        self.files[name] = source.read_bytes()


@contextlib.contextmanager
def working_directory(path):
    previous = Path.cwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(previous)


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.install = self.root / "rest"
        (self.install / "bin").mkdir(parents=True)
        # Model the real setup's nounset hazard and REST_HOME reset.
        (self.install / "thisREST.sh").write_text(
            'unused="$UNSET_SETUP_VARIABLE"\nexport REST_HOME=/shared/bad-cache\n'
            + "\n".join(
                "export " + key + "=" + str(self.install)
                for key in (
                    "G4NEUTRONHPDATA",
                    "G4LEDATA",
                    "G4LEVELGAMMADATA",
                    "G4RADIOACTIVEDATA",
                )
            )
            + "\n"
        )
        for binary in ("restRoot", "restG4", "restManager"):
            path = self.install / "bin" / binary
            path.write_text(
                '#!/bin/bash\necho "IAXO_PRIVATE_REST_PREFLIGHT_OK $REST_HOME/.rest"\n'
            )
            path.chmod(0o755)
        self.gsl = self.root / "gsl"
        (self.gsl / "lib").mkdir(parents=True)
        self.script = self.root / "input.sh"
        self.script.write_text('printf "physics" > "$IAXO_OUTPUT_DIR/data.root"\n')
        self.args = argparse.Namespace(
            campaign="test",
            script=str(self.script),
            run_dir=str(self.root / "prepared"),
            jobs=1,
            max_running=1,
            max_output_gb=1,
            memory_mb=1024,
            runtime_minutes=1,
            transfer_minutes=1,
            credentials=str(self.root / "credentials"),
            cpus=2,
            rest_install=str(self.install),
            gsl_prefix=str(self.gsl),
            rest_resources=None,
        )

    def prepare(self):
        with patch.object(m, "EOS", MemoryEOS), contextlib.redirect_stdout(
            io.StringIO()
        ):
            m.prepare(self.args)
        return Path(self.args.run_dir)

    def sandbox(self, prepared):
        sandbox = self.root / "sandbox"
        sandbox.mkdir()
        for name in (
            "simulation.sh",
            "rest-bootstrap.sh",
            "cernbox_batch.py",
            "job.json",
        ):
            shutil.copyfile(prepared / name, sandbox / name)
        if (prepared / "rest-resources").exists():
            shutil.copytree(prepared / "rest-resources", sandbox / "rest-resources")
        return sandbox

    def run_worker(self, sandbox):
        eos = MemoryEOS()
        with working_directory(sandbox), patch.object(
            m, "EOS", return_value=eos
        ), patch.dict(
            os.environ, {"_CONDOR_SCRATCH_DIR": str(sandbox)}
        ), contextlib.redirect_stdout(
            io.StringIO()
        ), contextlib.redirect_stderr(
            io.StringIO()
        ):
            status = m.run_worker("job.json", "123.0")
        return status, eos, json.loads((sandbox / "receipt.json").read_text())

    def test_private_setup_survives_rest_reset_and_uploads_provenance(self):
        self.script.write_text(
            """set -euo pipefail
test "$HOME" = "$REST_HOME"
test -d "$REST_HOME/.rest/gdml"
test -w "$REST_HOME/.rest/runNumber"
test "$OMP_NUM_THREADS" = 2
printf '%s' "$REST_HOME" > "$IAXO_OUTPUT_DIR/data.root"
"""
        )
        sandbox = self.sandbox(self.prepare())
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 0)
        self.assertEqual(receipt["state"], "complete")
        self.assertEqual(list((sandbox / "recovery").iterdir()), [])
        prefix = m.CAMPAIGNS_BASE + "test/123.0/"
        self.assertTrue(
            eos.files[prefix + "data.root"].decode().startswith(str(sandbox))
        )
        self.assertIn(prefix + "_batch/prepared-job.json", eos.files)
        self.assertFalse(any(".worker-runtime" in name for name in eos.files))

    def test_preflight_missing_success_marker_prevents_simulation(self):
        (self.install / "bin/restRoot").write_text(
            '#!/bin/bash\necho "Interpreter error"\nexit 0\n'
        )
        sandbox = self.sandbox(self.prepare())
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 73)
        self.assertFalse(receipt["simulation_complete"])
        self.assertFalse((sandbox / "recovery/data.root").exists())
        self.assertTrue((sandbox / "recovery/.worker-runtime").is_dir())
        self.assertFalse(eos.files)

    def test_runtime_state_cannot_count_as_simulation_data(self):
        self.script.write_text("exit 0\n")
        sandbox = self.sandbox(self.prepare())
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 73)
        self.assertIn("no data files", receipt["error"])
        self.assertFalse(eos.files)

    def test_tampered_script_refused_before_execution(self):
        sandbox = self.sandbox(self.prepare())
        (sandbox / "simulation.sh").write_text("touch executed\n")
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 73)
        self.assertIn("Changed prepared input", receipt["error"])
        self.assertFalse((sandbox / "executed").exists())

    def test_evicted_incomplete_job_never_reruns(self):
        sandbox = self.sandbox(self.prepare())
        (sandbox / "recovery").mkdir()
        partial = sandbox / "recovery/partial.root"
        partial.write_bytes(b"partial physics")
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 73)
        self.assertIn("Recovered incomplete", receipt["error"])
        self.assertEqual(partial.read_bytes(), b"partial physics")
        self.assertFalse(eos.files)

    def test_completed_upload_recovery_never_reruns_source(self):
        sandbox = self.sandbox(self.prepare())
        (sandbox / "simulation.sh").write_text("touch executed\nexit 99\n")
        (sandbox / "recovery").mkdir()
        (sandbox / "recovery/data.root").write_bytes(b"completed physics")
        (sandbox / "receipt.json").write_text(
            json.dumps(
                dict(
                    campaign="test",
                    job="123.0",
                    remote_base=m.CAMPAIGNS_BASE,
                    state="uploading",
                    simulation_complete=True,
                    files=[],
                )
            )
        )
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 0)
        self.assertFalse((sandbox / "executed").exists())
        self.assertEqual(receipt["state"], "complete")

    def test_prepared_helper_is_independent_of_installed_helper(self):
        prepared = self.prepare()
        submit = (prepared / "jobs.sub").read_text()
        self.assertIn("executable = " + str(prepared / "cernbox_batch.py"), submit)
        self.assertIn("request_cpus = 2", submit)
        self.assertEqual(
            (prepared / "cernbox_batch.py").read_bytes(), Path(m.__file__).read_bytes()
        )

    def test_changed_installation_prevents_physics(self):
        sandbox = self.sandbox(self.prepare())
        (self.install / "bin/restG4").write_text("#!/bin/bash\nexit 0\n")
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 73)
        self.assertIn("installation changed", receipt["error"])
        self.assertFalse(receipt["simulation_complete"])
        self.assertFalse((sandbox / "recovery/data.root").exists())
        self.assertFalse(eos.files)

    def test_added_or_removed_installation_files_are_detected(self):
        prepared = self.prepare()
        config = json.loads((prepared / "job.json").read_text())
        expected = config["rest"]["installation_files"]
        extra = self.install / "bin/unexpected-library"
        extra.write_text("new code")
        self.assertNotEqual(m.rest_installation_files(self.install), expected)
        extra.unlink()
        (self.install / "bin/restRoot").unlink()
        self.assertNotEqual(m.rest_installation_files(self.install), expected)

    def test_credential_expiring_during_queue_wait_prevents_physics(self):
        sandbox = self.sandbox(self.prepare())
        with patch.object(m.time, "time", return_value=1999999900):
            status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 73)
        self.assertIn("credential cannot outlast", receipt["error"])
        self.assertFalse(receipt["simulation_complete"])
        self.assertFalse((sandbox / "recovery/data.root").exists())
        self.assertFalse(eos.files)

    def test_short_credential_refuses_preparation_without_partial_campaign(self):
        with patch.object(m.time, "time", return_value=1999999900):
            with self.assertRaises(m.StageError):
                self.prepare()
        self.assertFalse(Path(self.args.run_dir).exists())

    def test_small_output_budget_and_scratch_reservation(self):
        self.args.max_output_mb = 48
        prepared = self.prepare()
        config = json.loads((prepared / "job.json").read_text())
        self.assertEqual(config["max_output_bytes"], 48 * 1024**2)
        self.assertIn("request_disk = 2195456KB", (prepared / "jobs.sub").read_text())

    def test_monitor_failure_stops_worker_process_group(self):
        self.script.write_text("sleep 60\n")
        sandbox = self.sandbox(self.prepare())
        processes = []
        popen = subprocess.Popen

        def capture(*args, **kwargs):
            process = popen(*args, **kwargs)
            processes.append(process)
            return process

        with patch.object(m.subprocess, "Popen", side_effect=capture), patch.object(
            m, "output_size", side_effect=m.StageError("Quota monitor failed")
        ):
            status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 73)
        self.assertEqual(len(processes), 1)
        self.assertIsNotNone(processes[0].poll())
        self.assertIn("Quota monitor failed", receipt["error"])

    def test_invalid_script_and_submit_path_leave_no_campaign(self):
        self.script.write_text("if then\n")
        with self.assertRaises(m.StageError):
            self.prepare()
        self.assertFalse(Path(self.args.run_dir).exists())
        self.script.write_text("exit 0\n")
        self.args.run_dir = str(self.root / "bad$(ClusterId)")
        with self.assertRaises(m.StageError):
            self.prepare()
        self.assertFalse(Path(self.args.run_dir).exists())

    def test_pinned_resource_copy_survives_original_removal(self):
        resource = self.root / "gas.dat"
        resource.write_bytes(b"pinned gas resource")
        manifest = self.root / "resources.json"
        manifest.write_text(
            json.dumps(
                {
                    "download/gas.dat": dict(
                        path=str(resource), sha256=m.checksums(resource)[0]["sha256"]
                    )
                }
            )
        )
        self.args.rest_resources = str(manifest)
        prepared = self.prepare()
        resource.unlink()
        self.assertEqual(
            (prepared / "rest-resources/download/gas.dat").read_bytes(),
            b"pinned gas resource",
        )
        sandbox = self.sandbox(prepared)
        status, eos, receipt = self.run_worker(sandbox)
        self.assertEqual(status, 0)
        self.assertEqual(
            eos.files[
                m.CAMPAIGNS_BASE + "test/123.0/_batch/rest-resources/download/gas.dat"
            ],
            b"pinned gas resource",
        )

    def test_changed_or_unsafe_resource_rejected_before_preparation(self):
        resource = self.root / "resource"
        resource.write_text("new contents")
        manifest = self.root / "resources.json"
        manifest.write_text(
            json.dumps({"runNumber": dict(path=str(resource), sha256="0" * 64)})
        )
        self.args.rest_resources = str(manifest)
        with self.assertRaises(m.StageError):
            self.prepare()
        self.assertFalse(Path(self.args.run_dir).exists())
        for relative in (
            "../escape",
            "/absolute",
            "gdml/shared.gdml",
            "AddonDict/lib.so",
            "a/../b",
        ):
            with self.subTest(relative=relative), self.assertRaises(m.StageError):
                m.safe_resource(relative)

    def test_worker_private_homes_are_distinct(self):
        config = dict(rest=dict(install="unused", gsl_prefix="unused"), cpus=1)
        envs = []
        for name in ("one", "two"):
            out = self.root / name
            out.mkdir()
            env = {}
            m.rest_environment(self.root, out, config, env)
            envs.append(env)
        self.assertNotEqual(envs[0]["REST_HOME"], envs[1]["REST_HOME"])
        Path(envs[0]["REST_HOME"], ".rest/gdml/PID123.gdml").write_text("one")
        self.assertFalse(Path(envs[1]["REST_HOME"], ".rest/gdml/PID123.gdml").exists())

    def test_network_filesystem_rejected_even_through_local_mount_name(self):
        with patch.dict(
            os.environ, {"_CONDOR_SCRATCH_DIR": str(self.root)}, clear=True
        ), patch.object(m, "filesystem_type", return_value="nfs4"):
            with self.assertRaises(m.StageError):
                m.scratch_root()

    def test_upload_operations_share_remaining_time_budget(self):
        eos = object.__new__(m.EOS)
        eos.timeout = 100
        eos.deadline = 50
        eos.env = {}
        eos.token = "fake-token"
        with patch.object(m.time, "monotonic", return_value=47), patch.object(
            m.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")
        ) as run:
            eos.execute(["test"])
            self.assertEqual(run.call_args.kwargs["timeout"], 3)
        with patch.object(m.time, "monotonic", return_value=51), patch.object(
            m.subprocess, "run"
        ) as run:
            with self.assertRaises(m.StageError):
                eos.execute(["test"])
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
