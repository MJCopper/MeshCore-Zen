import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('zen_boundary', REPO/'tools/check_zen_baseline.py')
boundary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boundary)

class BaselineExtensionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.sources = {p: (REPO/p).read_bytes() for p in ('src/Dispatcher.h','src/Dispatcher.cpp','src/MeshCore.h')}
        self.manifest = json.loads((REPO/'tools/zen_baseline_extensions.json').read_text())
        for name, content in self.sources.items():
            path = self.root/name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(content)
        (self.root/'tools').mkdir()
        self.write_manifest()
    def tearDown(self):
        self.temp.cleanup()
    def write_manifest(self):
        (self.root/'tools/zen_baseline_extensions.json').write_text(json.dumps(self.manifest))
    def check(self, missing=False):
        def source(command, **kwargs):
            name = command[2].split(':',1)[1]
            content=self.sources[name]
            if name=='src/MeshCore.h': content=content.replace(b'  virtual uint32_t getCurrentTimeUnique() {',b'  uint32_t getCurrentTimeUnique() {')
            return SimpleNamespace(returncode=1 if missing else 0, stdout=content)
        with patch.object(boundary,'ROOT',self.root), patch.object(boundary.subprocess,'run',side_effect=source):
            return boundary.checked_core_extensions()
    def test_exact_sensor_sources_are_accepted(self):
        allowed, errors = self.check()
        self.assertEqual(allowed,set(self.sources)); self.assertEqual(errors,[])
    def test_local_source_drift_is_rejected(self):
        with (self.root/'src/Dispatcher.h').open('ab') as out: out.write(b'\n// unreviewed\n')
        self.assertTrue(self.check()[1])
    def test_changed_pin_digest_is_rejected(self):
        self.manifest['src/Dispatcher.h']['sha256'] = '0'*64
        self.write_manifest(); self.assertTrue(self.check()[1])
    def test_additional_core_exemptions_are_rejected(self):
        self.manifest['src/Mesh.cpp'] = dict(self.manifest['src/Dispatcher.cpp'])
        self.write_manifest(); self.assertTrue(self.check()[1])
    def test_rtc_drift_and_unpinned_rtc_base_are_rejected(self):
        with (self.root/'src/MeshCore.h').open('ab') as out: out.write(b'\n// drift\n')
        self.assertTrue(self.check()[1])
        (self.root/'src/MeshCore.h').write_bytes(self.sources['src/MeshCore.h'])
        self.manifest['src/MeshCore.h']['source_commit']='unreviewed'
        self.write_manifest(); self.assertTrue(self.check()[1])
    def test_unavailable_source_commit_is_rejected(self):
        self.assertTrue(self.check(missing=True)[1])

if __name__ == '__main__':
    unittest.main()
