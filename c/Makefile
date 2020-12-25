.PHONY: clean get install tests
PREFIX?=/usr/local
EXE:=typing_c
DEPS:=$(wildcard *.c) $(wildcard *.h)

$(EXE): $(DEPS) tags
	gcc -g -O0 -D_GNU_SOURCE -DEXE_NAME='"$(EXE)"' -DGIT_REF_STR='"$(shell git rev-parse HEAD)"' -std=c11 -Wall $(filter %.c,$^) -lm -lcurses -lsqlite3 -lunistring -o $@

tests: test_of test_xkb
	@echo TEST: sz_mult.c
	./test_of
	@echo TEST: layout.c
	./test_xkb

test_of: sz_mult.c sz_mult.h
	gcc -g -O2 -D_GNU_SOURCE -D_TEST_SZMULT -std=c11 -Wall $< -o $@

test_xkb: layout.c layout.h
	gcc -g -O2 -D_GNU_SOURCE -D_TEST_XKB -std=c11 -Wall $< -o $@

get:
	sudo apt install libncurses-dev libsqlite3-dev ctags cscope libunistring-dev

tags: $(DEPS)
	@rm -f tags cscope.out
	-ctags -R -f $@ .
	-cscope -R -b -f cscope.out

cscope.out: $(DEPS)
	@rm -f $@
	cscope -R -b -f $@

clean:
	rm -fv test_of test_xkb typing_c

install: $(EXE)
	sudo cp ./$(EXE) $(PREFIX)/bin/$(EXE)

