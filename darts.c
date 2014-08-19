
/*
 * solve the son of darts programming contest from al zimm.
 * inputs: D = number of darts.  R= number of regions.
 * outputs: vals[R] = values for all R regions.  score = smallest unatt-
 * ainable score for this solution, hopefully the max.
 * NOTE: you can miss the dartboard.
 * Usage:
 * darts [-vtchH:bDVql:O:] Darts Regions
 * -v : verbose. use multiple times to be more noisy.
 * -t : time ourselves
 * -c : track number of evals (counter)
 * -h : display hash marks (like a heartbeat)
 * -H interval : how many evals per hash mark (default is 1,000,000).
 * -b : show best scores as they are found (progress report)
 * -D val : set debug value manually.
 * -V : display version info
 * -q : quiet. Shhhhhhh....
 * -l limit : quit after limit evals.
 * -O file : write debug output to file instead of stdout.
 * -C interval: checkpoint, every interval evals. Also when interrupted.
 * -R string: use string to restart from checkpoint.
 */

#define VER	"0.8.5"	// using  bar

#ifdef __sparc
#include <sys/types.h>
#else
#include <inttypes.h>
#include <time.h>
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include "bar.h"

#define DBG_INIT	0x00000001
#define DBG_FILLIN	0x00000002
#define DBG_MRF		0x00000004
#define DBG_DUMPS	0x00000008
#define DBG_DUMPF	0x00000010
#define DBG_MAIN	0x00000020
#define DBG_DUMPV	0x00000040
#define DBG_FILLIN2	0x00000080
#define	DBG_DBG		0x00000200
#define	DBG_VER		0x00000400
#define	DBG_CTR		0x00000800
#define DBG_TIME	0x00001000
#define	DBG_PRO		0x00002000
#define	DBG_HASH	0x00004000
#define	DBG_NONE	0x80000000
#define DBG_ALL		0x7FFFFFFF

long	dbg = DBG_NONE;

#define DEBUG

#ifdef DEBUG
#define	DBG(f, pargs)			\
	if ((((f) & dbg)==(f)) ? (printf("%s:",__func__) + printf pargs) : 0)
#else
#define	DBG(f, pargs)			\
	if (0)
#endif

uint64_t	hashval = 1000000;
uint64_t	stop = 0;
char 		*outfile = NULL;
uint64_t	cpval = 20000000;
char		*cpstr = NULL;

#define MAXR	40
#define	MAXVAL	(12*1024)
#define	MAXD	6

int limits[6] = { 40, 40, 40, 30, 20, 10 };

typedef struct _sc {
	bar_t s;		/* bit array. */
//	uint8_t s[MAXVAL];
	int gap;		/* also score. first uncovered number */
	int maxval;		/* max val covered. */
} sc_t;

typedef struct _plane {
	sc_t sc[MAXD];		/* one set for each number of darts */
	int rval;		/* value of region for this plane. */
} plane_t;

typedef struct _block {
	plane_t pl[MAXR+1];	/* one for each number of regions. */
} block_t;

typedef struct _vals {
	int score;		/* where is the first gap? */
	int vals[MAXR+1];	/* value of each region. */
} vals_t;

vals_t vals;			/* current set of r values. */
vals_t besties[MAXD][MAXR+1];	/* best values for each r,d comb. */

/* global huge block of stuff */
block_t hbos;


