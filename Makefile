test: lerl lerl.lr test.exp
	./lerl ./lerl.lr ./lerl.lr> test.out
	diff test.out test.exp

lerl: lerl.c
	gcc -g -Wall -std=c99 $< -o $@

.PHONY:test

