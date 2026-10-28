#!/usr/bin/env python3
"""Refresh reviewed upstream data explicitly; generate/check firmware offline."""
import argparse
from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SOURCE_URL = 'https://api.meshcore.nz/api/v1/config'
SNAPSHOT = ROOT / 'data/zen_radio_presets.json'
OVERRIDES = ROOT / 'data/zen_radio_preset_overrides.json'
OUTPUT = ROOT / 'examples/companion_radio/zen-overlay/app/RadioPresets.generated.h'
BANDWIDTHS = {Decimal(x) for x in ('7.8','10.4','15.6','20.8','31.25','41.7','62.5','125','250','500')}


def digest(payload):
    raw = json.dumps(payload, sort_keys=True, ensure_ascii=False, allow_nan=False, separators=(',', ':')).encode('utf-8')
    return hashlib.sha256(raw).hexdigest()


def number(value, field):
    if isinstance(value, bool) or not isinstance(value, (str, int, float)):
        raise ValueError(f'{field}: expected a number')
    try:
        result = Decimal(str(value))
    except InvalidOperation as exc:
        raise ValueError(f'{field}: invalid number') from exc
    if not result.is_finite():
        raise ValueError(f'{field}: non-finite number')
    return result


def integer(value, field, minimum, maximum):
    result = number(value, field)
    if result != int(result) or not minimum <= result <= maximum:
        raise ValueError(f'{field}: expected integer {minimum}..{maximum}')
    return int(result)


def normalize(entry):
    if not isinstance(entry, dict):
        raise ValueError('preset must be an object')
    title = entry.get('title')
    if not isinstance(title, str) or not title.strip() or title != title.strip():
        raise ValueError('preset title must be nonempty, with no surrounding whitespace')
    if any(ord(c) < 32 or ord(c) == 127 for c in title) or len(title.encode('utf-8')) > 96:
        raise ValueError('preset title contains controls or exceeds 96 UTF-8 bytes')
    frequency = number(entry.get('frequency'), f'{title}: frequency')
    if not Decimal('150') <= frequency <= Decimal('2500'):
        raise ValueError(f'{title}: unsupported frequency')
    bandwidth = number(entry.get('bandwidth'), f'{title}: bandwidth')
    if bandwidth not in BANDWIDTHS:
        raise ValueError(f'{title}: unsupported bandwidth')
    sf = integer(entry.get('spreading_factor'), f'{title}: SF', 5, 12)
    cr = integer(entry.get('coding_rate'), f'{title}: CR', 5, 8)
    network = entry.get('network_settings', {})
    if not isinstance(network, dict) or set(network) - {'path_hash_size'}:
        raise ValueError(f'{title}: unsupported network settings; review required')
    width = integer(network['path_hash_size'], f'{title}: path_hash_size', 1, 3) if 'path_hash_size' in network else 0
    return (title, frequency, bandwidth, sf, cr, width)


def catalogue(payload, overrides):
    if not isinstance(payload, dict) or not isinstance(payload.get('entries'), list) or not payload['entries']:
        raise ValueError('suggested_radio_settings.entries must be a nonempty list')
    if not isinstance(overrides, dict) or overrides.get('schema_version') != 1 or not isinstance(overrides.get('entries'), list):
        raise ValueError('invalid overrides file')
    source = list(payload['entries'])
    for entry in overrides['entries']:
        if not isinstance(entry, dict) or any(not isinstance(entry.get(key), str) or not entry[key].strip() for key in ('reason', 'source')):
            raise ValueError('each Zen addition requires reason and source')
        source.append(entry)
    included, excluded, seen = [], [], set()
    for entry in source:
        row = normalize(entry)
        key = row[0].casefold()
        if key in seen:
            raise ValueError(f'duplicate/conflicting preset name: {row[0]}')
        seen.add(key)
        reason = 'deprecated upstream entry' if 'deprecated' in key else 'outside Zen 868–928 MHz policy' if not Decimal('868') <= row[1] <= Decimal('928') else None
        if reason:
            excluded.append((row[0], reason))
        else:
            included.append(row)
    if not included or len(included) > 255:
        raise ValueError('firmware catalogue must contain 1..255 presets')
    return included, excluded


def validate_snapshot(snapshot):
    if not isinstance(snapshot, dict) or snapshot.get('schema_version') != 1 or snapshot.get('source_url') != SOURCE_URL:
        raise ValueError('invalid snapshot metadata/source')
    stamp = datetime.fromisoformat(snapshot['fetched_at_utc'].replace('Z', '+00:00'))
    if stamp.utcoffset() != timezone.utc.utcoffset(stamp):
        raise ValueError('snapshot timestamp must be UTC')
    payload = snapshot['suggested_radio_settings']
    if snapshot.get('sha256') != digest(payload):
        raise ValueError('snapshot SHA-256 mismatch')
    return payload


