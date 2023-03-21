CSTANDARD = -std=gnu99

CFLAGS := -g
CFLAGS += -Wall -Wstrict-prototypes -Werror -Wno-strict-aliasing
CFLAGS += $(CSTANDARD)
CFLAGS += -ffunction-sections -fdata-sections

all: a.out
	./a.out

a.out: iovm.o test.o
	$(CC) $(CFLAGS) test.o iovm.o

iovm.o: iovm.c iovm.h
	$(CC) $(CFLAGS) -c iovm.c

test.o: test.c iovm.h
	$(CC) $(CFLAGS) -c test.c

clean:
	$(RM) a.out test.o iovm.o
