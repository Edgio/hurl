#!/usr/bin/env python 
# ------------------------------------------------------------------------------
# imports
# ------------------------------------------------------------------------------
import socket 

# ------------------------------------------------------------------------------
# Response
# ------------------------------------------------------------------------------
g_resp ="""HTTP/1.1 200 OK
Server: nginx/1.5.10
Date: Sat, 03 Sep 2016 05:34:01 GMT
Content-Type: text/html; charset=utf-8
Content-Length: 12
Connection: keep-alive

HELLO WORLD
"""

# ------------------------------------------------------------------------------
# 
# ------------------------------------------------------------------------------
host = '' 
port = 50000
backlog = 5 
size = 1024 
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
s.bind((host,port)) 
s.listen(backlog) 
while 1: 
    client, address = s.accept() 
    data = client.recv(size) 
    if data: 
        client.send(g_resp) 
    client.close()

