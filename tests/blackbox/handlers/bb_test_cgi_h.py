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
    l_subproc_proxy = subprocess.Popen(["../../build/examples/cgi"])
    g_server_pid = l_subproc_proxy.pid
    time.sleep(0.5)

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
def teardown_func():
    global g_server_pid
    l_code, l_out, l_err = util.run_command('kill -9 %d'%(g_server_pid))
    time.sleep(0.5)

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
@with_setup(setup_func, teardown_func)
def bb_test_cgi_h_001():

    # script doesn't exist
    l_e = G_TEST_HOST + 'bloop.sh'
    l_r = requests.get(l_e)
    assert l_r.status_code == 404
    #print l_r.text
    l_js = l_r.json()
    assert l_js
    assert 'errors' in l_js
    assert len(l_js['errors']) > 0
    assert 'message' in l_js['errors'][0]
    assert l_js['errors'][0]['message'] == 'Not Found'

    # slow cgi times out
    l_e = G_TEST_HOST + 'slow.sh'
    l_r = requests.get(l_e)
    assert l_r.status_code == 200
    l_l = l_r.text.splitlines(True)
    l_err_msg = False
    for i_l in l_l:
        if i_l.startswith('cgi timeout'):
            l_err_msg = True
    assert l_err_msg

    # test vars
    l_e = G_TEST_HOST + 'vars.sh?var1=monkeys&var2=bananas&var3=pickles'
    l_r = requests.get(l_e)
    assert l_r.status_code == 200
    l_l = l_r.text.splitlines(True)
    l_found_var1 = False
    l_found_var2 = False
    l_found_var3 = False
    for i_l in l_l:
        #print i_l
        if 'var1' in i_l:
            assert 'var1: monkeys' in i_l
            l_found_var1 = True
        if 'var2' in i_l:
            assert 'var2: bananas' in i_l
            l_found_var2 = True
        if 'var3' in i_l:
            assert 'var3: pickles' in i_l
            l_found_var3 = True

    assert l_found_var1
    assert l_found_var2
    assert l_found_var3

    #print json.dumps(l_r_json,sort_keys=True,indent=4, separators=(',', ': '))

