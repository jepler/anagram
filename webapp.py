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
import traceback
import errno
import ana

d = ana.from_binary('dict.bin')

def query(pi):
    return d.run(pi)

       
# Every WSGI application must have an application object - a callable
# object that accepts two arguments. For that purpose, we're going to
# use a function (note that you're not limited to a function, you can
# use a class for example). The first argument passed to the function
# is a dictionary containing CGI-style envrironment variables and the
# second variable is the callable object (see PEP 333).
def anagram_app(environ, start_response):
    status = '200 OK' # HTTP Status

    pi = environ['QUERY_STRING']
    pi = cgi.parse_qs(pi)
    plain = pi.get('p', False)
    if pi:
        pi = pi.get('q', [''])[-1]
    else:
        pi = ''

    if plain:
        headers = [('Content-type', 'application/octet-stream')] # HTTP Headers
    else:
        headers = [('Content-type', 'text/html')] # HTTP Headers
    start_response(status, headers)

    if not plain:
        yield '''<!DOCTYPE html>
<html>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width">
<head>
<title>Surly Anagram Server</title>
<style>
@media screen and (min-width: 680px) { #cheatsheet { float: right; } }
#cheatsheet { font-size: 71%%; cursor: pointer; }
#cheatsheet table, #cheatsheet caption { background: #d9d9d9; color: #000; }
#cheatsheet.hidden { cursor: zoom-in; }
#cheatsheet caption:after { content: "\\a0«" }
#cheatsheet.hidden caption:after { content: "\\a0»" }
#cheatsheet.hidden tbody { display: none; }
//#cheatsheet { float: right; }
#cheatsheet th { text-align: left }
#cheatsheet caption { font-weight: bold; }
</style>
</head>
<body>
<div id="cheatsheet">
<table>
<caption>Cheatsheet</caption>
<tr><th>letters... <td> Letters available to anagram
<tr><th>=word <td> word must be in result
<tr><th>&gt;n <td> words must contain at least n letters
<tr><th>&lt;n <td> words must contain at most n letters
<tr><th>' <td> words with apostrophes are considered
<tr><th>n <td> choose a word with exactly n letters
<tr><th>-n <td> display at most n results (limit 1000)
<tr><th>? <td> display candidate words, not whole phrases
<tr><td colspan=2>In ajax mode, hit enter or click "anagram" to do get full results
<tr><td colspan=2>Source (web app and unix commandline program) on
 <a href="https://github.com/jepler/anagram">github</a>
</table>
</div>
            
<form id="f"><input type="text" id="query" name="q" value="%s">
<input type="submit" value="anagram">
<script>document.getElementById("query").focus()</script>
</form>
<pre id="results">''' % cgi.escape(pi, True)

    if pi:
	if plain: e = lambda x: x
	else: e = cgi.escape
	yield '# Query: ' + e(repr(pi)) + '\n'

        for row in query(pi):
            yield e(row) + '\n'

    if not plain:
        yield '''
</pre>
<script src="http://code.jquery.com/jquery-1.9.1.min.js"></script>
<script src="anagram.js"></script>
<script>
$("#cheatsheet").click(function() { $(this).toggleClass("hidden"); })
</script>
</body></html>'''

from flup.server.fcgi import WSGIServer
WSGIServer(anagram_app).run()