/*
 * a few semi-helpful notes.
 * darts are 0-based. if there are n darts, they are numbered 0..n-1.
 * regions are also 0 based, r regions numberes 0 .. r-1.
 * regions and darts are printed using whole numbers 1..d or 1..n.
 * the values are 1-based.  There's an implicit 0-value region, but we
 * don't bother to track it.
 *
 * method - with 1 dart 1 region, there is only 1 value, and it has
 * to be 1, giving a gap at 2 (score).  This is the foundation case.
 * Incrementing the number of darts (n darts, 1 region) increments
 * the score.  Now we can calculate solutions for any value of d for
 * r=1.  Conversely, with 1 dart it is easy to add another region with
 * value v, by simply adding v to the set of numbers reached and checking 
 * for a change in the score/gap.   So we can contruct any d=1,r=m or
 * d=n,r=1.
 * For d=n,r=m, we can construct a solution that is a set of values for 
 * each region. A solution is a set of values, one for each region.
 * V[m] = {V(m)}= { V1...Vm}. This is mapped to a set B(n,m) of numbers that
 * can be reached with that combination of values and the given number of
 * darts. The score S is then the first "missing" number in the set B.
 * If we have the set {B(n-1,m)}, what happens when we add a dart? Imagine
 * a dartboard with darts in it already.  If we throw another dart we
 * get to add any of the values in {V} to our total (including the
 * case where we miss the board altogether).  Therefore we can construct
 * the set {B(n,m)} from {B(n-1,m)} by adding all v in {V} to the values in
 * {B(n-1,m)}.
 *       for v in {V}
 *		for b in {B}
 *			{B} += v+b
 * Moving from n-1 to n regions is harder, except in the case where
 * d=1.  Adding a region with value v to B(1,m-1) is simply adding
 * v to the set B. {B(1,m)} = {B(1,m-1)} + v;
 * For any d=n,r=m given the values for the regions V[m], we can construct the
 * set {B(n,m)} if we have {B{n-1,m)}.  Simply recursing downward on n
 * eventually gets us to the set {B(1,m)}.  This set can be found by
 * using {B(1,m-1)}, and recursing on this eventually gets us to the
 * foundation set {B(1,1)}, which we know already is simply {1}, with
 * a score of 2.  Note that finding a solution for n,m also yeilds solutions
 * for all d,r 1..n,1--m.
 * Finally, how do we know what to use for v when adding a region?
 * Adding a value greater than the first gap cannot effect the score,
 * so that is the upper limit.  If we order our darts by their value
 * then we can ignore any v <= V[r-1].  So the set of possible values
 * for v[m] ranges from v[m-1]+1 to S(n,m-1) inclusive.  Anchoring the
 * value of V[1] at 1 determines the complete set of possible values for all
 * V[m].
 * An algorithm A takes parameters D and R for the number of darts and
 * regions, respectively.  Starting with r=1, it determines the range
 * of values for V[r] and interates over them.  For each v, it recurses
 * upward to A(d,r+1) until r==R.  Over the range of v, the best score
 * is kept by saving the values in {V} to a optimal score set O which
 * has a set V for each pair of d,r.
 */

void
dumpscore(sc_t score)
{
	int i;
	printf("gap=%d:[max=%d] ", score.gap, score.maxval);
	for (i=0; i <= score.maxval +1; i++) {
		if (barget(&(score.s),i)) printf("%d ",i);
	}
//	printf("\n");
}

void
dumpframe(int r, int D)
{
	int d, i;
	plane_t *f;

	f = &(hbos.pl[r]);

//	printf("frame %d \n", r);
	for (d =0; d < D; d++) {
		printf("d=%d ", d+1);
		dumpscore(f->sc[d]);
		printf("\n");
	}

}

void
dumpvals(int R)
{
	int i;
	printf("score=%d:[r=%d] ", vals.score, R+1);
	for (i = 1; i < R; i++) {
		printf("%d, ", vals.vals[i]);
	}
	printf("%d\n", vals.vals[i]);
}

/* at depth r, use val to fill in the frame for r, for all 1...D darts
 * we assume all frames < r have been filled in already.
 * returns score (gap). val does not change on any frame.
 */
