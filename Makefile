


darts:	darts.c
	gcc -O5 darts.c ./bar/bar.c -o darts -I./bar

debug:
	gcc -g -ggdb darts.c ./bar/bar.c -o darts -I./bar

prof:	darts
	rm -f gmon.out mon.out darts.gc*
	gcc -ggdb -g -pg -fprofile-arcs -ftest-coverage -I./bar darts.c ./bar/bar.c -o darts
	./darts 3 8
	gcov darts.c ./bar/bar.c
	gprof -b -c ./darts >> gprof.out
	gprof -b -l ./darts >> gprof.out
	head -20 gprof.out
	perf record ./darts 3 8
	perf report ./darts > gperf.out
	perf annotate ./darts >> gperf.out

clean:
	rm -f gmon.out gperf.out gprof.out *.gcda *.gcno perf.data
	rm -f perf.data *.gcov

