# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
from nose.tools import with_setup
import subprocess
import requests
import os
from .. import util
import time

# ------------------------------------------------------------------------------
# Constants
# ------------------------------------------------------------------------------
G_TEST_HOST = 'http://127.0.0.1:12345/'

# ------------------------------------------------------------------------------
# globals
# ------------------------------------------------------------------------------
g_server_pid = -1

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def setup_func():
    global g_server_pid
    l_subproc = subprocess.Popen(["../../build/examples/basic"])
    g_server_pid = l_subproc.pid
    time.sleep(0.2)

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def teardown_func():
    global g_server_pid
    l_code, l_out, l_err = util.run_command('kill -9 %d'%(g_server_pid))
    time.sleep(0.2)

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
@with_setup(setup_func, teardown_func)
def bb_test_basic_001():
    # Unimplemented request
    l_e = G_TEST_HOST + 'bleep/bloop/blop'
    l_r = requests.get(l_e)
    assert l_r.status_code == 501
    l_r_json = l_r.json()
    assert l_r_json != None
    assert len(l_r_json['errors']) > 0
    assert l_r_json['errors'][0]['code'] == 501

    # Valid request
    l_e = G_TEST_HOST + 'bananas'
    l_r = requests.get(l_e)
    assert l_r.status_code == 200
    assert 'Hello World' in l_r.text
