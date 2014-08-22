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
 * -c : track and display number of evals (counter)
 * -h : display hash marks (like a heartbeat)
 * -H interval : how many evals per hash mark (default is 1,000,000).
 * -b : show best scores as they are found (progress report)
 * -D val : set debug value manually.
 * -V : display version info
 * -q : quiet. Shhhhhhh....
 * -l limit : quit after limit evals.
 * -O file : write debug output to file instead of stdout.
 * -C interval: checkpoint, every interval evals. Also when interrupted.
 * -R fname: use file to restart from checkpoint.
 * -n ncpus: work using ncpus at once.
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
#include <sys/time.h>		// hrtime
#include <unistd.h>
#include <sys/stat.h>		// open
#include <fcntl.h>		// open
#include <sys/wait.h>		// wait
#include <errno.h>		// errno, perror.
#include <string.h>		// memset
#include <assert.h>		// assert
#include "bar.h"

// #define DEBUG

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
#define DBG_CPR		0x00008000
#define	DBG_NONE	0x80000000
#define DBG_ALL		0x7FFFFFFF

#ifdef DEBUG
#define	DBG(f, pargs)			\
	if ((((f) & dbg)==(f)) ? (printf("%s:",__func__) + printf pargs) : 0)

#define ASSERT(x)	assert((x))
#else
#define	DBG(f, pargs)			\
	if (0)

#define ASSERT(X)
#endif

#if !defined(__sun)
typedef uint64_t hrtime_t;
hrtime_t gethrtime()
{
	struct timespec ts;
        (void)clock_gettime(CLOCK_REALTIME, &ts);
        return ((uint64_t)ts.tv_sec * 1000000000LU + (uint64_t)ts.tv_nsec);
}
#endif

#define	CPFN	"darts.cpr"
#define	FNSZ	32
#define	FNFMT	"darts_r%02d_%04d"
#define MAXR	40
#define	MAXD	6

const int def_limits[MAXD] = { 40, 40, 40, 30, 20, 10 };

/* options */
uint64_t	hashval = 100000;	// iters per hash
uint64_t	stop = 0;		// iter limit
uint64_t	cpval = 0;		// checkpoint interval
int		checker;		// checkpoint on r level
char		*cpfn = NULL;		// checkpoint filename
char		def_cpfn[] = CPFN;	// default cpfn
int		restarting = 0;		// -R - recover from checkpoint
long		dbg = DBG_NONE;		// debug/verbose and other flags
int		wipe = 0;		// wipe - clean up and quit

typedef struct _sc {
	bar_t s;		/* bit array. */
	int gap;		/* also score. first uncovered number */
	int maxval;		/* max val covered. */
} sc_t;

typedef struct _plane {
	sc_t sc[MAXD];		/* one set for each number of darts */
	int rval;		/* value of region for this plane. */
	int rlow;		/* iteration start value */
	int rhi;		/* iteration limit */
} plane_t;

/* expanded  */
typedef struct _vals {
	int score;		/* where is the first gap? */
	int vals[MAXR+1];	/* value of each region. */
	int from;		/* starting value */
	int to;			/* ending value */
	int rhi;		/* highest r value */
	int rlo;		/* lowest r value */
} vals_t;


typedef struct _task {
	int id;		// who is this for
	int lvl;	// starting at R lvl
	int rhi;	// upper
	int rlo;	// lower
	int from;	// start here
	int to;		// end here
	int cpus;	// how many cpus to use
	char fname[FNSZ];	// non-suffix name for us
} task_t;

typedef struct _best {
	vals_t b[MAXD][MAXR+1];	/* best values for each r,d comb. */
} best_t;

typedef struct _stats {
	unsigned long iters;	// iteration count
	hrtime_t begin;		// start of iterations
	hrtime_t end;		// when we quit.
	hrtime_t runtime;	// total runtime.
	unsigned long checks;	// how many checkpoints
} stats_t;

typedef struct _result {
	task_t task;
	best_t besties;
	stats_t stats;
} result_t;

