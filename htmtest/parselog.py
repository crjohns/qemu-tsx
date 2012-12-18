#!/usr/bin/python

import sys
import os
import re
from pprint import pformat


def usage():
    print "Usage: %s <file>" % sys.argv[0]
    exit(1)

if len(sys.argv) < 2:
    usage()

filename = sys.argv[1]

f = open(filename, 'r')


class Line:
    def __repr__(self):
        return "Line" + pformat(vars(self)).replace('\n', '')


def parseLine(lst):
    logtype = lst[0]

    line = Line()

    line.ltype = lst[0]
    line.time = int(lst[1])
    line.cpu = int(lst[3])
    line.pc = lst[5]

    line.begin = False
    line.commit = False
    line.abort = False
    
    if logtype == 'XBEGIN':
        line.begin = True
    elif logtype == 'XCOMMIT':
        line.commit = True
        line.cachelines = lst[7]
        pass
    elif logtype == 'XABORT':
        line.abort = True
        reason = lst[6]
        if reason[2] != '.':
            line.abort_a = True
        if reason[3] != '.':
            line.abort_r = True
        if reason[4] != '.':
            line.abort_c = True
        if reason[5] != '.':
            line.abort_o = True
        if reason[6] != '.':
            line.abort_b = True
        if reason[7] != '.':
            line.abort_n = True
        line.confcount = lst[8]
    elif logtype == 'EXEC':
        pass

    return line




class Transaction:

    def __init__(self, cpu):
        self.cpu = cpu
        self.logs = []
        self.complete = False

    def addLog(self, log):
        self.logs.append(log)
        if log.abort or log.commit:
            self.complete = True

    def completed(self):
        return self.complete

    def aborted(self):
        return lst and lst[-1].abort

    def committed(self):
        return lst and lst[-1].commit

alltrans = []
curtrans = dict()

for line in f.readlines():
    lst = line.rstrip().split(' ')
    pl = parseLine(lst)

    if pl.cpu in curtrans:
        trans = curtrans[pl.cpu]
    else:
        trans = Transaction(pl.cpu)

    trans.addLog(pl)

    if trans.completed():
        alltrans.append(trans)
        del curtrans[trans.cpu]
    else:
        curtrans[trans.cpu] = trans

assert(len(curtrans.keys()) == 0)


