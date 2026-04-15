CC = gcc
CXX = g++
CXXFLAGS ?= -Wall -Wextra -Wno-unused-parameter

all: ../bin/palantir

../bin/palantir: palantir.o disassembler.o statica.o cfg.o statica.o x86.o cfg.hpp statica.hpp x86.hpp disassembler.hpp
	$(CXX) $(CXXFLAGS) -o ../bin/palantir palantir.o disassembler.o cfg.o statica.o x86.o -lelf -lZydis

palantir.o: palantir.cpp cfg.hpp statica.hpp disassembler.hpp
	$(CXX) $(CXXFLAGS) -c -o palantir.o palantir.cpp

disassembler.o: disassembler.cpp cfg.hpp statica.hpp x86.hpp disassembler.hpp
	$(CXX) $(CXXFLAGS) -c -o disassembler.o disassembler.cpp

cfg.o: cfg.cpp cfg.hpp x86.hpp
	$(CXX) $(CXXFLAGS) -c -o cfg.o cfg.cpp

statica.o: statica.cpp statica.hpp cfg.hpp x86.hpp
	$(CXX) $(CXXFLAGS) -c -o statica.o statica.cpp

x86.o: x86.cpp x86.hpp
	$(CXX) $(CXXFLAGS) -c -o x86.o x86.cpp

debug: CXXFLAGS += -g -DDEBUG
debug: all

clean:
	rm -f *.o ../bin/palantir
