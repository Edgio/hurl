#!/usr/bin/python
# ------------------------------------------------------------------------------
# imports
# ------------------------------------------------------------------------------
import BaseHTTPServer
import time
import sys
import argparse
import json
import random
import string
from datetime import datetime
from copy import deepcopy

# ------------------------------------------------------------------------------
# upstream response
# ------------------------------------------------------------------------------
G_FLOOD_RESP_TEMPLATE = {'status':200,'host':'XXX','port':80,'ttr':70,'response':[{'bananas':0}]}
G_ADMIN_BE_RESP_TEMPLATE = {}
G_TEST_RESP_TEMPLATE = {
    'stuff': [
        'junk',
        'games'
    ],
    'message': 'burrito',
    'how_many': 13,
    'blocks': [
       {'ducks': 2.14},
       {'tacos': 85}
    ]
}
G_WAIT_S = 0

# ------------------------------------------------------------------------------
# Upstream Server
# ------------------------------------------------------------------------------
class http_handler(BaseHTTPServer.BaseHTTPRequestHandler):
    global G_FLOOD_RESP_TEMPLATE
    global G_ADMIN_BE_RESP_TEMPLATE
    global G_TEST_RESP_TEMPLATE
    global G_WAIT_S

    protocol_version = 'HTTP/1.1'

    # ------------------------------------------------------
    # GET
    # ------------------------------------------------------
    def do_GET(self):
        
        # --------------------------------------------------
        # json resp
        # --------------------------------------------------
        if self.path.startswith('/cool_json_api'):
            if G_WAIT_S:
                time.sleep(G_WAIT_S)
            #print self.requestline
            #print self.headers
            self.send_response(200)
            self.send_header('Content-type','application/json')
            random.seed(datetime.now())
            l_str = json.dumps(G_TEST_RESP_TEMPLATE)
            self.send_header('Content-length',len(l_str))
            self.end_headers()
            self.wfile.write(l_str)

        # --------------------------------------------------
        # not json resp
        # --------------------------------------------------
        elif self.path.startswith('/not_json_api'):
            #print self.requestline
            #print self.headers
            self.send_response(200)
            self.send_header('Content-type','txt/html')
            random.seed(datetime.now())
            l_str = 'eJw1kLFuwzAMRPd+xW1ZjCxduhcF0rn5AdqhIyKSaIh0DP99KQdZNJzIu3e8JpYGppZ3bNoe2MhAyOKeGa6KyluIT8asDWWHkznbgHF1bIkr/hZtbpioMDREqTh9fQ74hSepj3h5R2PKEXEMSXXtagTrVgdMWgq3SY4JqjdQczGXqQtnXBNHkAYO5XEtSAfhlAM6dpvYAtO13obX7gFsiYNM5/CutpZCzliazmwmWimLlSAgf3Pd5cnWmcKq3rv/KHeMquZnXPhkGLthkC7U+BaHwU9+iuE7Jjhnfd3jXfmyRuUX50xxNW0chu7ciffam8G4GnfEtMb/+eMfg9yMNQ=='
            self.send_header('Content-length',len(l_str))
            self.end_headers()
            self.wfile.write(l_str)

        # --------------------------------------------------
        # not json resp
        # --------------------------------------------------
        elif self.path.startswith('/bad_json_api'):
            #print self.requestline
            #print self.headers
            self.send_response(200)
            self.send_header('Content-type','application/json')
            random.seed(datetime.now())
            l_str = '{{{{{{[[[[eJw1kLFuwzAMRPd+xW1ZjCxduhcF0rn5AdqhIyKSaIh0DP99KQdZNJzIu3e8JpYGppZ3bNoe2MhAyOKeGa6KyluIT8asDWWHkznbgHF1bIkr/hZtbpioMDREqTh9fQ74hSepj3h5R2PKEXEMSXXtagTrVgdMWgq3SY4JqjdQczGXqQtnXBNHkAYO5XEtSAfhlAM6dpvYAtO13obX7gFsiYNM5/CutpZCzliazmwmWimLlSAgf3Pd5cnWmcKq3rv/KHeMquZnXPhkGLthkC7U+BaHwU9+iuE7Jjhnfd3jXfmyRuUX50xxNW0chu7ciffam8G4GnfEtMb/+eMfg9yMNQ=='
            self.send_header('Content-length',len(l_str))
            self.end_headers()
            self.wfile.write(l_str)

        # --------------------------------------------------
        # /flood/server-admin-be
        # --------------------------------------------------
        elif self.path.startswith('/be-admin'):
            self.send_response(200)
            self.send_header('Content-type','application/json')
            random.seed(datetime.now())
            l_str = json.dumps(G_ADMIN_BE_RESP_TEMPLATE)
            print l_str
            self.send_header('Content-length',len(l_str))
            self.end_headers()
            self.wfile.write(l_str)

        # --------------------------------------------------
        # default endpoint
        # --------------------------------------------------
        else:
            self.send_response(200)
            self.send_header('Content-type','application/json')
            l_resp = {'response': 'hello wayne'}
            l_str = json.dumps(l_resp)
            self.send_header('Content-length',len(l_str))
            self.end_headers()
            self.wfile.write(l_str)

# ------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------
def main(argv):
    
    arg_parser = argparse.ArgumentParser(
                description='Upstream Server.',
                usage= '%(prog)s',
                epilog= '')

    # port
    arg_parser.add_argument('-p',
                            '--port',
                            dest='port',
                            help='Port',
                            type=int,
                            required=True)

    # port
    arg_parser.add_argument('-w',
                            '--wait',
                            dest='wait_s',
                            help='Wait For N seconds before responding',
                            type=int,
                            default=0,
                            required=False)

    l_args = arg_parser.parse_args()
    
    global G_WAIT_S
    G_WAIT_S = l_args.wait_s    
    global G_ADMIN_BE_RESP_TEMPLATE
    with open('../data/server_be_resp.json') as l_f:    
        G_ADMIN_BE_RESP_TEMPLATE = json.load(l_f)
    try:
        l_server = BaseHTTPServer.HTTPServer(('', l_args.port), http_handler)
        l_server.serve_forever()
    except KeyboardInterrupt:
        print '^C received, shutting down the web server'
        l_server.socket.close()
    except Exception as l_e:
        l_err_str = 'ERROR: type: %s error: %s, doc: %s, message: %s'%(type(l_e), l_e, l_e.__doc__, l_e.message)
        print l_err_str

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
if __name__ == '__main__':
    main(sys.argv[1:])
