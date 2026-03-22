#!/usr/bin/env python3
"""Parse sysmon console output into JSON consumed by the web UI."""

import json
import re
import sys
from datetime import datetime
from pathlib import Path

SNAP_RE = re.compile(r'^SNAP\s+(.*)$')
PROC_RE = re.compile(r'^PROC\s+(.*)$')
PAIR_RE = re.compile(r'(\w+)=([^\s]+)')

INT_FIELDS = {
    'seq', 'ts', 'policy', 'cpu', 'ctx', 'ticks', 'total_pages', 'free_pages',
    'nr_total', 'nr_running', 'nr_sleeping', 'nr_zombie', 'runqueue',
    'buddy_merges', 'buddy_splits', 'cow_faults', 'lazy_allocs', 'cow_copy_pages',
    'pid', 'priority', 'tickets', 'sched_class', 'sz', 'rutime', 'retime', 'stime',
}


def parse_pairs(payload):
    record = {}
    for key, value in PAIR_RE.findall(payload):
        record[key] = int(value) if key in INT_FIELDS else value
    return record


def parse_file(path):
    snapshots = []
    processes = []

    for raw_line in Path(path).read_text(encoding='utf-8', errors='ignore').splitlines():
        line = raw_line.strip()
        match = SNAP_RE.match(line)
        if match:
            snapshots.append(parse_pairs(match.group(1)))
            continue
        match = PROC_RE.match(line)
        if match:
            processes.append(parse_pairs(match.group(1)))

    latest_seq = max((item['seq'] for item in snapshots), default=-1)
    latest_processes = [item for item in processes if item.get('seq') == latest_seq]

    return {
        'generated_at': datetime.now().isoformat(),
        'source': str(path),
        'samples': snapshots,
        'latest_processes': latest_processes,
        'summary': {
            'sample_count': len(snapshots),
            'latest_seq': latest_seq,
            'latest_scheduler': snapshots[-1]['sched'] if snapshots else None,
        },
    }


def main(argv):
    if len(argv) < 2:
        print('Usage: python3 webui/parse_sysmon.py <raw-log> [output-json]')
        return 1

    source = Path(argv[1])
    target = Path(argv[2]) if len(argv) > 2 else Path('webui/data/snapshots.json')
    payload = parse_file(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding='utf-8')
    print(f'Wrote {target}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
