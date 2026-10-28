import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('zen_radio_presets', ROOT / 'tools/zen_radio_presets.py')
presets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(presets)


class PresetGeneratorTests(unittest.TestCase):
    def setUp(self):
        self.snapshot = json.loads(presets.SNAPSHOT.read_text())
        self.overrides = {'schema_version': 1, 'entries': []}

    def regenerate(self):
        self.snapshot['sha256'] = presets.digest(self.snapshot['suggested_radio_settings'])
        return presets.generate(self.snapshot, self.overrides)

    def test_committed_snapshot_is_deterministic_and_matches_header(self):
        first, rows, excluded = presets.generate(self.snapshot, self.overrides)
        self.assertEqual(first, presets.generate(copy.deepcopy(self.snapshot), self.overrides)[0])
        self.assertEqual(first, presets.OUTPUT.read_text())
        self.assertEqual(21, len(rows))
        self.assertEqual(5, len(excluded))
        narrow = next(row for row in rows if row[0] == 'Australia (Narrow)')
        self.assertEqual(('916.575', '62.5', 7, 7, 0), (str(narrow[1]), str(narrow[2]), *narrow[3:]))
        self.assertEqual(3, next(row for row in rows if row[0] == 'Canada')[-1])
        self.assertEqual(2, next(row for row in rows if row[0] == 'New Zealand (Narrow)')[-1])

    def test_filters_report_all_exclusions_and_preserve_order(self):
        _, rows, excluded = presets.generate(self.snapshot, self.overrides)
        self.assertTrue(all(presets.Decimal(868) <= r[1] <= presets.Decimal(928) for r in rows))
        self.assertTrue(all('deprecated' not in r[0].lower() for r in rows))
        names = [r[0] for r in rows]
        upstream = [e['title'] for e in self.snapshot['suggested_radio_settings']['entries'] if e['title'] in names]
        self.assertEqual(upstream, names)
        self.assertEqual(3, sum('outside' in reason for _, reason in excluded))
        self.assertEqual(2, sum('deprecated' in reason for _, reason in excluded))

    def test_rejects_invalid_or_missing_rf_values(self):
        for field, values in {'frequency': [None, True, 'NaN', 'Infinity', '-1', '2600', 'bad'],
                              'bandwidth': [None, '63', '0'], 'spreading_factor': [None, '4', '13', '7.5'],
                              'coding_rate': [None, '4', '9', '7.5']}.items():
            for value in values:
                with self.subTest(field=field, value=value):
                    row = copy.deepcopy(self.snapshot['suggested_radio_settings']['entries'][0])
                    row[field] = value
                    with self.assertRaises(ValueError):
                        presets.normalize(row)
            row = copy.deepcopy(self.snapshot['suggested_radio_settings']['entries'][0])
            del row[field]
            with self.assertRaises(ValueError):
                presets.normalize(row)

    def test_rejects_conflicts_controls_empty_names_and_unsupported_network_fields(self):
        original = copy.deepcopy(self.snapshot['suggested_radio_settings']['entries'][0])
        for name in ['', ' ', ' Name', 'Name ', 'x\ny', 'x\x00y', 'x' * 97]:
            row = dict(original, title=name)
            with self.assertRaises(ValueError):
                presets.normalize(row)
        for network in [None, [], {'other': 1}, {'path_hash_size': 0}, {'path_hash_size': 4}, {'path_hash_size': '2.5'}]:
            with self.assertRaises(ValueError):
                presets.normalize(dict(original, network_settings=network))
        self.snapshot['suggested_radio_settings']['entries'].append(dict(original, title=original['title'].upper()))
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.regenerate()

    def test_missing_optional_fields_and_unknown_non_network_keys_are_supported(self):
        row = dict(self.snapshot['suggested_radio_settings']['entries'][0])
        row.pop('description', None)
        row['future_info'] = {'unused': True}
        self.assertEqual(0, presets.normalize(row)[-1])
        row['network_settings'] = {}
        self.assertEqual(0, presets.normalize(row)[-1])
        row['network_settings'] = {'path_hash_size': '2'}
        self.assertEqual(2, presets.normalize(row)[-1])

    def test_cpp_strings_are_safe_utf8_and_generation_is_stable(self):
        name = 'Quoted "path" \\ café 🌏'
        self.snapshot['suggested_radio_settings']['entries'][0]['title'] = name
        header, _, _ = self.regenerate()
        literal = presets.cpp_string(name)
        self.assertIn(literal, header)
        self.assertNotIn('\\ud83c', header)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'string.cpp'
            executable = Path(directory) / 'string'
            source.write_text('#include <stdio.h>\nint main(){fputs(' + literal + ',stdout);}\n')
            subprocess.run(['c++', str(source), '-o', str(executable)], check=True)
            self.assertEqual(name.encode('utf-8'), subprocess.check_output([str(executable)]))

    def test_overrides_require_provenance_and_cannot_override_official_names(self):
        row = dict(self.snapshot['suggested_radio_settings']['entries'][0], title='Zen example')
        self.overrides['entries'] = [row]
        with self.assertRaises(ValueError):
            self.regenerate()
        row.update(reason='Documented local addition', source='https://example.org/mesh')
        self.assertEqual('Zen example', self.regenerate()[1][-1][0])
        self.assertIn('Zen-only addition', self.regenerate()[0])
        row['title'] = 'Australia'
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.regenerate()

    def test_snapshot_metadata_and_digest_are_checked(self):
        for key, value in [('sha256', '0' * 64), ('source_url', 'https://example.org'), ('schema_version', 2), ('fetched_at_utc', 'not-a-date')]:
            wrong = dict(self.snapshot, **{key: value})
            with self.assertRaises((ValueError, KeyError, TypeError)):
                presets.generate(wrong, self.overrides)

    def test_offline_check_and_stale_failure_never_fetch_or_modify_files(self):
        with tempfile.TemporaryDirectory() as directory:
            snapshot, overrides, output = [Path(directory) / n for n in ('snapshot.json', 'overrides.json', 'header.h')]
            snapshot.write_text(json.dumps(self.snapshot))
            overrides.write_text(json.dumps(self.overrides))
            argv = ['--snapshot', str(snapshot), '--overrides', str(overrides), '--output', str(output)]
            with mock.patch.object(presets.urllib.request, 'urlopen', side_effect=AssertionError('network forbidden')):
                self.assertEqual(0, presets.main(['--generate'] + argv))
                self.assertEqual(0, presets.main(['--check'] + argv))
                output.write_text('stale')
                self.assertEqual(1, presets.main(['--check'] + argv))
                self.assertEqual('stale', output.read_text())
                output.unlink()
                self.assertEqual(1, presets.main(['--check'] + argv))
                self.assertFalse(output.exists())

    def test_failed_refresh_keeps_previous_snapshot_and_header(self):
        with tempfile.TemporaryDirectory() as directory:
            snapshot, overrides, output = [Path(directory) / n for n in ('snapshot.json', 'overrides.json', 'header.h')]
            snapshot.write_text(json.dumps(self.snapshot)); overrides.write_text(json.dumps(self.overrides)); output.write_text('known-good')
            original = snapshot.read_bytes()
            for payload in [b'not JSON', json.dumps({'config': {'suggested_radio_settings': {'entries': [{}]}}}).encode(), b'X' * (1024 * 1024 + 1)]:
                with self.assertRaises((ValueError, KeyError)):
                    presets.refresh(snapshot, overrides, output, opener=lambda *a, **k: io.BytesIO(payload))
                self.assertEqual(original, snapshot.read_bytes()); self.assertEqual('known-good', output.read_text())
            with self.assertRaises(OSError):
                presets.refresh(snapshot, overrides, output, opener=mock.Mock(side_effect=OSError('timeout')))
            self.assertEqual(original, snapshot.read_bytes()); self.assertEqual('known-good', output.read_text())

    def test_refresh_checks_request_and_is_noop_when_data_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            snapshot, overrides, output = [Path(directory) / n for n in ('snapshot.json', 'overrides.json', 'header.h')]
            snapshot.write_text(json.dumps(self.snapshot, indent=2, ensure_ascii=False) + '\n'); overrides.write_text(json.dumps(self.overrides))
            output.write_text(presets.generate(self.snapshot, self.overrides)[0]); original = snapshot.read_bytes()
            payload = json.dumps({'config': {'suggested_radio_settings': self.snapshot['suggested_radio_settings']}}).encode()
            def opener(request, timeout):
                self.assertEqual(presets.SOURCE_URL, request.full_url)
                self.assertIn('MeshCore-Zen', request.get_header('User-agent'))
                self.assertEqual(20, timeout)
                return io.BytesIO(payload)
            presets.refresh(snapshot, overrides, output, opener=opener)
            self.assertEqual(original, snapshot.read_bytes())
            changed = copy.deepcopy(self.snapshot['suggested_radio_settings'])
            changed['entries'][0]['coding_rate'] = '6'
            presets.refresh(snapshot, overrides, output, opener=lambda *a, **k: io.BytesIO(json.dumps({'config': {'suggested_radio_settings': changed}}).encode()))
            updated = json.loads(snapshot.read_text())
            self.assertEqual(presets.digest(changed), updated['sha256'])
            self.assertEqual(presets.generate(updated, self.overrides)[0], output.read_text())


if __name__ == '__main__':
    unittest.main()