/* values for status */
#define SS_NEW	0	/* blank and empty */
#define SS_INIT	1	/* initialized */
#define SS_SET	2	/* ready to run. */
#define SS_RUN	3	/* spawned, off chewing data */
#define SS_REV	4	/* reviving-don't disturb */
#define	SS_EXIT	5	/* called exit with non-zero */
#define	SS_DEAD	6	/* hit a signal, and keeled over */
#define SS_DONE	7	/* all done. (exited with 0) */
#define SS_READ	9	/* results in hbos (of parent). */
#define SS_GOOD	9	/* results are good. (afawk) */
#define SS_MELD	10	/* results have been merged. */
#define SS_ERR	-1	/* ob */

typedef struct _slot {
	task_t task;
	int status;
	int rv;
	pid_t cpid;
	result_t res;
} slot_t;

typedef struct _block {
	plane_t pl[MAXR+1];	/* one for each number of regions. */
	vals_t vals;			/* current set of r values. */
//	vals_t besties[MAXD][MAXR+1];
	int limits[MAXD];
	int D;		// number of darts
	int R;		// number of regions
	int d;		// current d value
	int r;		// current value of r 
	int ncpus;		// all cpus for this run
	int subcpus;		// all cpus for peers
	int mycpus;		// cpus assigned to me.
	int startdepth;		// know when to retire
	int kids;		// how many sub-processes (n slots)
	slot_t *slots;		// if watching kids
	task_t task;		// assigned work to do
	best_t besties;	/* best values for each r,d comb. */
	stats_t	stat;	// counters and timers
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

/* read and validate results */
int
getres(int id)
{
	int fd;
	char resfn[FNSZ];
	int i;
	int rv;
	size_t rrv;

	if (hbos.slots == NULL) return -1;

	if (hbos.slots[id].status != SS_DONE) return -2;

	/* construct filename */
	strcpy(resfn, hbos.slots[id].task.fname);
	strcat(resfn, ".res");

	fd = open(resfn, O_RDONLY);
	if (fd < 0) {
		perror("open results file");
		return -3;
	}
	rrv = read(fd, &(hbos.slots[id].res), sizeof(result_t));
	if (rrv != sizeof(result_t)) {
		perror("read res");
		close(fd);
		return -4;
	}
	close(fd);
	hbos.slots[id].status = SS_READ;
	/* any validation? Yes. task must match.*/
	if (hbos.slots[id].res.task.id != id) return -5;
	if (hbos.slots[id].res.task.lvl != hbos.slots[id].task.lvl) return -5;
	if (hbos.slots[id].res.task.rhi != hbos.slots[id].task.rhi) return -5;
	if (hbos.slots[id].res.task.rlo != hbos.slots[id].task.rlo) return -5;
	if (hbos.slots[id].res.task.from != hbos.slots[id].task.from) return -5;
	if (hbos.slots[id].res.task.to != hbos.slots[id].task.to) return -5;
	/* if the fname was different, we wouldn't have found it. */

	hbos.slots[id].status = SS_GOOD;
	return 0;
}

/* combine slot results as much as possible. */
/* all kids should be good, or at least read. */
int
meld()
{
	int i, j, k;
	int maxsc;
	int lumps = 0;
	int gaps = 0;
	int gapping = 1;
	int lead;


	for (i = 0; i < hbos.kids; i++) {
		if (hbos.slots[i].status != SS_GOOD) {
			if (! gapping) {
				gapping = i+1;
				gaps++;
			}
			continue;
		} else {
			if (gapping) {
				lumps++;
				gapping = 0;
				lead = i;
				continue;
			}
			/* otherwise, try to do a meld */
			assert(hbos.slots[lead].res.task.to +1 ==
			    hbos.slots[i].res.task.from);
			for (j = 0; j <= MAXR ; j++) {
				for (k = 0; k < MAXD ; k++) {
					if (hbos.slots[i].res.besties.b[j][k].score > hbos.slots[lead].res.besties.b[j][k].score) {
						hbos.slots[lead].res.besties.b[j][k] = hbos.slots[i].res.besties.b[j][k];
					}

				}
			}
			hbos.slots[lead].res.stats.iters += hbos.slots[i].res.stats.iters;
			hbos.slots[lead].res.stats.runtime += hbos.slots[i].res.stats.runtime;
			hbos.slots[lead].res.stats.checks += hbos.slots[i].res.stats.checks;
			hbos.slots[lead].res.task.to = hbos.slots[i].res.task.to;
			hbos.slots[i].status = SS_MELD;
		}
	}
	if ((lumps==1) && (gaps == 0) && (lead == 0)) {
		return 0;
	}
	if (lumps > 0) return lumps;
	return -gaps;
}

/* a child has died. attempt to bring it back to life.
 * Child must be EXIT or DEAD.
 * returns number of revived, 0 if nothing revivable, <0 on erorr.
 * pass in -1 to revive everything possible.
 * For now, just respawn. Later on, want to use CPR.
 */
int
revive(int id)
{
	int i, j;
	int n;
	int souls = 0;
	int rv;

	if (id == -1) {
		i = 0;
		n = hbos.kids;
	} else {
		i = id;
		n = id+1;
	}

	for (; i<n; i++) {
		if ((hbos.slots[i].status != SS_EXIT) &&
		    (hbos.slots[i].status != SS_DEAD)) {
			continue;
		}
		if ((hbos.slots[i].status == SS_EXIT) &&
		    (hbos.slots[i].rv < 0)) {
			continue;
		}
		hbos.slots[i].status = SS_REV;
		/* is there anything to reset? */
		hbos.slots[i].rv = 0;
		hbos.slots[i].cpid == 0;
		hbos.slots[i].status = SS_SET;
		rv = spawn(i);
		if (rv < 0) {
			hbos.slots[i].status = SS_ERR;
		} else if (rv > 1) {
			souls++;
		} else {
			/* we are the child again. */
			// CALL work(); Or return to it. Something.
		}
	}
	return souls;
}

/* watch the kids */
int
babysit()
{
	int i, j;
	int sub;
	pid_t p;
	int status;
	int notdone;
	int n = hbos.kids;


	if ((hbos.kids < 1) || (hbos.slots == NULL)) {
		return -1;
	}
	notdone = hbos.kids;
	while (notdone) {
		p = wait(&status);
		if (p < 0) {
			if (errno == EINTR)  {
				continue;		// retry.
			} else if (errno == ECHILD) {
				if (revive(-1) == 0) {
					/* nothing to revive. */
					break;
				}
				continue;
			} else {
				perror("wait");
				return -2;
			}
		} else if (p == 0) {
			/* doesn't make sense. */
			printf("weird pid from wait\n");
			return -3;
		}
		/* got a pid. */
		for (sub = -1, i = 0; i < hbos.kids; i++) {
			if (hbos.slots[i].cpid == p) {
				sub = i;
				break;
			}
		}
		if (sub < 0) {
			printf("unknown child process %d: ignored\n", p);
			continue;
		}
		if (WIFEXITED(status)) {
			hbos.slots[sub].rv =  WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			hbos.slots[sub].status = SS_DEAD;
			revive(sub);
			continue;
		}
		if (hbos.slots[sub].rv < 0) {
			/* it's a lost cause */
			hbos.slots[sub].status = SS_EXIT;
			break;
		} else if (hbos.slots[sub].rv > 0) {
			/* possible to try again */
			hbos.slots[sub].status = SS_EXIT;
			revive(sub);
			continue;
		} else { 	/* just right */
			// add in the info
			hbos.slots[sub].status = SS_DONE;
			meld(sub);	// more params?
			notdone--;
		}

	}
	return 0;
}

/*
 * create kids. All done by what's in the slots.
 * Normally use -1 id to spawn all. Can give >0 id to spawn 1.
 * returns -1 = error, 0 if we are the forked child, and >1 if parent,
 * indicating number of kids.
 */
int
spawn(int id)
{
	int i = 0;
	pid_t fp;
	int rv = 0;
	int n = hbos.kids;

	if ((hbos.slots == NULL) || (hbos.kids == 0)) {
		return -1;
	}

	if (id >= 0) {
		/* special case when we want to spawn just one. */
		i = id;
		n = id+1;
	}
	for (; i < n; i++) {
		if (hbos.slots[i].status != SS_SET) {
			continue;	/* nope */
		}

		fp = fork();
		if (fp == 0) {
			/* we are the child */
			hbos.task = hbos.slots[i].task;
			free(hbos.slots);	/* dont need parent's data */
			hbos.slots = NULL;
			return(0);
		} else if (fp > 0) {
			/* parentage */
			hbos.slots[i].status = SS_RUN;
			hbos.slots[i].cpid = fp;
			rv++;
			/* keep going */
		} else {
			perror("fork");
			hbos.slots[i].status = SS_ERR;
			/* miscarry */
			return(-1);
		}
	}
	/* proud parents have spawned */
	return rv;
}

/* divide up task.
 * returns: <0=error, >0=#slots created, =0 nothing to do.
*/
int
divvy()
{
	int i, j, k, slice;
	int n, range;

	if (hbos.task.cpus <= 1) {
		/* no extra cpus. We do it all ourselves. */
		return 0;
	}
	assert(hbos.slots == NULL);

	range = (hbos.task.to - hbos.task.from) + 1;

	if (range > hbos.task.cpus) {
		n = hbos.task.cpus;
	} else {
		/* more cpus than r-values (or same) */
		n = range;
	}
	/* allocate slots */
	hbos.slots = malloc(sizeof(slot_t) * n);
	if (hbos.slots == NULL) {
		perror("malloc");
		return -1;
	}
	memset(hbos.slots, 0, sizeof(slot_t)*n);
	hbos.kids = n;
	j = hbos.task.cpus;
	k = range;
	for (i = 0; i < n; i++) {
		hbos.slots[i].task.id = i;
		hbos.slots[i].task.lvl = hbos.r + 1; 
		hbos.slots[i].task.rhi = hbos.pl[hbos.r+1].rhi;
		hbos.slots[i].task.rlo = hbos.pl[hbos.r+1].rlow;
		if (n == range) {
			hbos.slots[i].task.to = hbos.slots[i].task.from =
			     (hbos.slots[i].task.rlo + i);
			slice = j / k;
			assert(slice > 0);
			hbos.slots[i].task.cpus = slice;
			j -= slice;
			k--;
		} else {
			hbos.slots[i].task.cpus = 1;
			if (i = 0) {
				hbos.slots[i].task.from = hbos.slots[i].task.rlo;
			} else {
				hbos.slots[i].task.from = hbos.slots[i-1].task.to+1;
			}
			slice = k / j;
			assert(slice > 0);

			hbos.slots[i].task.to = hbos.slots[i].task.from + (slice - 1);
			k -= slice;
			j--;
		}
		sprintf(hbos.slots[i].task.fname,  FNFMT, hbos.slots[i].task.lvl, hbos.slots[i].task.id);
		hbos.slots[i].status = SS_INIT;
	}
	assert((j== 0) && (k == 0));

	return n;
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

	/* minor cleanup */
	if (dbg & DBG_TIME) {
		hrtime_t now;

		now = gethrtime();
		hbos.stat.runtime += (now - hbos.stat.begin);
		hbos.stat.begin = gethrtime();
		hbos.stat.end = 0;
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

	/* restart thingy. */
	if (dbg & DBG_TIME) {
		hbos.stat.begin = gethrtime();
	}
	printf("restarting from checkpoint %s\n", cpfn);
	return 0;
}

/* at the end, print out everything we found. */
void
dumpscores(int D)
{
	int d, r, i;

	printf("best scores found this run:\n");
	for (d=0; d < D; d++) {
		for (r = 0; r < hbos.limits[d]; r++) {
			printf("%d darts, %d regions: score=%d; vals=", d+1, r+1, hbos.besties.b[d][r].score);
			for (i=0; i <= r; i++) {
				printf("%d,", hbos.besties.b[d][r].vals[i]);
			}
			printf("\n");
		}
	}
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
	if (scores->gap > hbos.besties.b[d][r].score) {
		hbos.besties.b[d][r] = hbos.vals;
		hbos.besties.b[d][r].score = scores->gap;
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

/*It's not just a job... 
 * We need to know start depth to know when to quit.
 */
int
work(int depth)
{
	sc_t *scores;

	if (restarting && (depth < hbos.r)) {
		DBG(DBG_MRF, ("-> restarting depth=%d, r=%d\n", depth, hbos.r+1));
	} else {

		DBG(DBG_MRF, ("-> called with d=%d r=%d\n", hbos.d+1, hbos.r+1));
		if ((hbos.d < 0) || (hbos.r < 0)) return -1;
		if ((hbos.d == 0) && (hbos.r == 0)) return -1;
		if (hbos.r >= hbos.limits[hbos.d]) return -1;	/* important stop condition. */

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
		if ((cpval > 0) && (hbos.stat.iters > 0) && ( (hbos.stat.iters % cpval)==0) && (! restarting)) {
			printf("checkpoint at %lu iterations (depth=%d)\n", hbos.stat.iters, depth);
			checkpoint(cpfn);
		}
		if ((stop > 0) && (!restarting) && (hbos.stat.iters >= stop)) {
			printf("checkpoint and stop at %lu iterations(depth=%d)\n", hbos.stat.iters, depth);
			checkpoint(cpfn);
			exit(0);
		}
		if (restarting) {
			if (hbos.stat.iters > stop) {
				stop = 0;
			}
			if (depth < hbos.r) {
				work(depth+1);
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
			work(depth+1);
		}
		hbos.r--;
		hbos.stat.iters++;
		if (dbg & DBG_HASH) {
			if ((hbos.stat.iters % hashval) == 0) {
				printf("#");
				fflush(stdout);
			}
		}
		if (depth == checker) {
			checkpoint(cpfn);
		}
	}
	DBG(DBG_DUMPV, ("current vals ")) {
		dumpvals(hbos.r);
	}
	DBG(DBG_CTR, ("performed %lu iterations\n", hbos.stat.iters));
	DBG(DBG_MRF, ("<- returning from d=%d,r=%d\n", hbos.d+1, hbos.r+1));
	return 0;
}


#define NX_SOUL		0x0000F
#define NX_REV		0x00003		/* we are a reincarnation */
#define NX_NEW		0x00000		/* new soul */

#define NX_FROM		0x000F0
#define NX_MAIN		0x00010		/* called from main. */
#define NX_FORK		0x00020		/* called as child just forked */
#define NX_WORK		0x00040		/* called from worker */

#define NX_RELATE	0x00F00
#define NX_ALONE	0x00000		/* no kids, no parent. */
#define NX_CHILD	0x00100		/* has a parent. */
#define NX_PARENT	0x00200		/* children. possibly orphan. */
#define NX_FAMILY	0x00300		/* both */

#define NX_ACTION	0xFF000
#define NX_EARN		0x01000		/* go to work */
#define NX_RETIRE	0x02000		/* time to retire */
#define NX_BREED	0x04000		/* start a family */
#define NX_KILLEM	0x08000		/* shut ti all down */
#define NX_RECOVER	0x10000		/* find any orphans, clean up */
#define NX_CLEAN	0x20000		/* clean up */
#define NX_ADOPT	0x40000		/* play godfather */

/* the nexus.
 * the nexus is the crossroads of the program.  It's the first thing
 * called from main or after fork. It knows what it is, and what to
 * do next.
 * The nexus has to handle a lot of different cases, some of which 
 * can overlap.  It knows if it has been reincarnated, and what it did
 * on what it learned.
 * The nexus is implemented like a traffic cop.  The state is
 * examined, and then dispatched to the appropriate action.  It
 * has to handle this when called, and it has to handle things when
 * an action returns.
 * Hang on , it might get a litle messy.
 */
int nexus(int marker, int depth)
{
	/* depth tells us how far down the stack we are. Could be
	 * extracted from the hbos, but safer this way.
	 * the marker is like a token or cookie. Callers use it to
	 * help nexus find the right action.
	 */

	/* NOTE: work doesn't call back here (yet). */
/* call flow graph... 
 * main
 *   nexus
 *     earn -> nexus(ALONE) <-- nexus
 *     breed
 *        divvy
 *	  spawn
 *	    fork -> nexus(CHILD)
 *	  babysit <-- nexus
 *     recover -->nexus(?)  <-- nexus
 *       revive -> nexus(?)
 *     revive -->nexus(?)
 *     retire
 */

/* I/O matrix
SOUL	FROM	RELATE	ACTION(s)		after
----	----	---	------			-----
NEW	MAIN	ALONE	EARN			CLEAN, return
NEW	MAIN	CHILD	ERR			exit(1)
NEW	MAIN	PARENT	BREED			CLEAN, return
NEW	MAIN	FAMILY	ERR			exit(1)
NEW	FORK	ALONE	ERR			exit(1)
NEW	FORK	CHILD	EARN			RETIRE
NEW	FORK	PARENT	ERR			exit(1)
NEW	FORK	FAMILY	BREED			RETIRE
NEW	WORK	ALONE	EARN			return
NEW	WORK	CHILD	EARN			RETIRE
NEW	WORK	PARENT	BREED			RETIRE
NEW	WORK	FAMILY	BREED			RETIRE
REV	MAIN	ALONE	REVIVE, EARN		CLEAN, return
REV	MAIN	CHILD	EARN			RETIRE
REV	MAIN	PARENT	RECOVER			return
REV	MAIN	FAMILY	RECOVER			RETIRE
REV	FORK	ALONE	EARN			return
REV	FORK	CHILD	EARN			RETIRE
REV	FORK	PARENT	RECOVER/BREED		return
REV	FORK	FAMILY	RECOVER/BREED		RETIRE
REV	WORK	ALONE	EARN			return
REV	WORK	CHILD	EARN			RETIRE
REV	WORK	PARENT	RECOVER			RETIRE
REV	WORK	FAMILY	RECOVER			RETIRE
*/

/*
 * Yeah, that's messy and I've only written 1 line of code.
 * When is nexus called?
 *	- from main, either restarting or new
 *	- from work, when ncpu>1, also restarting or new
 *	- after a fork, as a sub-process, restart or new
 *	- possibly, when doing recovery ops, restart only
 * Problem is, the process tree is wiggly.
 */
}



void
initstuff(int D, int R)
{
	int i;

	DBG(DBG_INIT, ("zeroing out hbos %d bytes\n", sizeof(block_t)));
	bzero(&hbos, sizeof(block_t));

	hbos.D = D;	/* big-D and big-R are not 0-based. */
	hbos.R = R;
	hbos.r = 0;
	hbos.d = hbos.D - 1;
	/* set the degenerate case up first.
	 * d=1, r=1 can only have a val of 1 and a score/gap of 2.
	 * also, there must always be a val=1.
	 */
	barset(&(hbos.pl[0].sc[0].s),1);
	// should we set bit 0 as well??
	hbos.pl[0].sc[0].gap = 2;
	hbos.pl[0].sc[0].maxval = 1;
	hbos.pl[0].rval = 1;

	hbos.besties.b[0][0].score = 2;
	hbos.besties.b[0][0].vals[0] = 1;

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
	fprintf(stderr, "usage: %s [-vqtcVhb] [-H interval] [-C cnt] [-n cpus]"
		" [-l limit] [-F file ] [-D val] Darts Regions\n", me);
	fprintf(stderr, "usage: %s -R [-vqtcVhb] [-H interval] [-C cnt]"
		" [-l limit] [-F file ] [-D val]\n", me);
	fprintf(stderr, "\t-v: verbose. use multiple time for more.\n"
		"\t-q: quiet. turns off verbose.\n"
		"\t-t: display execution time.\n"
		"\t-c: show iteration counts\n"
		"\t-V: display version number.\n"
		"\t-h: show hash mark every interval iterations.\n"
		"\t-H interval: set the hash interval (default 1,000,000)\n"
		"\t-b: show best answers as found (progress report)\n"
		"\t-D value: set dbg value directly.\n"
		"\t-n cpus: fork n copies to work in parallel.\n"
		"\t-l limit: stop after limit iterations (checkpoint)\n"
		"\t-R restart execution from checkpoint file.\n"
		"\t-F filename: use filename for checkpoint file.\n"
		"\t-C interval: save checkpoint file every interval iterations.\n"
	);
}

int
main(int argc, char **argv)
{
	int D, R;
	char c;
	int verbose = 0;

	while ((c = getopt(argc, argv, "vqtcVhH:bD:l:RF:C:n:")) != -1) {
		switch(c) {
		case 'w':
			wipe = 1;
			break;
		case 'n':
			hbos.ncpus = strtol(optarg, NULL, 0);
			break;
		case 'v':
			verbose++;
			break;
		case 'q':	
			verbose = -1;
			break;
		case 't':
			dbg |= DBG_TIME;
			break;
		case 'c':
			dbg |= DBG_CTR;
			break;
		case 'V':
			dbg |= DBG_VER;
			break;
		case 'h':
			dbg |= DBG_HASH;
			break;
		case 'H':
			hashval = strtol(optarg, NULL, 0);
			break;
		case 'b':
			dbg |= DBG_PRO;
			break;
		case 'D':
			dbg = strtol(optarg, NULL, 0);
			dbg |= DBG_DBG;
			break;
		case 'l':
			dbg |= DBG_CPR;
			stop = strtol(optarg, NULL, 0);
			break;
		case 'F':
			cpfn = optarg;
			break;
		case 'R':
			restarting = 1;
			break;
		case 'C':
			dbg |= DBG_CPR;
			cpval = strtol(optarg, NULL, 0);
			break;
		case '?':
		default:
			usage(argv[0]);
			return 1;
			break;
		}
	}

	/* on restart, ignore D,R on cmd line. use from file. */
	if (! restarting ) {
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
	}

	if (cpval < hbos.R) {
		checker = cpval;
		cpval = 0;
	}
	if (cpfn == NULL) {
		cpfn = def_cpfn;
	}

	/* translate verbose into dbg flags. Note fall-through. */
	switch(verbose) {
		case 5: dbg = DBG_ALL;
		case 4: dbg |= DBG_DUMPS|DBG_DUMPF|DBG_DUMPV|DBG_FILLIN2;
		case 3: dbg |= DBG_MAIN|DBG_INIT;
		case 2: dbg |= DBG_MRF|DBG_FILLIN;
		case 1:	dbg |= DBG_PRO;
			break;
		case -1: dbg = DBG_NONE;
			break;
	}
	DBG(DBG_DBG, ("debug is 0x%lX\n", dbg));
	DBG(DBG_VER, ("version %s\n", VER));

	/* don't init on restart */
	if (! restarting) {
		initstuff(D, R);
	}
	
	if (restarting) {
		if (restart(cpfn) < 0) {
			printf("failed to restart. Abort.\n");
			exit(2);
		}
	}

	if (dbg & DBG_TIME) {
		hbos.stat.begin = gethrtime();
	}
	work(0);
	if (dbg & DBG_TIME) {
		hbos.stat.end = gethrtime();
		hbos.stat.runtime += (hbos.stat.end - hbos.stat.begin);
		hbos.stat.end = hbos.stat.begin = 0;
	}

	if (dbg & DBG_HASH) {
		printf("\n");
	}
	dumpscores(hbos.D);

	if (dbg & DBG_TIME) {
		if (dbg & DBG_CTR) {
			printf("performed %lu iters in %llu.%04llu seconds",
			    hbos.stat.iters,
			    (hbos.stat.runtime/1000000000ULL),
			    ((hbos.stat.runtime/1000000ULL)%1000ULL));
			if (hbos.stat.runtime >= 1000000000ULL) {
				printf(" %llu iters/sec",
				    hbos.stat.iters/(hbos.stat.runtime/1000000000ULL));
			}
			printf("\n");
		} else {
			printf("elapsed time %llu.%04llu seconds.\n", hbos.stat.runtime/1000000000ULL,  (hbos.stat.runtime/1000000)%1000);
		}
	}
	return 0;
}

