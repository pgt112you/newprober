import os
import sys
import time
import errno
import struct
import ctypes 



def new_read_line(fd):
    len = 0
    try:
        recv_struct = struct.Struct('I') 
        recvbuf = ctypes.create_string_buffer(recv_struct.size)
        recvbuf = os.read(fd, recv_struct.size) 
        if recvbuf == "":
            return ""
        recvdata = recv_struct.unpack_from(recvbuf, 0)  
        len = recvdata[0]
    except OSError as err:
        if err.errno == errno.EAGAIN or err.errno == errno.EWOULDBLOCK:
            return ""
        else:
            print "read len wrong"
            return ""
    content = ""
    c = ""
    if (len == 0):
        return ""
    try:
        c = os.read(fd, len)
    except OSError as err:
        if err.errno == errno.EAGAIN or err.errno == errno.EWOULDBLOCK:
            print "read content wrong"
            return ""
    if c == '\n':
        return content
    elif c == '':
        return content
    else:
        content += c
    return content


path = "/usr/home/guangtong/newprober/newprober.fifo"
fifo = os.open(path, os.O_RDONLY|os.O_NONBLOCK)
while(1):
    print "hhhhhhh"
    line = new_read_line(fifo)
    while line:
        print "Received: " + line
        line = new_read_line(fifo)
    time.sleep(1)
os.close(fifo)


#def read_line(fd):
#    content = ""
#    while(1):
#        try:
#            c = os.read(fd, 1)
#        except OSError as err:
#            if err.errno == errno.EAGAIN or err.errno == errno.EWOULDBLOCK:
#                break
#        if c == '\n':
#            break
#        elif c == '':
#            break
#        else:
#            content += c
#    return content
#
#
#path = "/usr/home/guangtong/newprober/newprober.fifo"
#fifo = os.open(path, os.O_RDONLY|os.O_NONBLOCK)
#while(1):
#    print "hhhhhhhhh"
#    line = read_line(fifo)
#    while line:
#        print "Received: " + line
#        line = read_line(fifo)
#    time.sleep(1)
#os.close(fifo)

#print "aaaaaaaaaaaaa"
#fifo = open(path, "r")
#while(1):
#    print "hhhhhhhhhhhhhh"
#    #for line in fifo:
#    line = fifo.readline()
#    while line:
#        print "Received: " + line
#        line = fifo.readline()
#    time.sleep(1)
#fifo.close()
