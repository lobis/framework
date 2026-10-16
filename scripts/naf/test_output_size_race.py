import importlib.util, tempfile, threading, shutil, time, unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "helper", Path(__file__).with_name("cernbox_batch.py")
)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class CacheRaceTests(unittest.TestCase):
    def test_real_cache_churn_preserves_stable_payload_count(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            payload = b"x" * 131072
            (root / "transport.root").write_bytes(payload)
            stop = threading.Event()
            errors = []

            def churn():
                try:
                    for i in range(1500):
                        if stop.is_set():
                            break
                        cache = root / "rest-home"
                        cache.mkdir()
                        for j in range(8):
                            (cache / str(j)).write_bytes(b"a" * 512)
                        shutil.rmtree(cache)
                except BaseException as e:
                    errors.append(e)

            t = threading.Thread(target=churn)
            t.start()
            try:
                for _ in range(1500):
                    self.assertGreaterEqual(m.output_size(root), len(payload))
            finally:
                stop.set()
                t.join()
            self.assertFalse(errors)

    def test_missing_root_refused(self):
        with self.assertRaises(m.StageError):
            m.output_size(Path("/a/missing/worker/output/root"))

    def test_io_error_not_suppressed(self):
        with tempfile.TemporaryDirectory() as d:

            def fail(*args, **kwargs):
                kwargs["onerror"](PermissionError("denied"))
                return iter(())

            with patch.object(m.os, "walk", side_effect=fail):
                with self.assertRaises(PermissionError):
                    m.output_size(Path(d))


if __name__ == "__main__":
    unittest.main()
