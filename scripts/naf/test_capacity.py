import unittest
import check_capacity as capacity


class CapacityTests(unittest.TestCase):
    def test_foreign_factory_charged_at_possible_concurrency(self):
        jobs = [dict(ClusterId=1, JobStatus=2, Iwd='/registered', Cmd='worker')]
        factories = [dict(ClusterId=1, JobMaterializeLimit=500, JobMaterializeMaxIdle=500)]
        report = capacity.reservations(jobs, factories, lambda *_: 48 * 1024 ** 2)
        self.assertEqual(report[0]['reserved_bytes'], 500 * 96 * 1024 ** 2)

    def test_paused_factory_still_charges_live_jobs(self):
        jobs = [dict(ClusterId=1, JobStatus=2, Iwd='/registered', Cmd='worker')]
        factories = [dict(ClusterId=1, JobMaterializeLimit=0, JobMaterializeMaxIdle=0)]
        self.assertEqual(capacity.reservations(jobs, factories, lambda *_: 100)[0]['reserved_bytes'], 200)

    def test_unbounded_factory_and_unknown_dust_writer_refused(self):
        with self.assertRaises(ValueError):
            capacity.reservations([], [dict(ClusterId=1, JobMaterializeLimit=0,
                                           JobMaterializeMaxIdle=1)], lambda *_: 1)
        with self.assertRaises(ValueError):
            capacity.budget_lookup('/data/dust/user/lobis/unknown', 'worker')

    def test_uses_desy_quota_units_and_rejects_missing_observation(self):
        self.assertEqual(capacity.quota_free('user.af-axion.lobis 0.288 0.5 57.6'), 212000000000)
        with self.assertRaises(ValueError):
            capacity.quota_free('filesystem df output')


if __name__ == '__main__':
    unittest.main()