def literal(value):
    result = format(value.normalize(), 'f')
    return result + ('.0' if '.' not in result else '') + 'f'


def cpp_string(text):
    # Octal UTF-8 bytes avoid JSON's UTF-16 surrogate escapes and C++ hex greed.
    pieces = ['"']
    for byte in text.encode('utf-8'):
        if byte == 34:
            pieces.append('\\"')
        elif byte == 92:
            pieces.append('\\\\')
        elif 32 <= byte < 127:
            pieces.append(chr(byte))
        else:
            pieces.append('\\%03o' % byte)
    return ''.join(pieces) + '"'


def generate(snapshot, overrides):
    rows, exclusions = catalogue(validate_snapshot(snapshot), overrides)
    lines = ['#pragma once', '// Generated by tools/zen_radio_presets.py; do not edit.',
             f'// Source: {SOURCE_URL}', f'// Snapshot SHA-256: {snapshot["sha256"]}',
             '// Policy: 868–928 MHz; deprecated entries excluded.']
    lines += [f'// Excluded: {name} — {reason}' for name, reason in exclusions]
    lines += ['static constexpr RadioPreset RADIO_PRESETS[] = {']
    zen_names = {entry["title"] for entry in overrides["entries"]}
    for title, freq, bw, sf, cr, width in rows:
        if title in zen_names:
            lines.append("  // Zen-only addition; provenance in data/zen_radio_preset_overrides.json.")
        name = cpp_string(title)
        lines.append(f'  {{{name}, {literal(freq)}, {literal(bw)}, {sf}, {cr}, {width}}},')
    lines += ['};', 'static constexpr int RADIO_PRESET_COUNT = sizeof(RADIO_PRESETS) / sizeof(RADIO_PRESETS[0]);', '']
    return '\n'.join(lines), rows, exclusions


def atomic_write(path, text):
    path = Path(path)
    if path.exists() and path.read_text(encoding='utf-8') == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', newline='\n', dir=path.parent, delete=False) as file:
            name = file.name
            file.write(text)
        os.replace(name, path)
    finally:
        if name and os.path.exists(name):
            os.unlink(name)


def refresh(snapshot_path, overrides_path, output_path, opener=None):
    request = urllib.request.Request(SOURCE_URL, headers={'User-Agent': 'MeshCore-Zen-presets/1 (https://github.com/MJCopper/MeshCore-Zen)'})
    opener = opener or urllib.request.urlopen
    with opener(request, timeout=20) as response:
        raw = response.read(1024 * 1024 + 1)
    if len(raw) > 1024 * 1024:
        raise ValueError('API response exceeds 1 MiB')
    payload = json.loads(raw.decode('utf-8'))['config']['suggested_radio_settings']
    snapshot = {'schema_version': 1, 'source_url': SOURCE_URL,
                'fetched_at_utc': datetime.now(timezone.utc).isoformat(timespec='seconds').replace('+00:00', 'Z'),
                'sha256': digest(payload), 'suggested_radio_settings': payload}
    if Path(snapshot_path).exists():
        old = json.loads(Path(snapshot_path).read_text(encoding='utf-8'))
        if old.get('sha256') == snapshot['sha256'] and old.get('source_url') == SOURCE_URL:
            snapshot = old  # Avoid timestamp-only diffs when upstream is unchanged.
    overrides = json.loads(Path(overrides_path).read_text(encoding='utf-8'))
    header, rows, exclusions = generate(snapshot, overrides)  # Validate before replacing either file.
    atomic_write(snapshot_path, json.dumps(snapshot, indent=2, ensure_ascii=False) + '\n')
    atomic_write(output_path, header)
    return rows, exclusions


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--check', action='store_true')
    action.add_argument('--generate', action='store_true')
    action.add_argument('--refresh', action='store_true')
    parser.add_argument('--snapshot', type=Path, default=SNAPSHOT)
    parser.add_argument('--overrides', type=Path, default=OVERRIDES)
    parser.add_argument('--output', type=Path, default=OUTPUT)
    args = parser.parse_args(argv)
    try:
        if args.refresh:
            rows, exclusions = refresh(args.snapshot, args.overrides, args.output)
        else:
            snapshot = json.loads(args.snapshot.read_text(encoding='utf-8'))
            overrides = json.loads(args.overrides.read_text(encoding='utf-8'))
            header, rows, exclusions = generate(snapshot, overrides)
            if args.check:
                if not args.output.exists() or args.output.read_text(encoding='utf-8') != header:
                    raise ValueError('generated presets are stale/missing; run python3 tools/zen_radio_presets.py --generate')
            else:
                atomic_write(args.output, header)
        print(f'Zen presets: {len(rows)} included; {len(exclusions)} excluded; snapshot/header verified.')
        for title, reason in exclusions:
            print(f'  Excluded {title}: {reason}')
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f'Zen preset error: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
