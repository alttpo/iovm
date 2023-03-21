all: a.out
	./a.out

a.out: iovm.o test.o
	gcc test.o iovm.o

iovm.o: iovm.c iovm.h
	gcc -c iovm.c

test.o: test.c iovm.h
	gcc -c test.c
