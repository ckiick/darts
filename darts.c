
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
#include <sys/stat.h>
#include <fcntl.h>

#include "bar.h"

// #define DEBUG

#ifdef DEBUG
#include <assert.h>
#endif

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

#ifdef DEBUG
#define	DBG(f, pargs)			\
	if ((((f) & dbg)==(f)) ? (printf("%s:",__func__) + printf pargs) : 0)

#define ASSERT(x)	assert((x))
#else
#define	DBG(f, pargs)			\
	if (0)

#define ASSERT(X)
#endif

uint64_t	hashval = 1000000;
uint64_t	stop = 0;
char 		*outfile = NULL;
uint64_t	cpval = 0;
char		*cpfn = NULL;

#define	CPFN	"darts.cpr"
#define MAXR	40
#define	MAXVAL	(12*1024)
#define	MAXD	6

int restarting = 0;

int def_limits[MAXD] = { 40, 40, 40, 30, 20, 10 };

typedef struct _sc {
	bar_t s;		/* bit array. */
//	uint8_t s[MAXVAL];
	int gap;		/* also score. first uncovered number */
	int maxval;		/* max val covered. */
} sc_t;

typedef struct _plane {
	sc_t sc[MAXD];		/* one set for each number of darts */
	int rval;		/* value of region for this plane. */
	int rlow;		/* iteration start value */
	int rhi;		/* iteration limit */
} plane_t;

typedef struct _vals {
	int score;		/* where is the first gap? */
	int vals[MAXR+1];	/* value of each region. */
} vals_t;

typedef struct _block {
	plane_t pl[MAXR+1];	/* one for each number of regions. */
	vals_t vals;			/* current set of r values. */
	vals_t besties[MAXD][MAXR+1];	/* best values for each r,d comb. */
	int limits[MAXD];
	int D;		// number of darts
	int R;		// number of regions
	int d;		// current d value
	int r;		// current value of r 
	unsigned long iters;	// iteration count
} block_t;

/* global huge block of stuff. EVERYTHING is stored here. */
block_t hbos;


/*
 * a few semi-helpful notes.
 * darts are 0-based. if there are n darts, they are numbered 0..n-1.
 * regions are also 0 based, r regions numberes 0 .. r-1.
 * regions and darts are printed using whole numbers 1..d or 1..n.
 * the values are 1-based.  There's an implicit 0-value region, which
 * corresponds to a dart missing the dart board, but we don't bother to
 * track it.
 *
 * method - with 1 dart 1 region, there is only 1 value, and it has
 * to be 1, giving a gap at 2 (score).  This is the foundation case.
 * Incrementing the number of darts (n darts, 1 region) increments
 * the score (S=d+1).  Now we can calculate solutions for any value of d for
 * r=1.  Conversely, with 1 dart it is easy to add another region with
 * value v, by simply adding v to the set of numbers reached and checking 
 * for a change in the score/gap.   So we can construct any d=1,r=m or
 * d=n,r=1.
 * For d=n,r=m, we can construct data that is a set of values for
 * each region and the numbers it can reach with the given number of darts.
 * The score is then the first "gap" in the set of numbers that can be
 * reached.  An optimal solution is the maximum score for all possible
 * region values.
 * A single score for darts d and regions r is calculated from the values
 * assigned to the regions. {V(r)}= { V[1]...V[r]}. This is mapped to a set
 * {B(d,r)} of numbers that can be reached with that combination of values
 * and the given number of darts.  The score S is then the first missing
 * integer in {B(d,r)}.
 * If we have the set {B(d-1,r)}, what happens when we add a dart? Imagine
 * a dartboard with darts in it already.  If we throw another dart we
 * get to add any of the values in {V(r)} to our total (including the
 * case where we miss the board altogether).  Therefore we can extend
 * the set {B(d,r)} from {B(d-1,r)} by adding all v in {V} to the values in
 * {B(d-1,r)}.
 *       for each pair v,b, v in {V(r)} and b in {B(d-1,r)}
 *		{B(d,r} += v+b;
 * However, with an extra dart there are now more combinations that
 * can be made. So we have to include all of them that can be done with
 * the extra dart and NOT the extra region. This is the set {B(d,r-1)}.
 * So to get {B(d,r)} we need {B(d-1,r)} and {B(d,r-1)}.
 * The first can be found by recursing until d=1 is reached, which from above
 * is defined as {B(1,r)} = {V(r)}.
 * Now that we know how to find S(d,r) given {V(r)}, finding the optimal
 * solution requires exploring all possible sets of {V(r)}.  Fortunately,
 * the range of values for any region can be pruned quite a bit. If we
 * are adding a region r, then it doesn't effect the score at all if
 * V[r] is greater than the gap.  So the upper bound on V[r] must be
 * S(d,r-1).  If we order the darts by value, then anything at or below
 * the last value will have already been covered.  So the lower limit
 * is V[r-1]+1.  This again presents a good use of recursion, since we
 * know that V[1] = 1, and S(d,1) = d+1.
 * In order to get S(d,r-1) we have to have {B(d,r-1)}.  Thus to find
 * the optimal V[r] we need both {B(d-1,r)} and {B(d,r-1)}.  Looked at
 * another way, when finding optimal S(d,r) we also get optimal S(1..d, 1..r)
 * thrown in!
 * The algorithm A(D,R) returns the set {V(R)} that yeilds the highest
 * score S for D,R. In fact, it can fill out a matrix of sets for all
 * 1..D,1..R as O[D,R]{V(1..r)} that provides optimal solutions for all
 * values of D and R.
 * A(d,r), in order to avoid repeating combinations of values, starts
 * at r=1. So the initial call is A(D,1) with the limit of R being saved
 * as a global.  Also global is the current set {V(r)}, and the sets
 * {B(1..D,1..R)}, along with the scores S(1..D,1..R).
 * On entry A determines the limits of V[r]. For the initial call this
 * is just 1, otherwise it is V[r-1]+1 to S(d-1,r).  Also we know that
 * S(1,1) must be 2, and that {B(1,1)} = { 1 }.
 * The algorithm iterates V[r] over those limits, first recursing "down" with
 * A(d-1,r) to get the set {B(d-1,r)}. Then it recurses "up" to get
 * A(d,r+1), as long as r < R.  The score for each V[r] is compared to the
 * current best score, and the set {V[r]} with the better score is kept
 * in the optimal score matrix (If S(d,r) > S(O[d,r]), then O[d,r] = {V[r]}).
 * When the initial call to A returns, the matrix O will contain all
 * optimal solutions for the problem for all values of R and D.
 * 
 * Note that if the sets {B} are represented as bit-vectors, then
 * "Adding" two sets becomes a bit-wise OR operation, and the operation
 * of "add value v to all values in B" becomes a SHIFT+OR (X={B}|({B}<<v)).
 * Much faster than looping over arrays of integers.
 */

