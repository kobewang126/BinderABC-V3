#!/usr/bin/env python3
"""Run the three real programs; stdout readiness replaces fixed sleeps."""
import pathlib
import selectors
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
processes = []
def start(name):
    process = subprocess.Popen([str(ROOT / 'build' / name)], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, bufsize=1)
    processes.append(process)
    return process

def expect(process, text):
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    try:
        assert selector.select(5), f'timed out waiting for {text}'
        line = process.stdout.readline().strip()
        assert text in line, (text, line)
        print(line)
    finally:
        selector.close()

def run_client(*args, success=True):
    result = subprocess.run([str(ROOT / 'build/calculator_client'), *args],
                            capture_output=True, text=True, timeout=10)
    assert (result.returncode == 0) == success, (result.returncode, result.stdout, result.stderr)
    print((result.stdout + result.stderr).strip())
    return result

try:
    manager = start('servicemanager')
    expect(manager, 'ServiceManager ready: handle 0')
    run_client('missing', success=False)
    provider = start('calculator_server')
    expect(provider, 'calculator registered: LOCAL(cookie=123)')
    first = run_client()
    assert 'remote add(7, 3) = 10' in first.stdout and 'remote sub(7, 3) = 4' in first.stdout
    run_client()
    simultaneous = [start('calculator_client'), start('calculator_client')]
    for caller in simultaneous:
        stdout, _ = caller.communicate(timeout=10)
        assert caller.returncode == 0 and 'remote add(7, 3) = 10' in stdout and 'remote sub(7, 3) = 4' in stdout
    print('two concurrent clients received their own results')
    duplicate = subprocess.run([str(ROOT / 'build/calculator_server')], capture_output=True,
                               text=True, timeout=10)
    assert duplicate.returncode == 1 and 'addService failed: -17' in duplicate.stderr
    print('duplicate registration rejected: EEXIST')
    print('three standalone processes: register, get and call passed')
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        remaining = process.stdout.read().strip() if not process.stdout.closed else ''
        if remaining:
            print(remaining)
        if not process.stdout.closed:
            process.stdout.close()
