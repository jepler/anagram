#!/usr/bin/python
# -*- coding: utf-8 -*-
# 
# Copyright © 2013 Jeff Epler <jepler@unpythonic.net>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 
import subprocess
import threading
import sys
import os
import cgi

ana = os.path.abspath(os.path.join(os.path.dirname(__file__), "ana"))
if not os.path.exists(ana): raise SystemExit, "%s does not exist" % ana

class LocalPipe(threading.local):
    def __init__(self):
        self.pipe = subprocess.Popen([ana, "-D", "dict.bin", "-s"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=1)

pipes = LocalPipe()
def get_pipe_for_thread():
    return pipes.pipe
       
# Every WSGI application must have an application object - a callable
# object that accepts two arguments. For that purpose, we're going to
# use a function (note that you're not limited to a function, you can
# use a class for example). The first argument passed to the function
# is a dictionary containing CGI-style envrironment variables and the
# second variable is the callable object (see PEP 333).
def anagram_app(environ, start_response):
    status = '200 OK' # HTTP Status

    pi = environ['QUERY_STRING']
    if pi: pi = cgi.parse_qs(pi).get('q', [''])[0]

    headers = [('Content-type', 'text/html')] # HTTP Headers
    start_response(status, headers)

    yield "<html><head><title>Surly anagram server</title></head>"

    yield "<div style='float:right'>"
    yield "<p>Cheatsheet:</p>"
    yield "<dl>"
    yield "<dt>letters... <dd> Letters available to anagram\n"
    yield "<dt>=word <dd> word must be in result\n"
    yield "<dt>&gt;n <dd> words must contain at least n letters\n"
    yield "<dt>&lt;n <dd> words must contain at most n letters\n"
    yield "<dt>' <dd> words with apostrophes are considered\n"
    yield "<dt>n <dd> choose a word with exactly n letters\n"
    yield "<dt>-n <dd> display at most n results (limit 1000)\n"
    yield "</dl>"
    yield "<p>Source (web app and unix commandline program) on"
    yield " <a href='https://github.com/jepler/anagram'>github</a></p>"
    yield "</div>"
        
    yield "<h1>Array unravels germs</h1>"
    yield "<body><form><input type='text' id='query' name='q' value=\"%s\">" % cgi.escape(pi, True)
    yield "<input type='submit' value='anagram'>"
    yield "<script>document.getElementById('query').focus()</script>"
    yield "</form>"

    if pi:
        pi = pi.replace('\n', ' ')
        pipe = get_pipe_for_thread()

        yield "<pre>"
        yield "# Query: " + repr(pi) + "\n"

        pipe.stdin.write(pi + "\n"); pipe.stdin.flush()
        while 1:
            line = pipe.stdout.readline()
            if not line.strip():
                    break
            yield cgi.escape(line)
        yield "</pre> "

    yield "</body></html>"

from flup.server.fcgi import WSGIServer
WSGIServer(anagram_app).run()
