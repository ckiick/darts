
ifdef DEBUG
	OPTS = -g -DDEBUG -Wall
else
	OPTS = -O4
endif

bartest:	bartest.c bar.c
	gcc $(OPTS) bartest.c bar.c -o bartest

bartest64:	bartest.c bar.c
	gcc -m64 $(OPTS) bartest.c bar.c -o bartest64

test:	bartest	bartest64
	./bartest
	./bartest64

clean:
	rm -rf core bartest bartest64 *.o
