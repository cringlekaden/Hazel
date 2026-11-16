#!/usr/bin/env python3
"""Validate the minimum shape of Hazel's repo-local Codex setup (no dependencies)."""
from pathlib import Path
import argparse
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    root = parser.parse_args().root.resolve()
    missing = [name for name in ("AGENTS.md", "CODEX_SETUP.md", "docs/agents/PROJECT.md") if not (root / name).is_file()]
    skills = sorted((root / ".agents/skills").glob("*/SKILL.md"))
    errors = [f"Missing: {name}" for name in missing]
    names = set()
    if not skills:
        errors.append("No skills found in .agents/skills/")
    for skill in skills:
        content = skill.read_text(encoding="utf-8")
        match = re.match(r"\A---\n(.*?)\n---\n", content, flags=re.S)
        if not match:
            errors.append(f"{skill.relative_to(root)}: missing YAML frontmatter")
            continue
        header = match.group(1)
        name = re.search(r"^name:\s*(.+)$", header, re.M)
        description = re.search(r"^description:\s*(.+)$", header, re.M)
        if not name or not description or not description.group(1).strip():
            errors.append(f"{skill.relative_to(root)}: requires name and description")
        else:
            value = name.group(1).strip()
            if value in names:
                errors.append(f"Duplicate skill name: {value}")
            names.add(value)
            if value != skill.parent.name:
                errors.append(f"Skill folder differs from frontmatter name: {skill}")
    for error in errors:
        print("ERROR", error)
    print(f"Codex setup: {len(skills)} skills; {'FAIL' if errors else 'PASS'}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
