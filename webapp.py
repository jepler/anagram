from flup.server.fcgi import WSGIServer
import subprocess
import threading
import sys

class LocalPipe(threading.local):
    def __init__(self):
        self.pipe = subprocess.Popen(["./ana", "-D", "dict.bin", "-s"],
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

    pi = environ['PATH_INFO']

    if pi:

        headers = [('Content-type', 'text/plain')] # HTTP Headers
        start_response(status, headers)

        pipe = get_pipe_for_thread()

        pipe.stdin.write(pi + "\n"); pipe.stdin.flush()
        while 1:
            line = pipe.stdout.readline()
            if not line.strip():
                    break
            yield line
    else:
        headers = [('Content-type', 'text/plain')] # HTTP Headers
        yield "No query."

WSGIServer(anagram_app).run()
