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

ana: $(wildcard *.cc) $(wildcard *.h) Makefile
	$(CXX) $(CXXFLAGS) -fwhole-program -o $@ $(filter %.cc, $^)
anamodule.so: $(wildcard *.cc) $(wildcard *.h) Makefile
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_PYTHON) -o $@ $(filter %.cc, $^) \
		$(LFLAGS_PYTHON)
