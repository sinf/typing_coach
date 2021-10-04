.PHONY: clean build_env install tests tags all
.SUFFIXES:

PREFIX?=/usr/local
VPATH:=.:./util
MAIN_DEP:=cpm_counter.c database.c debug.c dpy.c filepath.c ini.c kseq.c kblayout.c main.c mainloop.c menu.c microsec.c murmur.c persist.c prog_util.c session_timer.c spambox.c sz_mult.c tm1.c tm2.c tm.c wordlist.c
MAIN_OBJ:=$(patsubst %.c,obj/%.o,$(MAIN_DEP))
MAIN_EXE:=typing_c
TESTBIN:=$(addprefix test/,layout sz_mult)

CF_COMMON:=-g -O0 -D_GNU_SOURCE -DGIT_REF_STR='"$(shell git rev-parse HEAD)"' -std=c11 -Wall -I. -I./util
CF_OPT:=-g -O0

all: $(MAIN_EXE) ;

$(MAIN_EXE): $(MAIN_OBJ)
	gcc $(CF_OPT) $(CF_COMMON) $^ $(shell pkg-config --libs ncursesw) -lm -lsqlite3 -lunistring -MMD -MP -MF dep/$(*F).d -MT '$$(MAIN_EXE)' -o $@

obj/%.o: %.c
	gcc $(CF_OPT) $(CF_COMMON) $(shell pkg-config --cflags ncursesw) -MMD -MP -MF dep/$(*F).d -MT $@ -c $< -o $@

tests: $(TESTBIN)
	for b in $^; do ./$$b > $$b.out 2>&1; echo "$$b: $$?"; done

test/sz_mult: test/sz_mult_test.c sz_mult.h
	gcc -g -O0 $(CF_COMMON) $< -o $@

test/layout: test/layout_test.c kblayout.h
	gcc -g -O0 $(CF_COMMON) $< -o $@

build_env:
	sudo apt install libncursesw5-dev libsqlite3-dev ctags cscope libunistring-dev

tags:
	@rm -f tags cscope.out
	-ctags -R -f $@ .
	-cscope -R -b -f cscope.out

clean:
	rm -fv test_of test_xkb $(MAIN_EXE) obj/*.o dep/*.d $(TESTBIN) $(addsuffix .out,$(TESTBIN))

install: $(MAIN_EXE)
	mkdir -p $(PREFIX)/bin
	cp ./$(MAIN_EXE) $(PREFIX)/bin/$(MAIN_EXE)

-include $(wildcard dep/*.d)

