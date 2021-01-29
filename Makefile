test: lerl lerl.lr test.exp
	./lerl ./lerl.lr > test.out
	diff test.out test.exp

lerl: lerl.c
	gcc -Wall -std=c99 $< -o $@

.PHONY:test

