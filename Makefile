cc-option = $(shell if $(CC) $(CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
	     > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CXX := g++
CXXFLAGS := -g $(call cc-option,-std=c++11,-std=c++0x) -fwhole-program -Wall

ana: $(wildcard *.cc) $(wildcard *.h) Makefile
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cc, $^)
