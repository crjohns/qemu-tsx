#!/usr/bin/python

# This parses the log file output in "execlog" and prints statistics

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
        line.cachelines = int(lst[7])
        pass
    elif logtype == 'XABORT':
        line.abort = True
        line.reason = lst[6]
        if line.reason[2] != '.':
            line.abort_a = True
        if line.reason[3] != '.':
            line.abort_r = True
        if line.reason[4] != '.':
            line.abort_c = True
        if line.reason[5] != '.':
            line.abort_o = True
        if line.reason[6] != '.':
            line.abort_b = True
        if line.reason[7] != '.':
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
        return self.logs and self.logs[-1].abort

    def committed(self):
        return self.logs and self.logs[-1].commit

    def getEnd(self):
        return self.logs[-1]

alltrans = []
curtrans = dict()
cpuset = set()

for line in f.readlines():
    lst = line.rstrip().split(' ')
    pl = parseLine(lst)

    if pl.cpu in curtrans:
        trans = curtrans[pl.cpu]
    else:
        trans = Transaction(pl.cpu)

    trans.addLog(pl)
    cpuset.add(trans.cpu)

    if trans.completed():
        alltrans.append(trans)
        del curtrans[trans.cpu]
    else:
        curtrans[trans.cpu] = trans

assert(len(curtrans.keys()) == 0)

def accum_reasons(reasondict, trans):
    if not trans.getEnd().reason in reasondict:
        reasondict[trans.getEnd().reason] = 1
    else:
        reasondict[trans.getEnd().reason] += 1

    return reasondict


def printStats(lst):
    aborted = filter(lambda x: x.aborted(), lst)
    committed = filter(lambda x: x.committed(), lst)
    abort_reasons = reduce(accum_reasons, aborted, dict())
    print "Transactions/Committed/Aborts: %d/%d (%.2f%%)/%d (%.2f%%)" % (len(lst), 
                len(committed), 100.0*len(committed)/len(lst),
                len(aborted), 100.0*len(aborted)/len(lst))

    total = (1.0 * reduce(lambda x,y: x + len(y.logs), lst, 0)) 
    print "Average Instructions/TXN: %.5f (%d total)" % (total / len(lst), total) 

    if committed: 
        total = (1.0 * reduce(lambda x,y: x + len(y.logs), committed, 0))
        print "Average Instructions/Commit: %.5f (%d total)" % (total / len(committed), total)
        clines = (1.0 * reduce(lambda x,y: x + y.getEnd().cachelines, committed, 0))
        maxlines = reduce(lambda x,y: max(x, y.getEnd().cachelines), committed, 0)
        print "Average Lines/Commit: %.5f" % (clines / len(committed))
        print "Max Lines/Commit: %.5f" % (maxlines)
    if aborted:
        total = (1.0 * reduce(lambda x,y: x + len(y.logs), aborted, 0))
        print "Average Instructions/Abort: %.5f (%d total)" % (total / len(aborted), total)
    if abort_reasons:
        print "Abort reasons:"
        for reason, count in abort_reasons.iteritems():
            print "    %10s: %4d (%.2f%%)" % (reason, count, 100.0*count/len(aborted))


print "ALL TRANSACTIONS:"
printStats(alltrans)
for cpu in cpuset:
    cpulst = filter(lambda x: x.cpu == cpu, alltrans)
    print "TRANSACTIONS FOR CPU", cpu, ":"
    printStats(cpulst)
        



