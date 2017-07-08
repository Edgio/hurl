# ------------------------------------------------------------------------------
# Imports
# ------------------------------------------------------------------------------
import pytest
import subprocess
import requests
import os
import time
import json
# ------------------------------------------------------------------------------
# Constants
# ------------------------------------------------------------------------------
G_TEST_HOST = 'http://127.0.0.1:12345/'
# ------------------------------------------------------------------------------
# Globals
# ------------------------------------------------------------------------------
g_server_pid = -1
# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def run_command(command):
    p = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    return (p.returncode, stdout, stderr)
# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def setup_func():
    global g_server_pid
    l_subproc = subprocess.Popen(["../../build/examples/basic"])
    g_server_pid = l_subproc.pid
    time.sleep(0.5)
# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def teardown_func():
    global g_server_pid
    l_code, l_out, l_err = run_command('kill -9 %d'%(g_server_pid))
    time.sleep(0.5)
# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
@pytest.yield_fixture(autouse=True)
def run_around_tests():
    # before
    setup_func()
    # ...
    yield
    # after
    teardown_func()
# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def test_hurl_001():
    # Unimplemented request
    l_url = G_TEST_HOST + 'bananas'
    l_code, l_out, l_err = run_command('../../build/src/hurl/hurl %s -l3 -j -p1 -t1 -A100 -q'%(l_url))
    l_d = json.loads(l_out)
    assert 'response-codes' in l_d
    assert '200' in l_d['response-codes']
    assert 'fetches' in l_d
    assert l_d['fetches'] > 0
    assert 'fetches-per-sec' in l_d
    assert l_d['fetches-per-sec'] > 10
    assert l_d['fetches-per-sec'] < 200