void
dumpscore(sc_t score)
{
	int i;
	printf("gap=%d:[max=%d] ", score.gap, score.maxval);
	for (i=1; i <= score.maxval +1; i++) {
		if (barget(&(score.s),i)) printf("%d ",i);
	}
}

void
dumpframe(int r, int D)
{
	int d, i;
	plane_t *f;

	f = &(hbos.pl[r]);

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
	printf("score=%d:[r=%d] ", hbos.vals.score, R+1);
	for (i = 1; i < R; i++) {
		printf("%d, ", hbos.vals.vals[i]);
	}
	printf("%d\n", hbos.vals.vals[i]);
}

void
dumpmore(int d, int r)
{
	int i;
	sc_t *scores;

	scores = &(hbos.pl[r].sc[d]);

	printf("O[d=%d,r=%d]:S=%d,V={", d+1, r+1, scores->gap);
	for (i = 0; i < r+1; i++) {
		printf("%d ", hbos.vals.vals[i]);
	}
	printf("},max=%d,B={", scores->maxval);
	for (i=1; i <= scores->maxval +1; i++) {
		if (barget(&(scores->s),i)) printf("%d ",i);
	}
	printf("}\n");
}

int
checkpoint(char *fn)
{
	int i,j;
	int fd;
	size_t wrv;
	int brv;

	if (fn == NULL) return -1;

	fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0660);
	if (fd < 0) {
		perror("checkpoint open");
		return -2;
	}

	wrv = write(fd, &hbos, sizeof(block_t));
	if (wrv != sizeof(block_t)) {
		printf("failed to write hbos.\n");
		close(fd);
		return -3;
	}
	for (i = 0; i <= MAXR; i++) {
		for (j = 0; j < MAXD; j++) {
			brv = barwrite(&(hbos.pl[i].sc[j].s), fd);
			if (brv < 0) {
				printf("problems writing bar %d,%d, aborting.\n", i,j);
				close(fd);
				return -4;
			}
		}
	}
	close(fd);
	return 0;
}

