#!/usr/bin/python
# ------------------------------------------------------------------------------
# Upstream HTTP Server for use with testing hurl.
# Chunking bits copied from
# https://gist.github.com/josiahcarlson/3250376
# example notes:
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# chunked_server_test.py
# Copyright August 3, 2012
# Released into the public domain
# This implements a chunked server using Python threads and the built-in
# BaseHTTPServer module. Enable gzip compression at your own peril - web
# browsers seem to have issues, though wget, curl, Python's urllib2, my own
# async_http library, and other command-line tools have no problems.
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# imports
# ------------------------------------------------------------------------------
from BaseHTTPServer import BaseHTTPRequestHandler,HTTPServer
import time
import sys
import argparse
import json
import random
import string
from datetime import datetime
from copy import deepcopy

import SocketServer
import gzip

# ------------------------------------------------------------------------------
# globals
# ------------------------------------------------------------------------------
# upstream response
G_TEST_RESP_TEMPLATE = {
    'stuff': [
        'junk',
        'games'
    ],
    'message': 'burrito',
    'how_many': 13,
    'blocks': [
       {'ducks': 2},
       {'tacos': 85}
    ]
}
G_RESPONSE_FILE = ''

# ------------------------------------------------------------------------------
# Upstream Server
# ------------------------------------------------------------------------------
class UpstreamHTTPServer(SocketServer.ThreadingMixIn,
                         HTTPServer):
        
    # ------------------------------------------------------
    # Docs:
    # This flag specifies if when your webserver terminates
    # all in-progress client connections should be droppped.
    # It defaults to False. You might want to set this to
    # True if you are using HTTP/1.1 and don't set a
    # socket_timeout.     
    # ------------------------------------------------------
    daemon_threads = True

# ------------------------------------------------------------------------------
# ListBuffer
# ------------------------------------------------------------------------------
class ListBuffer(object):
    # ------------------------------------------------------
    # notes from original gist -see header comments:
    # ++++++++++++++++++++++++++++++++++++++++++++++++++++++
    # This little bit of code is meant to act as a l_gz_buffer
    # between the optional gzip writer and the actual
    # outgoing socket - letting us properly construct
    # the chunked l_gz_output. It also lets us quickly and easily
    # determine whether we need to flush gzip in the case
    # where a user has specified
    # 'ALWAYS_SEND_SOME'.
    # 
    # This offers a minimal interface necessary to back a
    # writing gzip stream.
    # ++++++++++++++++++++++++++++++++++++++++++++++++++++++
    # ------------------------------------------------------
    __slots__ = 'l_gz_buffer',
    def __init__(self):
        self.l_gz_buffer = []

    def __nonzero__(self):
        return len(self.l_gz_buffer)

    def write(self, data):
        if data:
            self.l_gz_buffer.append(data)

    def flush(self):
        pass

    def getvalue(self):
        data = ''.join(self.l_gz_buffer)
        self.l_gz_buffer = []
        return data

# ------------------------------------------------------------------------------
# chunk_generator
# ------------------------------------------------------------------------------
def chunk_generator():
    # generate some chunks
    for i in xrange(10):
        time.sleep(.01)
        yield "this is i_chunk: %s\r\n"%i

# ------------------------------------------------------------------------------
# Upstream Server
# ------------------------------------------------------------------------------
class UpstreamHTTPHandler(BaseHTTPRequestHandler):
    global G_TEST_RESP_TEMPLATE
    global G_RESPONSE_FILE
    protocol_version = 'HTTP/1.1'
    
    ALWAYS_SEND_SOME = False
    ALLOW_GZIP = False

    #Handler for the GET requests
    def do_GET(self):
        # --------------------------------------------------
        # /cool_json_api
        # --------------------------------------------------
        if self.path.startswith('/cool_json_api'):
            self.send_response(200)
            self.send_header('Content-type','application/json')
            random.seed(datetime.now())
            
            l_str = json.dumps(G_TEST_RESP_TEMPLATE)
            print l_str
            self.send_header('Content-length',len(l_str))
            self.end_headers()
            self.wfile.write(l_str)

        # --------------------------------------------------
        # /chunked
        # notes from original gist -see header comments:
        # ++++++++++++++++++++++++++++++++++++++++++++++++++
        # Nothing is terribly magical about this code, the
        # only thing that you need to really do is tell the
        # client that you're going to be using a chunked
        # transfer encoding. Gzip compression works
        # partially. See the module notes for more info.
        # ++++++++++++++++++++++++++++++++++++++++++++++++++
        # --------------------------------------------------
        if self.path.startswith('/chunked'):
            
            l_accept_encoding = self.headers.get('accept-encoding') or ''
            l_use_gzip = 'gzip' in l_accept_encoding and self.ALLOW_GZIP
    
            # send some headers
            self.send_response(200)
            self.send_header('Transfer-Encoding', 'chunked')
            self.send_header('Content-type', 'text/plain')
    
            # use gzip as requested
            if l_use_gzip:
                self.send_header('Content-Encoding', 'gzip')
                l_gz_buffer = ListBuffer()
                l_gz_output = gzip.GzipFile(mode='wb', fileobj=l_gz_buffer)

            self.end_headers()

            # ----------------------------------------------
            # from file
            # ----------------------------------------------
            print 'FILE: %s'%(G_RESPONSE_FILE)
            if G_RESPONSE_FILE:
                print 'opening FILE: %s'%(G_RESPONSE_FILE)
                with open(G_RESPONSE_FILE) as l_f:
                    for i_line in l_f:
                        l_tosend = '%X\r\n%s\r\n'%(len(i_line), i_line)
                        self.wfile.write(l_tosend)
                # send the chunked trailer
                self.wfile.write('0\r\n\r\n')

            # ----------------------------------------------
            # string chunks
            # ----------------------------------------------
            else:
                # get some chunks
                for i in xrange(10):
                    time.sleep(.01)
                    i_chunk = "this is i_chunk: %s\r\n"%i
    
                    # we've got to compress the i_chunk
                    if l_use_gzip:
                        l_gz_output.write(i_chunk)
                        # we'll force some l_gz_output from gzip if necessary
                        if self.ALWAYS_SEND_SOME and not l_gz_buffer:
                            l_gz_output.flush()
                        i_chunk = l_gz_buffer.getvalue()
        
                        # not forced, and gzip isn't ready to produce
                        if not i_chunk:
                            continue
        
                    l_tosend = '%X\r\n%s\r\n'%(len(i_chunk), i_chunk)
                    self.wfile.write(l_tosend)
    
                # no more chunks!
    
                if l_use_gzip:
                    # force the ending of the gzip stream
                    l_gz_output.close()
                    i_chunk = l_gz_buffer.getvalue()
                    if i_chunk:
                        l_tosend = '%X\r\n%s\r\n'%(len(i_chunk), i_chunk)
                        self.wfile.write(l_tosend)
        
                # send the chunked trailer
                self.wfile.write('0\r\n\r\n')

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


    # file
    arg_parser.add_argument('-f',
                            '--file',
                            dest='file',
                            help='File to send (for response)',
                            type=str,
                            required=False)


    args = arg_parser.parse_args()

    global G_RESPONSE_FILE
    if args.file:
        G_RESPONSE_FILE = args.file

    try:
        l_server = UpstreamHTTPServer(('', args.port), UpstreamHTTPHandler)
        l_server.serve_forever()
    except KeyboardInterrupt:
        print '^C received, shutting down the web server'
        l_server.socket.close()

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
if __name__ == '__main__':
    main(sys.argv[1:])
