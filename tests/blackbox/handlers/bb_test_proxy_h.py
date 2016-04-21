# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
from nose.tools import with_setup
import subprocess
import requests
import os
from .. import util
import time
import json

# ------------------------------------------------------------------------------
# Constants
# ------------------------------------------------------------------------------
G_TEST_HOST = 'http://127.0.0.1:12345/'

# ------------------------------------------------------------------------------
# globals
# ------------------------------------------------------------------------------
g_server_pid = -1
g_ups_server_pid = -1

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def setup_func():
    global g_server_pid
    l_subproc_proxy = subprocess.Popen(["../../build/examples/proxy"])
    g_server_pid = l_subproc_proxy.pid
    global g_ups_server_pid
    l_subproc_ups = subprocess.Popen(["./handlers/slow_http_server.py"])
    g_ups_server_pid = l_subproc_ups.pid
    time.sleep(0.5)

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def teardown_func():
    global g_server_pid
    l_code, l_out, l_err = util.run_command('kill -9 %d'%(g_server_pid))
    l_code, l_out, l_err = util.run_command('kill -9 %d'%(g_ups_server_pid))
    time.sleep(0.5)

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
@with_setup(setup_func, teardown_func)
def bb_test_proxy_h_001():

    # Unimplemented request
    l_e = G_TEST_HOST + 'bleep/bloop/blop'
    l_r = requests.get(l_e)
    assert l_r.status_code == 200
    assert 'Hello World' in l_r.content
    
    # Hit slow server -and verify 504
    l_e = G_TEST_HOST + 'proxy/'
    l_r = requests.get(l_e)
    assert l_r.status_code == 504
    l_js = l_r.json()
    assert l_js
    assert 'errors' in l_js
    assert len(l_js['errors']) >= 1
    assert 'code' in l_js['errors'][0]
    assert l_js['errors'][0]['code'] == 504
    assert l_js['errors'][0]['message'] == 'Gateway Timeout'