int restart(char *fn)
{
	int i,j;
	int fd;
	size_t rrv;
	int brv;

	if (fn == NULL) return -1;

	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		perror("restart open");
		return -2;
	}
	rrv = read(fd, &hbos, sizeof(block_t));
	if (rrv != sizeof(block_t)) {
		printf("trouble reading hbos.\n");
		close(fd);
		return -3;
	}
	for (i = 0; i <= MAXR; i++) {
		for (j = 0; j < MAXD; j++) {
			hbos.pl[i].sc[j].s = nobar;
			brv = barread(&(hbos.pl[i].sc[j].s), fd);
			if (brv < 0) {
				printf("problems reading bar %d,%d, aborting.\n", i,j);
				close(fd);
				return -4;
			}
		}
	}
	close(fd);
	return 0;
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
	int rv;

	ASSERT((d >=0)&&(r>=0));
	DBG(DBG_FILLIN, ("filling in d=%d,r=%d with val %d\n", d+1, r+1, val));
	scores = &(hbos.pl[r].sc[d]);

	if (d==0) {
		if (r > 0) {
			/* we are adding a value r to {B(1,r-1)}.
			 * with 1 dart we just add the value v to B.
			 */
			dsc = &(hbos.pl[r-1].sc[d]);
			ASSERT(val >= (barlen(&(dsc->s))-1));
			barcpy(&(scores->s), &(dsc->s));
			scores->maxval = val;
DBG(DBG_FILLIN2, ("copied d=%d r=%d ", d+1, r-1+1)) {
	dumpscore(*dsc);
	printf(" to ");
	dumpscore(*scores);
	printf("\n");
}
			barset(&(scores->s), val);
			scores->gap = barfnz(&(scores->s), 1);
			if (scores->gap == -1) {
				scores->gap = barlen(&(scores->s));
				barclr(&(scores->s), scores->gap);
			}
		}
		DBG(DBG_FILLIN, ("base for d=%d,r=%d,gap=%d\n", d+1,r+1, scores->gap));
	} else {
		/* otherwise, d > 1 */
		fillin(d-1, r, val);
		/* what we want to do is copy, shift, or. */
		dsc = &(hbos.pl[r].sc[d-1]);
		barcpy(&(scores->s),&(dsc->s));
DBG(DBG_FILLIN2, ("copied (only) d=%d r=%d ", d-1+1, r+1)) {
	dumpscore(*scores);
	printf("\n");
}
		barlsle(&(scores->s), &(scores->s), val);
		baror(&(scores->s), &(dsc->s), &(scores->s));
//		scores->gap = dsc->gap;
		scores->maxval = dsc->maxval + val;
DBG(DBG_FILLIN2, ("added val %d ", val)) {
	dumpscore(*dsc);
	printf(" to make  ");
	dumpscore(*scores);
	printf("\n");
}

		if (r > 0) {
			// now do the d,r-1 half.
			dsc = &(hbos.pl[r-1].sc[d]);
			baror(&(scores->s), &(dsc->s), &(scores->s));
		}

DBG(DBG_FILLIN2, ("summed with d=%d r=%d ", d+1,r-1+1)) {
	dumpscore(*dsc);
	printf(" and got ");
	dumpscore(*scores);
	printf("\n");
}
		/* now find the new gap. */
		rv = barfnz(&(scores->s),1);
		if (rv == -1) {
			/* add a 0 at the end. */
			scores->gap = barlen(&(scores->s));
			barclr(&(scores->s), scores->gap);
		} else {
			scores->gap = rv;
		}
		DBG(DBG_FILLIN, ("for d=%d,r=%d,maxval=%d gap=%d \n", d+1,r+1, scores->maxval, scores->gap));
	}
	/* update best scores. */
	if (scores->gap > hbos.besties[d][r].score) {
		hbos.besties[d][r] = hbos.vals;
		hbos.besties[d][r].score = scores->gap;
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

	DBG(DBG_DUMPV, ("  ")) {
		dumpmore(d,r);
	}
}


