CXX = g++ -O3 -Wall -std=c++11
MAIN_BINARIES = $(basename $(wildcard *_main.cpp))
HEADER = $(wildcard *.h)
OBJECTS = $(addsuffix .o, $(basename $(filter-out %_main.cpp, $(wildcard *.cpp))))
CPPLINT_PATH = cpplint.py
CPPLINT_FILTERS = -runtime/references,-build/header_guard,-build/include

.PRECIOUS: %.o

all: compile checkstyle

compile: $(MAIN_BINARIES)

checkstyle:
	@# Filter header_guards and include check, doesn't work well for
	@# projects roots that are not a svn or git root.
	@# Allow non-const references.
	python3 $(CPPLINT_PATH) --filter='$(CPPLINT_FILTERS)' *.h *.hpp *.cpp

clean:
	rm -f *.o
	rm -f $(MAIN_BINARIES)
	rm -f *.class
	rm -f core

%_main: %_main.o $(OBJECTS)
	$(CXX) -o $@ $^

%.o: %.cpp $(HEADER)
	$(CXX) -c $<
