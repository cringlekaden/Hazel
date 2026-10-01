#!/usr/bin/env python3
"""Compare tracked trees; hashes include vendor blobs and dependency gitlinks."""
import argparse
import collections
import csv
import json
from pathlib import Path
import subprocess

BASELINE = "b030be7"
TARGET = "1feb70572fa87fa1c4ba784a2cfeada5b4a500db"

def tree(repo, revision):
    raw = subprocess.check_output(["git", "-C", str(repo), "ls-tree", "-rz", revision])
    entries = {}
    for record in raw.split(b"\0"):
        if record:
            meta, name = record.decode().split("\t", 1)
            mode, kind, oid = meta.split()
            entries[name] = (mode, kind, oid)
    return entries

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    parser.add_argument("--baseline", default=BASELINE)
    parser.add_argument("--output", type=Path, default=Path("docs/migration/evidence"))
    args = parser.parse_args()
    local = tree(Path(__file__).resolve().parents[2], args.baseline)
    upstream = tree(args.upstream, TARGET)
    rows = []
    for name in sorted(local.keys() | upstream.keys()):
        a, b = local.get(name), upstream.get(name)
        status = "upstream-only" if a is None else "local-only" if b is None else "identical" if a == b else "changed"
        rows.append([name, status, *(a or ("", "", "")), *(b or ("", "", ""))])
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "source-comparison.tsv").open("w", newline="") as f:
        writer = csv.writer(f, delimiter="\t", lineterminator="\n")
        writer.writerow(["path", "status", "local_mode", "local_type", "local_oid", "upstream_mode", "upstream_type", "upstream_oid"])
        writer.writerows(rows)
    summary = {"baseline": args.baseline, "target": TARGET,
               "local_entries": len(local), "upstream_entries": len(upstream),
               "counts": dict(collections.Counter(row[1] for row in rows)),
               "engine_counts": dict(collections.Counter(row[1] for row in rows if row[0].startswith("Hazel/src/"))),
               "gitlinks": [row for row in rows if row[3] == "commit" or row[6] == "commit"]}
    (args.output / "comparison-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({k: v for k, v in summary.items() if k != "gitlinks"}, indent=2))

if __name__ == "__main__":
    main()
