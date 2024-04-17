#
# Students' Makefile for the Malloc Lab
#

CC = gcc
CFLAGS = -Wall -m32 -g -c

all: explicit implicit explicitTester implicitTester

OBJS = memlib.o fsecs.o fcyc.o clock.o ftimer.o

implicit: $(OBJS) mmImplicit.o driver.c
	$(CC) $(CFLAGS) -DIMPLICIT driver.c -o driver.o
	$(CC) -m32 $(OBJS) mmImplicit.o driver.o -o implicit 

explicit: $(OBJS) mmExplicit.o driver.c
	$(CC) $(CFLAGS) -DEXPLICIT driver.c -o driver.o
	$(CC) -m32 $(OBJS) mmExplicit.o driver.o -o explicit 

explicitTester: mmExplicit.o explicitTester.o memlib.o
	$(CC) -m32 mmExplicit.o explicitTester.o memlib.o -o explicitTester

implicitTester: mmImplicit.o implicitTester.o memlib.o
	$(CC) -m32 mmImplicit.o implicitTester.o memlib.o -o implicitTester

explicitTester.o: explicitTester.c mmExplicit.h
	$(CC) $(CFLAGS) -Wno-unused explicitTester.c -o explicitTester.o

implicitTester.o: implicitTester.c mmImplicit.h
	$(CC) $(CFLAGS) -Wno-unused implicitTester.c -o implicitTester.o

memlib.o: memlib.c memlib.h config.h

mmImplicit.o: mmImplicit.c mmImplicit.h memlib.h

mmExplicit.o: mmExplicit.c mmExplicit.h memlib.h

fsecs.o: fsecs.c fsecs.h fcyc.h clock.h ftimer.h config.h

fcyc.o: fcyc.c fcyc.h clock.h

ftimer.o: ftimer.c ftimer.h

clock.o: clock.c clock.h

clean:
	rm -f *~ *.o implicit explicit implicitTester explicitTester


