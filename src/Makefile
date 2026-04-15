CC = gcc
CXX = g++
CXXFLAGS = -Wall -Wextra -Wno-unused-parameter
PIN_DEBUG = 0

.PHONY: runtime analysis patching utils all clean clean-inst clean-all debug vardalith

all: instrumentation_build analysis patching runtime utils vardalith

instrumentation_build: instrumentation/*
	make -C instrumentation DEBUG=$(PIN_DEBUG)

analysis:
	make -C analysis CXXFLAGS="$(CXXFLAGS)"

runtime:
	make -C runtime CXXFLAGS="$(CXXFLAGS)"

patching:
	make -C patching

utils: runtime
	make -C utils CXXFLAGS="$(CXXFLAGS)"

vardalith:
	cargo build --manifest-path=vardalith/Cargo.toml

debug: CXXFLAGS += -g -DDEBUG
debug: PIN_DEBUG = 1
debug: all

clean:
	make -C analysis clean
	make -C runtime clean
	make -C patching clean
	make -C utils clean
clean-inst:
	make -C instrumentation clean
clean-vardalith:
	cargo clean --manifest-path=vardalith/Cargo.toml
clean-all: clean clean-inst clean-vardalith
