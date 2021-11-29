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

pyconfig = $(shell python3 -c \
		'from distutils import sysconfig; print(sysconfig.$(1))')

CXXFLAGS_PYTHON := -DANA_AS_PYMODULE \
	-I $(call pyconfig, get_python_inc(False)) \
	-I $(call pyconfig, get_python_inc(True)) \
	-fPIC

LFLAGS_PYTHON := \
	-shared \
	$(call pyconfig, get_config_var("LIBS"))

CXX := g++
CXXFLAGS := -g -std=c++11 -Wall
EXTRAFLAGS ?=

.PHONY: all
all: ana python dict.bin

.PHONY: python
python: ana.so

ana.js:  $(wildcard *.cc) $(wildcard *.h) Makefile words
	em++ -flto -O3 -g --bind -std=c++11 -s TOTAL_MEMORY=33554432  --preload-file words -DANA_AS_JS run.cc -o ana.js

words:
	LANG=C.UTF-8 grep '^[a-z]*$$' /usr/share/dict/words > $@

dict.bin: words ana
	./ana -D $@ -d $<

ana: $(wildcard *.cc) | $(wildcard *.h) Makefile
	$(CXX) $(CXXFLAGS) $(EXTRAFLAGS) -flto -o $@ $^
ana.so: $(wildcard *.cc) | $(wildcard *.h) Makefile
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_PYTHON) -o $@ $^ $(LFLAGS_PYTHON)

publish: ana.js ana.html ana.wasm ana.data ana.js.mem ana.data
	git branch -D gh-pages || true
	./import $(filter-out ana.html,$^) index.html=ana.html | git fast-import --date-format=now

.PHONY: clean
clean:
	rm -f ana ana.so dict.bin words ana.js
