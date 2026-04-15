CC = gcc
CXX = g++
CXXFLAGS ?= -Wall -Wextra -Wno-unused-parameter
ADDITIONAL_FLAGS =  -Wno-unused-function -fPIC -Wno-missing-field-initializers

all: libpifrrt.so mmap_wrapper.so

libpifrrt.so: runtime.cpp runtime.hpp backend.hpp tbb_backend.hpp tbb_backend.o
	$(CXX) $(CXXFLAGS) $(ADDITIONAL_FLAGS) -c runtime.cpp -o runtime.o
	$(CXX) $(CXXFLAGS) $(ADDITIONAL_FLAGS) -shared -o libpifrrt.so runtime.o tbb_backend.o

tbb_backend.o: tbb_backend.cpp tbb_backend.hpp backend.hpp
	$(CXX) $(CXXFLAGS) $(ADDITIONAL_FLAGS) -c tbb_backend.cpp -o tbb_backend.o

mmap_wrapper.so: mmap_wrapper.c ../utils/cvector.h
	$(CC) -Wall -fPIC -DPIC -c mmap_wrapper.c
	ld -shared -o mmap_wrapper.so mmap_wrapper.o -ldl

debug: CXXFLAGS += -g -DDEBUG
debug: all

clean:
	rm -f *.o *.so
