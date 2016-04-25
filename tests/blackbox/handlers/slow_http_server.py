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
    #Handler for the GET requests
    def do_GET(self):
        time.sleep(0.5)
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.send_header('Content-length',len('monkeys are crazy!'))
        self.end_headers()
        self.wfile.write('monkeys are crazy!')
        return
try:
    l_server = HTTPServer(('', G_SLOW_HTTP_SERVER_PORT), http_handler)
    l_server.protocol_version = 'HTTP/1.1'
    l_server.serve_forever()
except KeyboardInterrupt:
    print '^C received, shutting down the web server'
    l_server.socket.close()


