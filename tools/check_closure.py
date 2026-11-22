"""Check Python syntax and tracked-file security without emitting secret contents."""
import ast
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
paths = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
failures = []
python_count = 0
secret = re.compile(rb'(?:ghp_|github_pat_)[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----')
for name in filter(None, paths):
    path = ROOT / name
    if not path.exists():
        continue
    if path.name in {'local_wifi.h', 'local_f450_wifi.h', 'local_secrets.h', '.env'}:
        failures.append(f'private configuration tracked: {name}')
    if path.suffix in {'.bin', '.elf', '.map', '.pyc', '.exe', '.pdb'}:
        failures.append(f'generated artifact tracked: {name}')
    raw = path.read_bytes()
    if secret.search(raw):
        failures.append(f'possible secret: {name}')
    if path.suffix == '.py':
        ast.parse(raw, filename=name)
        python_count += 1
assert not failures, '\n'.join(failures)
print(f'PASS: {python_count} Python files parsed; tracked private-config, artifact and secret-pattern scan')