void
fillin(int d, int r, int val)
{
	sc_t *scores;
	sc_t *dsc;
	int i;
	int mv;
	int rv;

	DBG(DBG_FILLIN, ("filling in d=%d,r=%d with val %d\n", d+1, r+1, val));
	scores = &(hbos.pl[r].sc[d]);

	if (d==0) {
		if (r > 0) {
			dsc = &(hbos.pl[r-1].sc[d]);
			scores->gap = dsc->gap;
			barcpy(&(scores->s), &(dsc->s));
DBG(DBG_FILLIN2, ("copied d=%d r=%d ", d+1, r-1+1)) {
	dumpscore(*dsc);
	printf(" to ");
	dumpscore(*scores);
	printf("\n");
}
			barset(&(scores->s), val);
			scores->maxval=val;
			if (scores->gap == val) {
				scores->gap++;
			}
		}
		DBG(DBG_FILLIN, ("base for d=%d,r=%d,gap=%d\n", d+1,r+1, scores->gap));
	} else {
		/* otherwise, d > 1 */
		fillin(d-1, r, val);
/* what we want to do is copy, shift, or. */
		dsc = &(hbos.pl[r].sc[d-1]);
		barcpy(&(scores->s),&(dsc->s));
DBG(DBG_FILLIN2, ("copied d=%d r=%d ", d-1+1, r+1)) {
			dumpscore(*scores);
			printf("\n");
}
		barlsle(&(scores->s), &(scores->s), val);
		baror(&(scores->s), &(dsc->s), &(scores->s));
		scores->gap = dsc->gap;
		scores->maxval = dsc->maxval + val;
DBG(DBG_FILLIN2, ("added val %d ", val)) {
	dumpscore(*dsc);
	printf(" to make  ");
	dumpscore(*scores);
	printf("\n");
}
/*
		for (i = 0; i <= scores->maxval; i++) {
			if (dsc->s[i]) {
				scores->s[i+val]++;
			}
		}
*/
		if (r > 0) {
			/* now do the d,r-1 half. */
			dsc = &(hbos.pl[r-1].sc[d]);
			baror(&(scores->s), &(dsc->s), &(scores->s));
/*
			for (i = 0; i <= dsc->maxval; i++) {
				scores->s[i] += dsc->s[i];
			}
*/
		}
DBG(DBG_FILLIN2, ("summed with d=%d r=%d ", d+1,r-1+1)) {
	dumpscore(*dsc);
	printf(" and got ");
	dumpscore(*scores);
	printf("\n");
}
		/* now find the new gap. */
/*
		while (barget(&(scores->s), scores->gap)) {
			scores->gap++;
		}
*/
		rv = barfnz(&(scores->s), scores->gap - 2);
		if (rv == -1) {
if (scores->gap >= scores->s.numbits) printf("gapgrow\n");
			/* add a 0 at the end. */
			scores->gap = barlen(&(scores->s));
			barclr(&(scores->s), scores->gap);
			scores->gap++;
//			printf("OOPS! NO GAP\n");
//			exit(3);
		} else {
			scores->gap = rv + 1;
		}
		DBG(DBG_FILLIN, ("for d=%d,r=%d,maxval=%d gap=%d \n", d+1,r+1, scores->maxval, scores->gap));
	}
	/* update best scores. */
	if (scores->gap > besties[d][r].score) {
		besties[d][r] = vals;
		besties[d][r].score = scores->gap;
		DBG(DBG_PRO, ("new best for d=%d,r=%d is %d\n", d+1, r+1, scores->gap));
		DBG(DBG_DUMPV, ("\tvals ")) {
			dumpvals(r);
		}
	}
	DBG(DBG_DUMPS, ("scores for d=%d,r=%d ",d+1,r+1 )) {
		dumpscore(*scores);
		printf("\n");
	}
	DBG(DBG_FILLIN, ("filled in d=%d,r=%d with val %d\n", d+1, r+1, val));
}

/* massively recursive function. Well, maybe not so massive. */
void
mrf(int d, int r)
{
	sc_t *scores;
	int lv, uv, cv;

	DBG(DBG_MRF, ("-> called with d=%d r=%d\n", d+1, r+1));
	if ((d < 0) || (r < 0)) return;
	if ((d == 0) && (r == 0)) return;
	if (r >= limits[d]) return;

	if (r == 0) {
		lv = uv = 1;
	} else  {
		lv = hbos.pl[r-1].rval + 1;
		uv = hbos.pl[r-1].sc[d].gap;
	}

	DBG(DBG_MRF, ("  r=%d val from %d to %d\n", r+1, lv, uv));

	for (cv = lv; cv <= uv; cv++) {
		vals.vals[r] = cv;
		hbos.pl[r].rval = cv;
		fillin(d, r, cv);
		DBG(DBG_DUMPF, ("frame %d\n",r )) {
			dumpframe(r,d+1);
		}
		if (r+1 < limits[d]) {
			DBG(DBG_MRF, ("  recursing with d=%d,r=%d\n", d+1, r+1+1));
			mrf(d, r + 1);
		}
	}
	DBG(DBG_DUMPV, ("current vals ")) {
		dumpvals(r);
	}
	DBG(DBG_MRF, ("<- returning from d=%d,r=%d\n", d+1, r+1));
}

/* at the end, print out everything we found. */
void
dumpscores(int D)
{
	int d, r, i;

	printf("best scores found this run:\n");
	for (d=0; d < D; d++) {
		for (r = 0; r < limits[d]; r++) {
			printf("%d darts, %d regions: score=%d; vals=", d+1, r+1, besties[d][r].score);
			for (i=0; i <= r; i++) {
				printf("%d,", besties[d][r].vals[i]);
			}
			printf("\n");
		}
	}
}

