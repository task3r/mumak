CC = gcc
CXX = g++
CXXFLAGS ?= -Wall -Wextra -Wno-unused-parameter

all: ../bin/mumak_oftp ../bin/trace_parser ../bin/reporter ../bin/pifr_oftp

../bin/mumak_oftp: mumak_oftp.cpp tracing.hpp
	$(CXX) $(CXXFLAGS) -o ../bin/mumak_oftp mumak_oftp.cpp

../bin/trace_parser: trace_parser.o trace.o tracing.hpp
	$(CXX) $(CXXFLAGS) -o ../bin/trace_parser trace_parser.o trace.o

trace_parser.o: trace_parser.cpp tracing.hpp
	$(CXX) $(CXXFLAGS) -c -o trace_parser.o trace_parser.cpp

trace.o: tracing.cpp tracing.hpp
	$(CXX) $(CXXFLAGS) -c -o trace.o tracing.cpp

../bin/reporter: reporter.o
	$(CXX) $(CXXFLAGS) -o ../bin/reporter reporter.o

reporter.o: reporter.cpp
	$(CXX) $(CXXFLAGS) -c -o reporter.o reporter.cpp

../bin/pifr_oftp: pifr_oftp.cpp
	$(CXX) $(CXXFLAGS) -g -c pifr_oftp.cpp -o pifr_oftp.o
	$(CXX) $(CXXFLAGS) -g -o ../bin/pifr_oftp pifr_oftp.o ../runtime/tbb_backend.o -ltbb -pthread

debug: CXXFLAGS += -g -DDEBUG
debug: all

clean:
	rm -f *.o ../bin/mumak_oftp ../bin/trace_parser ../bin/reporter ../bin/pifr_oftp
