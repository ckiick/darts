
darts:	darts-nd
	cp darts-nd darts

debug:	darts-dbg darts-dbg.64

nondebug:	darts-nd darts-nd.64

darts-nd:	darts.c
	gcc -O5 darts.c ./bar/bar.c -o darts-nd -I./bar -lrt

darts-nd.64:	darts.c
	gcc -m64 -O5 darts.c ./bar/bar.c -o darts-nd.64 -I./bar -lrt

darts-dbg:		darts.c
	gcc -DDEBUG=1 -g -ggdb darts.c ./bar/bar.c -o darts-dbg -I./bar -lrt

darts-dbg.64:		darts.c
	gcc -m64 -DDEBUG=1 -g -ggdb darts.c ./bar/bar.c -o darts-dbg.64 -I./bar -lrt

all:	debug	nondebug

darts.prof:	darts.c
	gcc -ggdb -g -pg -fprofile-arcs -ftest-coverage -I./bar darts.c ./bar/bar.c -o darts.prof -lrt

prof:	darts.prof
	rm -f gmon.out mon.out darts.gc*
	./darts.prof 3 6
	gcov darts.c ./bar/bar.c
	ggprof -b -c ./darts.prof >> gprof.out
	ggprof -b -l ./darts.prof >> gprof.out
	head -20 gprof.out
#	gperf record ./darts.prof 3 6
#	gperf report ./darts.prof > gperf.out
#	gperf annotate ./darts.prof >> gperf.out

clean:
	rm -f gmon.out gperf.out gprof.out *.gcda *.gcno perf.data
	rm -f perf.data *.gcov darts.prof core

