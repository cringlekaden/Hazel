#!/usr/bin/env python3
"""Hermes/Hazel deterministic goal gate. Run from anywhere inside the worktree.

Python stdlib only. No shell evaluation. Checks fail closed unless real, verified
build and test commands are configured. Evidence written to .hermes-run/.
"""
import argparse
import datetime as dt
import json
import os
import pathlib
import re
import signal
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONFIG = ROOT / 'scripts' / 'hermes_render_gate.json'
OUT = ROOT / '.hermes-run'


def command(argv, timeout=10):
    # Start a separate process group so a timed-out build cannot leave compilers
    # continuing in the background and consuming the Mac's unified memory.
    proc = subprocess.Popen(argv, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, start_new_session=True)
    try:
        out, _ = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as ex:
        os.killpg(proc.pid, signal.SIGTERM)
        try:
            out, _ = proc.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            out, _ = proc.communicate()
        raise subprocess.TimeoutExpired(argv, timeout, output=out) from ex
    return subprocess.CompletedProcess(argv, proc.returncode, out)


def repo_path(value):
    path = (ROOT / value).resolve()
    if path != ROOT and ROOT not in path.parents:
        raise ValueError(f'Path escapes worktree: {value}')
    return path


def load_contract():
    if not CONFIG.is_file():
        raise ValueError(f'Missing {CONFIG}')
    data = json.loads(CONFIG.read_text(encoding='utf-8'))
    if data.get('schema_version') != 1:
        raise ValueError('schema_version must be 1')
    items = data.get('commands')
    if not isinstance(items, list) or not items:
        raise ValueError('commands must contain actual native build and test commands')
    roles = set()
    for step in items:
        if not isinstance(step, dict):
            raise ValueError('Each command must be an object')
        role = step.get('role')
        roles.add(role)
        argv = step.get('argv')
        if role not in ('build', 'test'):
            raise ValueError('Each command role must be build or test')
        if not isinstance(argv, list) or not argv or not all(isinstance(s, str) and s.strip() for s in argv):
            raise ValueError(f'{role} has no valid argv; replace the placeholder using the AUDIT')
        if any('REPLACE' in s or '<' in s or '>' in s for s in argv):
            raise ValueError('A command still contains placeholder text')
        t = step.get('timeout_seconds')
        if not isinstance(t, int) or not 1 <= t <= 240:
            raise ValueError('timeout_seconds must be an integer between 1 and 240')
    if not {'build', 'test'}.issubset(roles):
        raise ValueError('At least one build and one test command are mandatory')
    required = data.get('required_paths')
    if not isinstance(required, list) or not required or not all(isinstance(s, str) and s for s in required):
        raise ValueError('required_paths must list one or more real paths')
    for value in required:
        repo_path(value)
    total = data.get('max_total_seconds', 265)
    if not isinstance(total, int) or not 20 <= total <= 270:
        raise ValueError('max_total_seconds must be in 20..270 (Hermes gates time out at 300s by default)')
    branch_prefix = data.get('branch_prefix', 'agent/')
    if not isinstance(branch_prefix, str):
        raise ValueError('branch_prefix must be a string')
    return data


def write_evidence(blob):
    OUT.mkdir(exist_ok=True)
    tmp = OUT / 'last_verify.json.tmp'
    tmp.write_text(json.dumps(blob, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    tmp.replace(OUT / 'last_verify.json')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--preflight', action='store_true', help='Validate contract and show exact commands; do not build')
    args = p.parse_args()
    start = time.monotonic()
    now = dt.datetime.now(dt.timezone.utc).isoformat()
    evidence = {'timestamp': now, 'overall': 'FAIL', 'head': '', 'branch': '', 'commands': [],
                'errors': [], 'duration_seconds': 0}
    try:
        data = load_contract()
        branch = command(['git', 'branch', '--show-current'])
        if branch.returncode:
            raise RuntimeError(f'Not a git worktree: {branch.stdout}')
        evidence['branch'] = branch.stdout.strip()
        evidence['head'] = command(['git', 'rev-parse', 'HEAD']).stdout.strip()
        prefix = data['branch_prefix'] if 'branch_prefix' in data else 'agent/'
        if prefix and not evidence['branch'].startswith(prefix):
            raise RuntimeError(f'Unsafe branch: {evidence["branch"]!r}; required prefix {prefix!r}')
        for relative in data['required_paths']:
            if not repo_path(relative).exists():
                raise FileNotFoundError(f'Required artifact missing: {relative}')
        diff = command(['git', 'diff', '--check', 'HEAD'])
        if diff.returncode:
            raise RuntimeError(f'git diff --check HEAD failed:\n{diff.stdout}')
        print('Verified branch:', evidence['branch'])
        print('Required paths:', ', '.join(data['required_paths']))
        for step in data['commands']:
            print(step['role'] + ':', ' '.join(step['argv']))
        if args.preflight:
            print('PREFLIGHT PASSED (build and test NOT run)')
            return 0
        OUT.mkdir(exist_ok=True)
        (OUT / 'logs').mkdir(exist_ok=True)
        deadline = start + data['max_total_seconds']
        for idx, step in enumerate(data['commands'], 1):
            left = int(deadline - time.monotonic())
            if left <= 0:
                raise TimeoutError('Overall quality-gate time budget exhausted')
            timeout = min(step['timeout_seconds'], left)
            t0 = time.monotonic()
            print(f'[{idx}/{len(data["commands"])}] {step["role"]}: {" ".join(step["argv"])}', flush=True)
            stdout = ''
            exit_code = 124
            try:
                proc = command(step['argv'], timeout=timeout)
                stdout, exit_code = proc.stdout, proc.returncode
            except subprocess.TimeoutExpired as ex:
                stdout = ex.stdout or b''
                if isinstance(stdout, bytes):
                    stdout = stdout.decode('utf-8', 'replace')
                stdout += f'\nTIMED OUT after {timeout}s'
            elapsed = round(time.monotonic() - t0, 2)
            logname = f'{idx:02d}-{step["role"]}-{re.sub(r"[^a-z0-9_-]", "_", step.get("name", "check").lower())}.log'
            (OUT / 'logs' / logname).write_text(stdout, encoding='utf-8', errors='replace')
            evidence['commands'].append({'role': step['role'], 'name': step.get('name', step['role']),
                                         'argv': step['argv'], 'returncode': exit_code,
                                         'duration_seconds': elapsed, 'log': str(OUT / 'logs' / logname)})
            print(stdout[-1800:].rstrip())
            if exit_code != 0:
                raise RuntimeError(f'{step["role"]} failed with exit code {exit_code}; log: {OUT / "logs" / logname}')
        evidence['overall'] = 'PASS'
        print('QUALITY GATE: PASS')
        return 0
    except (ValueError, RuntimeError, OSError, TimeoutError, json.JSONDecodeError) as ex:
        evidence['errors'].append(str(ex))
        print(f'QUALITY GATE: FAIL — {ex}', file=sys.stderr)
        return 1
    finally:
        if not args.preflight:
            evidence['duration_seconds'] = round(time.monotonic() - start, 2)
            write_evidence(evidence)


if __name__ == '__main__':
    sys.exit(main())
