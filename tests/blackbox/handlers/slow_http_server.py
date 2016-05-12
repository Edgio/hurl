#!/usr/bin/python
# ------------------------------------------------------------------------------
# imports
# ------------------------------------------------------------------------------
from BaseHTTPServer import BaseHTTPRequestHandler,HTTPServer
import time

# ------------------------------------------------------------------------------
# Slow HTTP Server
# ------------------------------------------------------------------------------
G_SLOW_HTTP_SERVER_PORT = 12346
class http_handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    #Handler for the GET requests
    def do_GET(self):
        time.sleep(2.0)
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.send_header('Content-length',len('monkeys are crazy!\n'))
        self.end_headers()
        self.wfile.write('monkeys are crazy!\n')
    #Handler for the GET requests
    def do_POST(self):
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.send_header('Content-length',len('monkeys are crazy!\n'))
        self.end_headers()
        self.wfile.write('monkeys are crazy!\n')
try:
    l_server = HTTPServer(('', G_SLOW_HTTP_SERVER_PORT), http_handler)
    l_server.serve_forever()
except KeyboardInterrupt:
    print '^C received, shutting down the web server'
    l_server.socket.close()