/* massively recursive function. Well, maybe not so massive. */
void
mrf(int depth)
{
	sc_t *scores;

	if (restarting && depth < hbos.r) {
		DBG(DBG_MRF, ("-> restarting depth=%d, r=%d\n", depth, hbos.r+1));
	} else {

		DBG(DBG_MRF, ("-> called with d=%d r=%d\n", hbos.d+1, hbos.r+1));
		if ((hbos.d < 0) || (hbos.r < 0)) return;
		if ((hbos.d == 0) && (hbos.r == 0)) return;
		if (hbos.r >= hbos.limits[hbos.d]) return;	/* important stop condition. */

		if (hbos.r == 0) {
			hbos.pl[hbos.r].rlow = hbos.pl[hbos.r].rhi = 1;
		} else  {
			hbos.pl[hbos.r].rlow = hbos.pl[hbos.r-1].rval + 1;
			hbos.pl[hbos.r].rhi = hbos.pl[hbos.r-1].sc[hbos.d].gap;
		}
		hbos.pl[hbos.r].rval = hbos.pl[hbos.r].rlow;

		DBG(DBG_MRF, (" Iterate r=%d val from %d to %d\n", hbos.r+1, hbos.pl[hbos.r].rlow, hbos.pl[hbos.r].rhi));
	}


	for (/*already set*/; hbos.pl[hbos.r].rval <= hbos.pl[hbos.r].rhi;
	    hbos.pl[hbos.r].rval++) {
		/* here is where we CPR */
		if ((cpval > 0) && (hbos.iters > 0) && ( (hbos.iters % cpval)==0) && (! restarting)) {
			checkpoint(CPFN);
		}
		if (restarting) {
			if (depth < hbos.r) {
				mrf(depth+1);
			} else {
				restarting = 0;
				/* back to normal */
			}
		} else {
			hbos.vals.vals[hbos.r] = hbos.pl[hbos.r].rval;
			fillin(hbos.d, hbos.r, hbos.pl[hbos.r].rval);
			DBG(DBG_DUMPF, ("frame %d\n",hbos.r )) {
				dumpframe(hbos.r,hbos.d+1);
			}
			if (hbos.r+1 >= hbos.limits[hbos.d]) {
				continue;
			}
			DBG(DBG_MRF, ("  recursing with d=%d,r=%d V[r]=%d\n", hbos.d+1, hbos.r+1+1, hbos.pl[hbos.r].rval));
			hbos.r++;
//			mrf(hbos.d, hbos.r);
			mrf(depth+1);
		}
		hbos.r--;
		hbos.iters++;

	}
	DBG(DBG_DUMPV, ("current vals ")) {
		dumpvals(hbos.r);
	}
	DBG(DBG_MRF, ("<- returning from d=%d,r=%d\n", hbos.d+1, hbos.r+1));
}

/* at the end, print out everything we found. */
void
dumpscores(int D)
{
	int d, r, i;

	printf("best scores found this run:\n");
	for (d=0; d < D; d++) {
		for (r = 0; r < hbos.limits[d]; r++) {
			printf("%d darts, %d regions: score=%d; vals=", d+1, r+1, hbos.besties[d][r].score);
			for (i=0; i <= r; i++) {
				printf("%d,", hbos.besties[d][r].vals[i]);
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
/*
	DBG(DBG_INIT, ("zeroing out vals %d bytes\n", sizeof(vals_t)));
	bzero(vals, sizeof(vals_t));
	DBG(DBG_INIT, ("zeroing out besties %d bytes\n", sizeof(besties)));
	bzero(besties, sizeof(besties));
*/

	hbos.D = D;
	hbos.R = R;
	hbos.r = 0;
	hbos.d = hbos.D;
	/* set the degenerate case up first.
	 * d=1, r=1 can only have a val of 1 and a score/gap of 2.
	 * also, there must always be a val=1.
	 */
	barset(&(hbos.pl[0].sc[0].s),1);
	// should we set bit 0 as well??
	hbos.pl[0].sc[0].gap = 2;
	hbos.pl[0].sc[0].maxval = 1;
	hbos.pl[0].rval = 1;

	hbos.besties[0][0].score = 2;
	hbos.besties[0][0].vals[0] = 1;

	DBG(DBG_DUMPF, ("init frame\n")) {
		dumpscore(hbos.pl[0].sc[0]);
		printf("\n");
	}

	for( i = 0; i < MAXD; i++) {
		if (def_limits[i] > R) {
			hbos.limits[i] = R;
		} else {
			hbos.limits[i] = def_limits[i];
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
		"\t-c: show iteration counts\n"
		"\t-V: display version number.\n"
		"\t-h: show hash mark every interval iterations.\n"
		"\t-H interval: set the hash inteval (default 1,000,000)\n"
		"\t-b: show best answers as found (progress report)\n"
		"\t-D value: set dbg value directly.\n"
		"\t-O file: direct output to file instead of stdout\n"
		"\t-n cpus: fork n copies to work in parallel.\n"
		"\t-l limit: stop after limit iterations (checkpoint)\n"
		"\t-R file_name: use file to restart after checkpoint.\n"
		"\t-C interval: save checkpoint file every interval iterations.\n"
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
			cpfn = optarg;
			restarting = 1;
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

	if (restarting) {
	} else {
//		mrf(D-1, 0);
		mrf(0);
	}

	dumpscores(D);

	return 0;
}
