#!/usr/bin/env python3
"""Read-only local Markdown link/image check from README.md; no network or build claims."""
from __future__ import annotations

import argparse
import re
import sys
from collections import deque
from pathlib import Path
from urllib.parse import unquote, urlsplit

# Lightweight Markdown scanner; complex HTML, generated pages and custom anchor syntax
# still need human review. Keep this dependency-free and deliberately conservative.
INLINE_LINK = re.compile(r"!?\[[^\]\n]*\]\((<[^>]+>|[^)\n]+)\)")
REFERENCE_DEF = re.compile(r"^\s{0,3}\[[^\]\n]+\]:\s*(<[^>]+>|\S+)", re.M)
HTML_LINK = re.compile(r'''(?:href|src)\s*=\s*["']([^"']+)["']''', re.I)
HEADING = re.compile(r"^#{1,6}\s+(.+?)\s*#*\s*$", re.M)
EXPLICIT_ID = re.compile(r'''\bid\s*=\s*["']([^"']+)["']''', re.I)
FENCE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")


def without_code(text: str) -> str:
    lines, active = [], None
    for line in text.splitlines():
        match = FENCE.match(line)
        if match:
            fence = match.group(1)
            if active is None:
                active = fence
            elif fence[0] == active[0] and len(fence) >= len(active):
                active = None
            lines.append("")
            continue
        if active is not None:
            lines.append("")
            continue
        # Remove inline code and comments; not a full Markdown parser.
        line = re.sub(r"`+[^`\n]*`+", "", line)
        lines.append(line)
    return re.sub(r"<!--.*?-->", "", "\n".join(lines), flags=re.S)


def destination(raw: str) -> str:
    raw = raw.strip()
    if raw.startswith("<") and ">" in raw:
        return raw[1:raw.index(">")]
    # Markdown link title following a bare URL: (path "title").
    return raw.split(maxsplit=1)[0] if raw else ""


def local_path(source: Path, url: str, root: Path):
    if not url or url.startswith("//"):
        return None
    if url.startswith("#"):
        return source, unquote(url[1:])
    parts = urlsplit(url)
    if parts.scheme or parts.netloc or url.startswith("mailto:"):
        return None
    if parts.path.startswith("/"):
        # Website-root paths may not map to a checkout; leave them for manual review.
        return None
    path = (source.parent / unquote(parts.path)).resolve()
    if not path.is_relative_to(root):
        return None
    return path, unquote(parts.fragment)


def correct_case(path: Path, root: Path) -> bool:
    rel = path.relative_to(root)
    current = root
    for segment in rel.parts:
        try:
            matches = {p.name for p in current.iterdir()}
        except OSError:
            return False
        if segment not in matches:
            return False
        current = current / segment
    return True


def anchors_in(markdown: str) -> set[str]:
    slugs, dup = set(), {}
    for heading in HEADING.findall(without_code(markdown)):
        heading = re.sub(r"!?\[([^\]]+)\]\([^)]*\)", r"\1", heading)
        heading = re.sub(r"<[^>]+>", "", heading)
        heading = re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-")
        n = dup.get(heading, 0)
        dup[heading] = n + 1
        slugs.add(heading if not n else f"{heading}-{n}")
    slugs.update(EXPLICIT_ID.findall(markdown))
    return slugs


def scan(path: Path) -> list[str]:
    content = without_code(path.read_text(encoding="utf-8"))
    found = [destination(x) for x in INLINE_LINK.findall(content)]
    found.extend(destination(x) for x in REFERENCE_DEF.findall(content))
    found.extend(HTML_LINK.findall(content))
    return [x for x in found if x]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--entry", default="README.md", help="relative Markdown entry point")
    parser.add_argument("--max-depth", type=int, default=5)
    parser.add_argument("--max-docs", type=int, default=200)
    parser.add_argument("--check-anchors", action="store_true", help="best-effort GitHub Markdown heading checks")
    args = parser.parse_args()
    root = args.root.resolve()
    entry = (root / args.entry).resolve()
    if args.max_depth < 0 or args.max_docs < 1 or not entry.is_file() or not entry.is_relative_to(root):
        print("Invalid root, entry, depth or document limit", file=sys.stderr)
        return 2
    todo, seen, findings = deque([(entry, 0)]), set(), []
    links = 0
    while todo and len(seen) < args.max_docs:
        source, depth = todo.popleft()
        if source in seen:
            continue
        seen.add(source)
        try:
            urls = scan(source)
        except (OSError, UnicodeError) as exc:
            findings.append(f"ERROR {source.relative_to(root)}: could not read ({exc})")
            continue
        for url in urls:
            local = local_path(source, url, root)
            if local is None:
                continue
            target, fragment = local
            links += 1
            where = source.relative_to(root)
            if not target.exists():
                findings.append(f"MISSING {where}: {url}")
                continue
            if not correct_case(target, root):
                findings.append(f"CASE {where}: path differs in letter case: {url}")
            if args.check_anchors and fragment and target.is_file() and target.suffix.lower() in (".md", ".markdown"):
                try:
                    if fragment not in anchors_in(target.read_text(encoding="utf-8")):
                        findings.append(f"ANCHOR {where}: {url} (verify complex GitHub headings manually)")
                except (OSError, UnicodeError):
                    pass
            if depth < args.max_depth and target.is_file() and target.suffix.lower() in (".md", ".markdown") and target not in seen:
                todo.append((target, depth + 1))
    for item in findings:
        print(item)
    if todo:
        print(f"LIMIT: reached max-docs={args.max_docs}; deeper links were not inspected")
    print(f"Checked {len(seen)} Markdown files, {links} local links/images; {len(findings)} finding(s).")
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
