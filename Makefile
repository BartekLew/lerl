test: lerl lerl.lrc test.exp
	./lerl ./ex.lr> test.out
	diff test.out test.exp

lerl: lerl.c
	gcc -g -Wall -std=c99 $< -o $@

.PHONY:test

