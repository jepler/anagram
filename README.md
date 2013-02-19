# Build instructions:

    make

# General running:

    ./ana terms...
    ./ana -? # for help

# Binary dictionaries
These start a bit faster than reading the system dictionary.

Build one with

    ./ana -D dict.bin -d /usr/share/dict/words

then use it with

    ./ana -D dict.bin terms...

# Web app installation instructions
 1.    build a binary dictionary:

        ./ana -D dict.bin -d /usr/share/dict/words

 1.    Copy ana, the binary dictionary, anagram.js and webapp.py to your
 webspace

 1.    Set up .htaccess, e.g.,

        AddHandler fcgid-script .fcgi
        Options +ExecCgi
        DirectoryIndex index.fcgi
        Order allow,deny
        Allow from all

# Live web version
http://unpy.net/anagram/
