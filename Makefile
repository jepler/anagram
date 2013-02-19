ana: $(wildcard *.cc) $(wildcard *.h)
	g++ -std=c++11 -fwhole-program -Wall $(filter %.cc, $^) -o $@
