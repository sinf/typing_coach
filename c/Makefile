.PHONY: clean get

typing_c: $(wildcard *.c) $(wildcard *.h)
	gcc -g -O2 -D_GNU_SOURCE -std=c11 -Wall $(wildcard *.c) -lm -lcurses -lsqlite3 -o $@

test_of: sz_mult.c
	gcc -g -O2 -D_GNU_SOURCE -D_TEST_SZMULT -std=c11 -Wall $^ -o $@

get:
	sudo apt install libncurses-dev libsqlite3-dev

clean:
	rm -f test_of typing_c

