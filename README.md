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

# Python use
```
$ python3
>>> import ana
>>> d = ana.from_binary("dict.bin")
>>> for row in d.run("hello world"):
...    print(row)
```

# Web Browser Version (WASM/js)

 1. Compile with emscripten (tested with the version in debian bullseye):

        make ana.js

 1. Test it to your satisfaction using a local html server:

        python3 http.server &
 
 1. Commit it:

        make publish

 1. Push it to github:

        git push origin gh-pages

# Live web version
https://www.unpythonic.net/anagram/
