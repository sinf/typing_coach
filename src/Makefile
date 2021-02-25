.PHONY: clean build_env install tests tags
PREFIX?=/usr/local
EXE:=typing_c
DEPS:=$(wildcard *.c) $(wildcard *.h)

MAIN_DEP:=cpm_counter.c database.c debug.c dpy.c filepath.c ini.c kseq.c layout.c main.c mainloop.c menu.c microsec.c murmur.c persist.c prog_util.c session_timer.c spambox.c sz_mult.c tm1.c tm.c wordlist.c

$(EXE): $(patsubst %.c,obj/%.o,$(MAIN_DEP))
	gcc -g -O0 -D_GNU_SOURCE -DGIT_REF_STR='"$(shell git rev-parse HEAD)"' -std=c11 -Wall $^ $(shell pkg-config --libs ncursesw) -lm -lsqlite3 -lunistring -MMD -MP -MF dep/$(*F).d -MT '$$(EXE)' -o $@

obj/%.o: %.c
	gcc -g -O0 -D_GNU_SOURCE -DGIT_REF_STR='"$(shell git rev-parse HEAD)"' -std=c11 -Wall $(shell pkg-config --cflags ncursesw) -MMD -MP -MF dep/$(*F).d -MT $@ -c $< -o $@

TESTBIN:=$(addprefix test/,layout sz_mult)
tests: $(TESTBIN)
	for b in $^; do ./$$b > $$b.out 2>&1; echo "$$b: $$?"; done

test/sz_mult: test/sz_mult_test.c sz_mult.h
	gcc -I. -g -O0 -D_GNU_SOURCE -std=c11 -Wall $< -o $@

test/layout: test/layout_test.c layout.h
	gcc -I. -g -O0 -D_GNU_SOURCE -std=c11 -Wall $< -o $@

build_env:
	sudo apt install libncursesw5-dev libsqlite3-dev ctags cscope libunistring-dev

tags:
	@rm -f tags cscope.out
	-ctags -R -f $@ .
	-cscope -R -b -f cscope.out

clean:
	rm -fv test_of test_xkb $(EXE) obj/*.o obj/*.d *.d $(TESTBIN) $(addsuffix .out,$(TESTBIN))

install: $(EXE)
	sudo cp ./$(EXE) $(PREFIX)/bin/$(EXE)

-include $(wildcard dep/*.d)

