all: patcher

patcher: patcher.c ../analysis/x86_register_state.h
	$(E9ROOT)/e9compile.sh patcher.c

clean:
	rm -f *.o patcher
