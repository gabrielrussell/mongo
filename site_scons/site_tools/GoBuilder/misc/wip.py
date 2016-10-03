#!/usr/bin/env python
import re

BEGIN = 1
IMPORT = 2
DESCENT = 3


class Lexer(object):
    def __init__(self, text):
        self._rx = re.compile(r'(import|".*?"|\(|\))')
        self._text = text

    def __iter__(self):
        for m in self._rx.finditer(self._text):
            yield m.group(1)


class RecursiveDescent(object):
    state = BEGIN

    def __init__(self, lexer):
        self._lexer = lexer

    def __iter__(self):
        for token in self._lexer:
            if self.state == BEGIN:
                if token != 'import':
                    # Beginning of the program most likely
                    raise StopIteration
                self.state = IMPORT
            elif self.state == IMPORT:
                if token == '(':
                    self.state = DESCENT
                else:
                    self.state = BEGIN
                    yield token
            elif self.state == DESCENT:
                if token == ')':
                    self.state = BEGIN
                else:
                    yield token


# for path in RecursiveDescent(Lexer(test)):
#     print(path)


def scannit():
    fname = 'example1.go'

    content = ""
    with open(fname, 'r') as content_file:
        content += content_file.read()

    # print "C:%s"%content

    # Handle multiline import statements
    m_import = re.compile(r'import\s*(\()\s*([^\(]*?)(\))|import\s*(\")(.*?)(\")', re.MULTILINE)
    m_build = re.compile(r'\/\/\s*\+build\s(.*)')
    # m = re.search(r'import\s*\([^\)]*\)',content,re.MULTILINE)

    # import (...)  : r'import\s*(\()(.*?)(\))'
    # import ".."   : r'import\s*(\")(.*?)(\")'
    # // +build ... : r'\/\/\s*\+build\s(.*)'
    # import\s*(\()\s*([^\(]*?)(\))|import\s*(\")(.*?)(\")

    # print m.group(0),":",m.group(1),":",m.group(2),":",m.group(3),":",m.group(4),":",m.group(5),":",m.group(6)

    for b in m_build.finditer(content):
        if b.group(1):
            print "BUILD:%s"%b.group(1)

    for m in m_import.finditer(content):
        # print m.group(0),":",m.group(1),":",m.group(2),":",m.group(3),":",m.group(4),":",m.group(5),":",m.group(6)
        if m.group(1) == '(':
            print "Import() ", " ".join(m.group(2).splitlines())
        else:
            print "Import \"\"", m.group(5)
            # print m.group(0),":",m.group(1),":",m.group(2),":",m.group(3),":",m.group(4),":",m.group(5),":",m.group(6)


def lexit():
    import shlex

    # fname = '/Users/bdbaddog/clients/mongodb/mongo-tools/vendor/src/github.com/3rf/mongo-lint/lint.go'
    fname = 'example1.go'

    content = ""
    with open(fname, 'r') as content_file:
        content += content_file.read()

    lexer = shlex.shlex(content)
    lexer.quotes = r'\'\"'
    lexer.wordchars += '.\\/'
    lexer.whitespace = ' \t\r'

    print 'TOKENS:'
    for token in lexer:
        print repr(token)


def recParse():
    fname = 'example1.go'
    content = ""
    with open(fname, 'r') as content_file:
        content += content_file.read()

    for path in RecursiveDescent(Lexer(content)):
        print(path)


def parseIt():
    fname = 'example1.go'
    content = ""
    with open(fname, 'r') as content_file:
        content += content_file.read()

    rx = re.compile(r'import\s+([^(]+?$|\([^)]+\))', re.MULTILINE)
    rx2 = re.compile(r'".*"', re.MULTILINE)

    for m in rx.finditer(content):
        imp = m.group(1)
        if imp[0] == '(':
            for m2 in rx2.finditer(imp):
                print("rx2:", m2.group(0))
        else:
            print("rx:", m.group(1))


if __name__ == "__main__":
    scannit()
    # lexit()
    # recParse()
    # parseIt()
