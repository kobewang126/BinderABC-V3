#!/usr/bin/env python3
"""Exercise C/C++ interoperability through three real processes and the driver."""
import pathlib
import selectors
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
processes = []

def start(name):
    p = subprocess.Popen([str(ROOT / 'build' / name)], stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True)
    processes.append(p)
    return p

def expect(p, expected):
    with selectors.DefaultSelector() as selector:
        selector.register(p.stdout, selectors.EVENT_READ)
        assert selector.select(5), f'timed out: {expected}'
        line = p.stdout.readline().strip()
        assert expected in line, (expected, line)
        print(line, flush=True)

def run(name, *args, success=True):
    p = subprocess.run([str(ROOT / 'build' / name), *args], capture_output=True,
                       text=True, timeout=15)
    print((p.stdout + p.stderr).strip(), flush=True)
    assert (p.returncode == 0) == success, (name, p.returncode, p.stdout, p.stderr)
    return p

try:
    manager = start('servicemanager')
    expect(manager, 'ServiceManager ready: handle 0')
    run('framework_client', success=False)  # service not registered yet
    server = start('framework_server')
    expect(server, 'BBinder calculator registered: LOCAL(cookie=123)')
    output = run('framework_client').stdout
    assert 'IPCThreadState add(7, 3) = 10' in output and 'IPCThreadState sub(7, 3) = 4' in output
    run('framework_test')
    run('calculator_client')  # old C client -> new C++ BBinder
    run('service_parcel_test')
    duplicate = run('framework_server', success=False)
    assert 'addService: -17' in duplicate.stderr
    print('C ServiceManager + C++ server/client; C client interoperability and regressions passed', flush=True)
finally:
    for p in reversed(processes):
        if p.poll() is None:
            p.terminate()
        try:
            p.wait(timeout=3)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait(timeout=3)
        p.stdout.close()