void
initstuff(int D, int R)
{
	int i;

	DBG(DBG_INIT, ("zeroing out hbos %d bytes\n", sizeof(block_t)));
	bzero(&hbos, sizeof(block_t));
	DBG(DBG_INIT, ("zeroing out vals %d bytes\n", sizeof(vals_t)));
	bzero(&vals, sizeof(vals_t));
	DBG(DBG_INIT, ("zeroing out besties %d bytes\n", sizeof(besties)));
	bzero(besties, sizeof(besties));

	/* set the degenerate case up first.
	 * d=1, r=1 can only have a val of 1 and a score/gap of 2.
	 * also, there must always be a val=1.
	 */
	barset(&(hbos.pl[0].sc[0].s),1);
	hbos.pl[0].sc[0].gap = 2;
	hbos.pl[0].sc[0].maxval = 1;
	hbos.pl[0].rval = 1;

	DBG(DBG_DUMPF, ("init frame\n")) {
		dumpscore(hbos.pl[0].sc[0]);
		printf("\n");
	}
	for( i = 0; i < D; i++) {
		if (limits[i] > R) {
			limits[i] = R;
		}
	}
	/* might be good enough for now. */
	DBG(DBG_INIT, ("init complete\n"));
}


void
usage(char *me)
{
	fprintf(stderr, "usage: %s [-vqtcVhbD] [-H interval] [-O outfile]"
		" [-l limit] [-D val] Darts Regions\n", me);
	fprintf(stderr, "\t-v: verbose. use multiple time for more.\n"
		"\t-q: quiet. turns off verbose.\n"
		"\t-t: display execution time.\n"
		"\t-c: show eval counts\n"
		"\t-V: display version number.\n"
		"\t-h: show hash mark every interval evals.\n"
		"\t-H interval: st the hash inteval (default 1,000,000)\n"
		"\t-b: show best answers as found (progress report)\n"
		"\t-D value: set dbg value directly.\n"
		"\t-O file: direct output to file instead of stdout\n"
		"\t-l limit: stop after limit evals\n"
		"\t-R string: use string to restart after checkpoint.\n"
		"\t-C interval: enable checkpointing. Auto checkpoint every interval evals.\n"
	);
}

int
main(int argc, char **argv)
{
	int D, R;
	char c;
	int verbose = 0;

	while ((c = getopt(argc, argv, "vqtcVhH:bD:O:l:R:C:")) != -1) {
		switch(c) {
		case 'v':	verbose++;
			break;
		case 'q':	verbose = -1;
			break;
		case 't':	dbg |= DBG_TIME;
			break;
		case 'c':	dbg |= DBG_CTR;
			break;
		case 'V':	dbg |= DBG_VER;
			break;
		case 'h':	dbg |= DBG_HASH;
			break;
		case 'H':
			hashval = strtol(optarg, NULL, 0);
			break;
		case 'b':	dbg |= DBG_PRO;
			break;
		case 'D':
			dbg = strtol(optarg, NULL, 0);
			dbg |= DBG_DBG;
			break;
		case 'O':
			outfile = optarg;
			break;
		case 'l':
//			dbg |= DBG_STOP;
			stop = strtol(optarg, NULL, 0);
			break;
		case 'R':
			cpstr = optarg;
			break;
		case 'C':
//			dbg |= DBG_CPR;
			cpval = strtol(optarg, NULL, 0);
			break;
		case '?':
		default:
			usage(argv[0]);
			return 1;
			break;
		}
	}

	argc -= optind;
	if (argc != 2) {
		fprintf(stderr, "invalid number of arguments %d\n",
			argc);
		usage(argv[0]);
		return 1;
	}
	D = atoi(argv[optind]);
	if (D <= 0) {
		fprintf(stderr, "bad darts value %s\n", argv[1]);
		return 2;
	}
	R = atoi(argv[optind + 1]);
	if (R <= 0) {
		fprintf(stderr, "bad region value %s\n", argv[2]);
		return 3;
	}
	/* translate verbose into dbg flags. Note fall-through. */
	switch(verbose) {
		case 5: dbg = DBG_ALL;
		case 4: dbg |= DBG_DUMPS|DBG_DUMPF|DBG_DUMPV|DBG_FILLIN2;
		case 3: dbg |= DBG_MAIN|DBG_INIT;
		case 2: dbg |= DBG_MRF | DBG_FILLIN;
		case 1:	dbg |= DBG_PRO;
			break;
		case -1: dbg = DBG_NONE;
			break;
	}
	DBG(DBG_DBG, ("debug is 0x%lX\n", dbg));
	DBG(DBG_VER, ("version %s\n", VER));

	initstuff(D, R);

	mrf(D-1, 0);

	dumpscores(D);

	return 0;
}
