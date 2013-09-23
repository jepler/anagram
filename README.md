# Build instructions:

    make

# Use instructions:

    ./ana terms...
    man ./ana.1 ;# for help

# Binary dictionaries
These start a bit faster than reading the system dictionary.

Build one with

    ./ana -D dict.bin -d /usr/share/dict/words

then use it with

    ./ana -D dict.bin terms...

# Web app installation instructions
 1.    build a binary dictionary:

        ./ana -D dict.bin -d /usr/share/dict/words

 1.    Copy anamodule.so, the binary dictionary, anagram.js and webapp.py to
 your webspace (or put anamodule.so on your PYTHONPATH)

 1.    Optionally, edit webapp.py to use your own copy of jquery instead
 of the one from the code.jquery.com cdn

 1.    Set up .htaccess, e.g.,

        AddHandler fcgid-script .fcgi
        Options +ExecCgi
        DirectoryIndex index.fcgi
        Order allow,deny
        Allow from all

# Live web version
http://ana.unpythonic.net
