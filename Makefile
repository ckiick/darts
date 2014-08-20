


darts:	darts.c
	gcc -O5 darts.c ./bar/bar.c -o darts -I./bar

debug:
	gcc -DDEBUG=1 -g -ggdb darts.c ./bar/bar.c -o darts -I./bar

darts.prof:	darts.c
	gcc -ggdb -g -pg -fprofile-arcs -ftest-coverage -I./bar darts.c ./bar/bar.c -o darts.prof

prof:	darts.prof
	rm -f gmon.out mon.out darts.gc*
	./darts.prof 3 6
	gcov darts.c ./bar/bar.c
	ggprof -b -c ./darts.prof >> gprof.out
	ggprof -b -l ./darts.prof >> gprof.out
	head -20 gprof.out
	perf record ./darts.prof 3 6
	perf report ./darts.prof > gperf.out
	perf annotate ./darts.prof >> gperf.out

clean:
	rm -f gmon.out gperf.out gprof.out *.gcda *.gcno perf.data
	rm -f perf.data *.gcov darts.prof core

