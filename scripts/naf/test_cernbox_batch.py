import importlib.util
import argparse
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('stage',Path(__file__).with_name('cernbox_batch.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class FakeEOS:
    def __init__(self,source,wrong_size=False,wrong_checksum=False,existing=False):
        self.sums,_=m.checksums(source);self.present=existing;self.wrong_size=wrong_size;self.wrong_checksum=wrong_checksum;self.uploads=0
    def metadata(self,path):return self.sums['bytes']+(1 if self.wrong_size else 0) if self.present else None
    def checksum(self,path):return '00000000' if self.wrong_checksum else self.sums['adler32']
    def upload(self,source,path):self.present=True;self.uploads+=1

class IntegrityTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.src=Path(self.tmp.name)/'output.root';self.src.write_bytes(b'closed simulation file'*100)
        self.remote=m.BASE+'test/1/output.root'
    def test_verified_upload_removes_source_after_receipt(self):
        eos=FakeEOS(self.src);seen=[]
        m.stage_file(eos,self.src,self.remote,lambda r:seen.append((r,self.src.exists())),remove=True,attempts=1)
        self.assertFalse(self.src.exists());self.assertTrue(seen[0][1]);self.assertEqual(eos.uploads,1)
    def test_checksum_mismatch_keeps_source(self):
        with self.assertRaises(m.StageError):m.stage_file(FakeEOS(self.src,wrong_checksum=True),self.src,self.remote,lambda r:self.fail(),remove=True,attempts=1)
        self.assertTrue(self.src.exists())
    def test_size_mismatch_keeps_source(self):
        with self.assertRaises(m.StageError):m.stage_file(FakeEOS(self.src,wrong_size=True),self.src,self.remote,lambda r:self.fail(),remove=True,attempts=1)
        self.assertTrue(self.src.exists())
    def test_matching_existing_destination_skips_upload(self):
        eos=FakeEOS(self.src,existing=True);m.stage_file(eos,self.src,self.remote,lambda r:None,remove=True,attempts=1)
        self.assertEqual(eos.uploads,0);self.assertFalse(self.src.exists())
    def test_receipt_failure_keeps_source(self):
        def fail(r):raise OSError('cannot persist receipt')
        with self.assertRaises(OSError):m.stage_file(FakeEOS(self.src),self.src,self.remote,fail,remove=True,attempts=1)
        self.assertTrue(self.src.exists())
    def test_source_changed_before_cleanup_is_retained(self):
        eos=FakeEOS(self.src)
        def change(r):self.src.write_bytes(b'new contents')
        with self.assertRaises(m.StageError):m.stage_file(eos,self.src,self.remote,change,remove=True,attempts=1)
        self.assertEqual(self.src.read_bytes(),b'new contents')
    def test_symlink_rejected(self):
        link=Path(self.tmp.name)/'link';link.symlink_to(self.src)
        with self.assertRaises(m.StageError):m.stage_file(FakeEOS(self.src),link,self.remote,lambda r:None,remove=True,attempts=1)
        self.assertTrue(self.src.exists())
    def test_invalid_campaign_rejected(self):
        for name in ['../other','name/child','.', 'bad name']:
            with self.assertRaises(m.StageError):m.safe_name(name)
    def test_incomplete_simulation_cannot_be_retried_as_final_output(self):
        import argparse,json
        d=Path(self.tmp.name)
        (d/'receipt.json').write_text(json.dumps({'simulation_complete':False}))
        with self.assertRaises(m.StageError):m.retry_upload(argparse.Namespace(job_dir=str(d)))
    def test_native_credential_is_redacted_from_diagnostics(self):
        import subprocess
        eos=object.__new__(m.EOS);eos.token='zteos64:FAKE_NOT_A_REAL_TOKEN';eos.env={};eos.timeout=1
        output=subprocess.CompletedProcess(['test'],1,eos.token,'authz=zteos64:FAKE_NOT_A_REAL_TOKEN')
        with patch.object(m.subprocess,'run',return_value=output):
            rc,text=eos.execute(['test'])
        self.assertNotIn(eos.token,text);self.assertIn('[TOKEN REDACTED]',text)
    def test_no_dust_fallback_as_scratch(self):
        with patch.dict('os.environ',{'_CONDOR_SCRATCH_DIR':'/data/dust/user/lobis','TMPDIR':'/afs/desy.de/user/l/lobis'},clear=True):
            with self.assertRaises(m.StageError):m.scratch_root()

class CampaignPathTests(unittest.TestCase):
    def test_explicit_recovery_override_preserves_other_output_and_records_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'other/jobs/7/recovery/transport.root'
            source.parent.mkdir(parents=True);source.write_bytes(b'preserve failed transport')
            script=root/'simulation.sh';script.write_text('exit 0\n')
            args=argparse.Namespace(campaign='new',script=str(script),run_dir=str(root/'new'),jobs=1,
                max_running=1,max_output_gb=1,memory_mb=1024,runtime_minutes=1,transfer_minutes=1,
                credentials=str(root/'credentials'),allow_existing_recovery=False)
            with patch.object(m,'EOS') as cls,contextlib.redirect_stdout(io.StringIO()):
                cls.return_value.claims={2:2000000000}
                with self.assertRaises(m.StageError):m.prepare(args)
                self.assertFalse((root/'new').exists())
                args.allow_existing_recovery=True;m.prepare(args)
                with self.assertRaises(m.StageError):m.prepare(args)
            self.assertEqual(source.read_bytes(),b'preserve failed transport')
            config=json.loads((root/'new/job.json').read_text())
            self.assertEqual(config['preexisting_recovery'][0]['bytes'],len(source.read_bytes()))

    def test_new_preparations_use_campaigns_base(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); script=root/'simulation.sh'; script.write_text('exit 0\n')
            args=argparse.Namespace(campaign='test',script=str(script),run_dir=str(root/'run'),
                jobs=1,max_running=1,max_output_gb=1,memory_mb=1024,runtime_minutes=1,
                transfer_minutes=1,credentials=str(root/'credentials'))
            with patch.object(m,'EOS') as cls,contextlib.redirect_stdout(io.StringIO()):
                cls.return_value.claims={2:2000000000}
                m.prepare(args)
            config=json.loads((root/'run/job.json').read_text())
            self.assertEqual(config['remote_base'],m.BASE+'campaigns/')

    def test_new_receipt_resolves_campaign_subdirectory(self):
        self.assertEqual(m.remote_prefix(dict(campaign='shield',job='123.0',remote_base=m.CAMPAIGNS_BASE)),
                         m.BASE+'campaigns/shield/123.0/')

    def test_legacy_receipt_retains_original_destination(self):
        self.assertEqual(m.remote_prefix(dict(campaign='shield',job='123.0')),
                         m.BASE+'shield/123.0/')

    def test_unknown_bases_rejected(self):
        for base in [m.BASE+'tmp/',m.BASE+'campaigns/../', '/other/', None]:
            with self.assertRaises(m.StageError):m.remote_base(dict(remote_base=base))

    def test_prefix_change_fails_before_unlink(self):
        with tempfile.TemporaryDirectory() as tmp:
            recovery=Path(tmp);p=recovery/'data.root';p.write_bytes(b'preserve')
            r=dict(campaign='shield',job='123.0',remote_base=m.CAMPAIGNS_BASE,
                   files=[dict(remote=m.BASE+'shield/123.0/data.root')])
            with self.assertRaises(m.StageError):m.finish_upload(None,recovery,r,lambda:None,lambda r:None)
            self.assertEqual(p.read_bytes(),b'preserve')

    def test_finish_upload_and_manifest_use_same_base(self):
        class MemoryEOS:
            def __init__(self):self.files={}
            def metadata(self,p):return len(self.files[p]) if p in self.files else None
            def checksum(self,p):return '%08x'%(m.zlib.adler32(self.files[p])&0xffffffff)
            def upload(self,source,p):self.files[p]=source.read_bytes()
        for base in [m.BASE,m.CAMPAIGNS_BASE]:
            with self.subTest(base=base),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp); (root/'data.root').write_bytes(b'closed')
                receipt=dict(campaign='shield',job='123.0',files=[],remote_base=base)
                eos=MemoryEOS()
                with contextlib.redirect_stdout(io.StringIO()):
                    m.finish_upload(eos,root,receipt,lambda:None,receipt['files'].append)
                prefix=base+'shield/123.0/'
                self.assertEqual(set(eos.files),{prefix+'data.root',prefix+'_manifest.json'})
                manifest=json.loads(eos.files[prefix+'_manifest.json'])
                self.assertEqual(manifest['remote_base'],base)
                self.assertEqual(receipt['state'],'complete')
                self.assertFalse(list(root.rglob('*')))


if __name__=='__main__':unittest.main()
