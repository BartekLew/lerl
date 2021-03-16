test: lerl lerl.lrc test.exp
	./lerl ./ex.lr> test.out
	diff test.out test.exp

lerl.lrc.res: lerl.lrc
	objcopy --input binary --output elf64-x86-64\
	        --binary-architecture i386:x86-64\
	        lerl.lrc lerl.lrc.res

lerl: lerl.c lerl.lrc.res
	gcc -g -Wall -std=c99 $< lerl.lrc.res -o $@

.PHONY:test

