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

cc-option = $(shell if $(CC) $(CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
	     > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)
pyconfig = $(shell python -c \
		'from distutils import sysconfig; print sysconfig.$(1)')

CXXFLAGS_PYTHON := -DANA_AS_PYMODULE \
	-I $(call pyconfig, get_python_inc(False)) \
	-I $(call pyconfig, get_python_inc(True)) \
	-fPIC

LFLAGS_PYTHON := \
	-shared \
	$(call pyconfig, get_config_var("LIBS"))

CXX := g++
CXXFLAGS := -g $(call cc-option,-std=c++11,-std=c++0x) -Wall

all: ana anamodule.so

ana.js:  $(wildcard *.cc) $(wildcard *.h) Makefile words
	em++ -Os --bind -std=c++11 -s TOTAL_MEMORY=33554432  --preload-file words -DANA_AS_JS run.cc -o ana.js

words:
	grep '^[a-z]*$$' /usr/share/dict/words > $@

ana: $(wildcard *.cc) $(wildcard *.h) Makefile
	$(CXX) $(CXXFLAGS) -fwhole-program -o $@ $(filter %.cc, $^)
anamodule.so: $(wildcard *.cc) $(wildcard *.h) Makefile
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_PYTHON) -o $@ $(filter %.cc, $^) \
		$(LFLAGS_PYTHON)

publish: ana.js ana.html ana.js.mem ana.data
	git branch -D gh-pages || true
	./import $(filter-out ana.html,$^) index.html=ana.html | git fast-import --date-format=now
