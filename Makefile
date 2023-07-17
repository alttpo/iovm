CSTANDARD = -std=gnu99

CFLAGS := -g
CFLAGS += -Wall -Wstrict-prototypes -Werror -Wno-strict-aliasing
CFLAGS += $(CSTANDARD)
CFLAGS += -ffunction-sections -fdata-sections

all: a.out
	./a.out

a.out: test.o iovm.o
	$(CC) $(CFLAGS) test.o iovm.o

test.o: test.c iovm.h
	$(CC) $(CFLAGS) -c test.c

iovm.o: iovm.c iovm.h
	$(CC) $(CFLAGS) -c iovm.c

clean:
	$(RM) a.out test.o iovm.o
