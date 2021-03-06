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
 * -C interval: checkpoint, every interval evals. Also when interrupted.
 * -R fname: use file to restart from checkpoint.
 * -n ncpus: work using ncpus at once.
 * -T seconds: checkpoint and quit after seconds realtime.
 */

#define VER	"0.9.5"

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
#define DBG_SPAWN	0x00010000
#define DBG_TASK	0x00020000
#define DBG_CHILD	0x00040000
#define DBG_CUTOFF	0x00080000
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
#define	FNSZ	64
#define	FNFMT	"darts_r%02d_%04d_%05d"
#define MAXR	40
#define	MAXD	6
#define MATURE	2	/* reproductive age */

const int def_limits[MAXD] = { 40, 40, 40, 30, 20, 10 };

/* options */
uint64_t	hashval = 100000;	// iters per hash
uint64_t	stop = 0;		// iter limit
uint64_t	cpval = 0;		// checkpoint interval
int		checker = -1;		// checkpoint on r level
char		*cpfn = NULL;		// checkpoint filename
char		def_cpfn[] = CPFN;	// default cpfn
int		restarting = 0;		// -R - restart from checkpoint
long		dbg = DBG_NONE;		// debug/verbose and other flags
int		wipe = 0;		// wipe - clean up and quit
int		ncpus = 0;		// -n how many processes
int		timeout = 0;		// runtime limit (in seconds)
int		wall = 0;		// generic CPR/quit flag

#define		HIT	1		/* hit the wall (checkpoint) */
#define		QUIT	3		/* quit after checkpoint */

/* this is the data for a particular (d,r,{V[r]}). */
typedef struct _sc {
	bar_t s;		/* bit array. */
	int gap;		/* also score. first uncovered number */
	int maxval;		/* max val covered. */
} sc_t;

/* individual value */
typedef struct _val {
	int v;
	int hi;
	int lo;
	int from;
	int to;
} val_t;

/* each "plane" is a number of darts. */
typedef struct _plane {
	sc_t sc[MAXD];		/* one set for each number of darts */
	val_t val;		/* value and ranges */
} plane_t;

/* expanded  */
/* vals is an "answer" for a given d,r.  There's a single global one
 * in hbos, and a set of them for Besties.  For each val, have to
 * say what the range is.
 */

typedef struct _vals {
	int score;		/* where is the first gap? (sc.gap)*/
	val_t vals[MAXR+1];	/* value of each region. */
} vals_t;

typedef struct _task {
	int id;		// who is this for
	int lvl;	// starting at R lvl
	int rhi;	// upper (pl.rhi)
	int rlo;	// lower (pl.rlow)
	int from;	// start here (Divvy)
	int to;		// end here (Divvy)
	int cpus;	// how many cpus to use (Divvy)
	char fname[FNSZ];	// non-suffix name for us
} task_t;

/* base task. */
const task_t task0 = { 0, 0, 1, 1, 1, 1, 0, CPFN };

/* a 1 elemnt struct lets us use assignment instead of memcpy */
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

/* saved by child at Retirement,to be picked up by parent */
typedef struct _result {
	task_t task;
	best_t besties;
	stats_t stat;
} result_t;

/* values for status */
#define SS_NEW	0	/* blank and empty */
#define SS_INIT	1	/* initialized */
#define SS_SET	2	/* ready to run. */
#define SS_RUN	3	/* Spawned, off chewing data */
#define SS_REV	4	/* reviving-don't disturb */
#define	SS_EXIT	5	/* called exit with non-zero */
#define	SS_DEAD	6	/* hit a signal, and keeled over */
#define SS_DONE	7	/* all done. (exited with 0) */
#define SS_READ	9	/* results in hbos (of parent). */
#define SS_GOOD	9	/* results are good. (afawk) */
#define SS_MELD	10	/* results have been merged. */
#define SS_ERR	-1	/* ob */

/* made by Divvy. one for each process. used by Spawn. */
typedef struct _slot {
	task_t task;	/* given to child. */
	int status;	/* tracking child */
	int rv;		/* from process */
	pid_t cpid;	/* from fork */
	result_t res;	/* child fills this out. copied to hbos.task. */
} slot_t;

typedef struct _block {
	plane_t pl[MAXR+1];	/* one for each number of regions. */
	vals_t vals;			/* current set of r values. */
	int limits[MAXD];		/* dont go on forever */
	int D;		// number of darts
	int R;		// number of regions
	int d;		// current d value
	int r;		// current value of r 
	int ncpus;		// all cpus for this run
	int subcpus;		// all cpus for peers
	int mycpus;		// cpus assigned to me.
	int startdepth;		// know when to Retire
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
 * darts are 0-based. if there are D darts, they are numbered 0..D-1.
 * regions are also 0 based, R regions numberes 0 .. R-1.
 * regions and darts are printed using whole numbers 1..d or 1..r.
 * the values are 1-based.  There's an implicit 0-value region, which
 * corresponds to a dart missing the dart board, but we don't bother to
 * track it.
 *
 * To find the optimal solution we need to maximize the score S for
 * any D darts and R regions.  The brute-force method would be to
 * generate all possible combinations of values for the regions
 * and calculate the score for each one.  This takes too long.
 * Instead we need an inductive/recursive method that can be worked
 * in parallel.
 * 
 * Given the problem with parameters D,R, we need to come up with
 * the set of values for the regions V[] such that S(D,R,V[]) is the
 * maximum. In order to compute the score, we must construct the set of
 * numbers {B} that can be reached using the D,R,V[] values.  The score
 * then is the first missing number or "gap" in {B}.
 * 
 * Induction requires a base case.  We have two parameters, so we need
 * a foundation for both. With 1 dart and 1 region, there is only 1 value,
 * and it has to be 1, giving a gap at 2 (score).  This is the foundation case.
 * Incrementing the number of darts (n darts, 1 region) just adds d to the
 * set {B}, and increments the score (S=d+1).  Now we can calculate solutions
 * for any {B(1...D,1,[1])}={1...D}. Conversely, with 1 dart it is easy to add
 * another region r with value V[r], by simply adding V[r] to {B}. Therefore
 * {B(1,R,V[])}={V[]}.  So now we have base cases for D=1 or R=1.
 * 
 * For the general case consider that we have the set {B(D-1,R,V[])}.
 * What happens if we add a dart?  Imagine a dartboard made from V[] that
 * has D-1 darts in it already.  What happens if we throw another dart?
 * We can add any of the values in V[] to our total (including the case
 * where we miss the dartboard).  This corresponds to extending B(D-1,R,V[])} by
 * adding all v in V[1..R] to every value in {B}.  If the size of {B} is large,
 * then this can be an expensive operation.  However, if we already have
 * the sub-set {B(D,R-1,V[1..R-1])}, then it includes all the combinations that
 * do not involve V[R].  In that case we only have to add V[R] to each element
 * in {B} to get the full set.
 * Note that the operation to add V[R] to each element in {B} is equivalent to
 * a "shift-or" if {B} is represented as a bit-vector:  {B} <<|= v.
 * This gives us:
 * {B(D,R,V[1..R]} = ({B(D-1,R,V[1..R])} <<| v) | {B(D,R-1,V[1..R-1])}.
 * 
 * With the base cases and an inductive rule above, we see that any set
 * {B(D,R,V[1..R])} can be constructed by reducing D down to 1, and then
 * iterating (or recursing) up until we get back to D.  We reach R then by
 * starting with R=1, and increment upwards.  If we have a matrix M[D,R] of
 * sets {B(1..D,1..R,V[])}, then the algorithm can fill out a complete
 * matrix for all values of 1..D and 1..R while finding the set for D,R.
 * This means that we can get all the sub-solutions for a given V[] while
 * calculating the solution for D,R.
 * 
 * Now, how to determine the values in V[]?  Finding the optimal
 * solution requires exploring all possible combinations of values
 * that will fit in V[1..R].  Fortunately, the range of values for any one
 * region can be pruned quite a bit. Given a sub-set {B(D,R-1V[1..R-1])}
 * with score or gap S, then adding a region whose value V[R] is greater than
 * the gap will not effect the gap at all. Those values can be excluded.  So
 * the upper bound on V[R] is S({B(D,R-1V[1..R-1])}).  If we impose an
 * order on the regions, such that all V[r-1] < V[r], and also order our
 * dart "throws" by value, then all values at or under V[R-1] will already
 * be covered.  So we set our lower value to V[R-1]+1.  These two limits
 * also have convenient base cases, since S(D,1,V[1])=D+1 and V[1] is
 * always 1.  Iterating over these bounds for V[1..R], and retaining
 * the highest score for all 1..D,1..R,V[1..R]  will yeild the optimal
 * solution.
 * 
 * An algorithm A(D,R) can then produce a matrix of optimal solutions
 * O[D,R] containing the score S and set of values V[] such that S is the
 * maximum for the given R and D.  A(D,R) -> O[D,R]{S, {B(D,R,V[1..R])}}.
 * Internally A contains a matrix M of all {B(1..D,1..R,V[1..R])}.
 * It iterates (or recurses) r=1..R, at each r finding the limits vlo and vhi
 * by using M[d,r-1,V[r-1]].  For each v=vlo..vhi, it recursively fills out
 * M[d-1,r,V[1..r]] by going down to the base case of d=1 and on return
 * using the inductive rule to build up the matrix M.
 * After that, it can then move up to the next r, repeating the procedure.
 * When it finishes iterating a range of v for a particular level r, it
 * pops down to r-1 and moves to the next value of v for it, repeating
 * the procedure.
 * As each set {B(d,r,V)} is calculated, the score S can be easily found.
 * By comparing each S(d,r,V) to the O[d,r].S, the optimal values for
 * all d and r can be found while finding O(D,R).
 * 
 * Note: an efficient way to implement {B} is to use a bit-vector.
 * Then adding two sets becomes a bitwise OR, and the shift-or
 * can be literally implemented as  B = (B<<v)|B.
 *
 * In order to work in parallel, we can note that when going from
 * vlo to vhi at each r, the solutions are independent and can therefore
 * be found in parallel.  Threading is not an appropriate model for this,
 * since the matrix M is singular and changes.  However, a fork would
 * be exactly right, since it copies the exisitng data (M).  To work
 * in parallel, the algorithm can at particular values of r, divide
 * the range of v among n processes, and let them each return the
 * optimal matrix O for that particular sub-range of v.  WHen the
 * sub-processes return, their results can be merged together to get
 * the solutions for all of v, retaining the highest score for each
 * individual slot in O[D,R].
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
	for (i = 0; i < R; i++) {
		printf("%d, ", hbos.vals.vals[i].v);
	}
	printf("%d\n", hbos.vals.vals[i].v);
}

/* at the end, print out everything we found. */
void
dumpscores(best_t besties, int D, int R)
{
	int d, r, i;

	printf("best scores found this run:\n");
	for (d=0; d < D; d++) {
		for (r = 0; r < R; r++) {
			printf("%d darts, %d regions: score=%d; vals=", d+1, r+1, besties.b[d][r].score);
			for (i=0; i <= r; i++) {
				printf("%d,", besties.b[d][r].vals[i].v);
			}
			printf("\n");
		}
	}
}

void
dumpmore(int d, int r)
{
	int i;
	sc_t *scores;

	scores = &(hbos.pl[r].sc[d]);

	printf("O[d=%d,r=%d]:S=%d,V={", d+1, r+1, scores->gap);
	for (i = 0; i < r+1; i++) {
		printf("%d ", hbos.vals.vals[i].v);
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
	int lead = 0;
	int melds = 0;

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
			/* otherwise, try to do a Meld */
			assert(hbos.slots[lead].res.task.to +1 ==
			    hbos.slots[i].res.task.from);
			for (j = 0; j <= MAXD ; j++) {
				for (k = 0; k < MAXR ; k++) {
					if (hbos.slots[i].res.besties.b[j][k].score > hbos.slots[lead].res.besties.b[j][k].score) {
						hbos.slots[lead].res.besties.b[j][k] = hbos.slots[i].res.besties.b[j][k];
					}

				}
			}
			hbos.slots[lead].res.stat.iters += hbos.slots[i].res.stat.iters;
			hbos.slots[lead].res.stat.runtime += hbos.slots[i].res.stat.runtime;
			hbos.slots[lead].res.stat.checks += hbos.slots[i].res.stat.checks;
			hbos.slots[lead].res.task.to = hbos.slots[i].res.task.to;
			hbos.slots[i].status = SS_MELD;
			melds++;
DBG(DBG_SPAWN, ("melded %d to %d\n", i, lead));
		}
	}
	if ((lumps==1) && (gaps == 0) && (lead == 0) && (melds > 0)) {
		return 0;
	}
	if (lumps > 0) return lumps;
	return -gaps;
}

/* a child has died. attempt to bring it back to life.
 * Child must be EXIT or DEAD.
 * returns number of revived, 0 if nothing revivable, <0 on erorr.
 * pass in -1 to revive everything possible.
 * For now, just reSpawn. Later on, want to use CPR.
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
	int rv;


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
				//return -2;
				break;
			}
		} else if (p == 0) {
			/* doesn't make sense. */
			printf("weird pid from wait\n");
			continue;
//			return -3;
		}
		/* got a pid. */
		for (sub = -1, i = 0; i < hbos.kids; i++) {
			if (hbos.slots[i].cpid == p) {
				sub = i;
				break;
			}
		}
		if (sub < 0) {
			DBG(DBG_SPAWN, ("unknown child process %d: ignored\n", p));
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
			continue;
		} else if (hbos.slots[sub].rv > 0) {
			/* possible to try again */
			hbos.slots[sub].status = SS_EXIT;
			revive(sub);
			continue;
		} else { 	/* just right */
			// add in the info
			hbos.slots[sub].status = SS_DONE;
DBG(DBG_SPAWN, ("kid %d has done well\n", sub));
			notdone--;
			getres(sub);
		}
	}
DBG(DBG_SPAWN, ("no more kids to raise.\n"));
	/* get what we were after... */
	rv = meld();
	if (rv == 0) {
		/* all results together, in slot 0. */
		/* what to copy? well, keep the besties. */
		hbos.besties = hbos.slots[0].res.besties;
		/* XXX TODO: make stats work. */
	} else {
		DBG(DBG_SPAWN, ("meld returned %d\n", rv));
	}
	/* cleanup. */
	free(hbos.slots);
	hbos.slots = NULL;

	if (rv == 0) return 0;
	return notdone;
}

/*
 * create kids. All done by what's in the slots.
 * Normally use -1 id to Spawn all. Can give >0 id to Spawn 1.
 * returns -1 = error, 0 if we are a forked child, and >1 if parent,
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
		DBG(DBG_SPAWN, ("spawning one %d\n", id));
	} else {
		DBG(DBG_SPAWN, ("spawning all %d\n", n));
	}
	fflush(stdout);
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
			if (checker) {
				checker = hbos.r;
			}
			/* children should be seen and not heard */
			if (dbg & DBG_CUTOFF) {
				dbg = DBG_NONE;
			}
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
	/* proud parents have Spawned */
	DBG(DBG_SPAWN, ("Spawned %d kids\n", rv));
	return rv;
}

/*
 * when a sub-process has finished its task, it Retires.
 * it saves it's results and exits.
 */
void
retire()
{
	int fd;
	char resfn[FNSZ];
	size_t wrv;
	result_t res;

	res.task = hbos.task;
	res.besties = hbos.besties;
	res.stat = hbos.stat;

	/* construct filename */
	strcpy(resfn, hbos.task.fname);
	strcat(resfn, ".res");

	fd = open(resfn, O_WRONLY|O_CREAT|O_TRUNC, 0660);
	if (fd < 0) {
		perror("open results file");
		exit(1);
	}
	wrv = write(fd, &res, sizeof(result_t));
	if (wrv != sizeof(result_t)) {
		perror("write res");
		close(fd);
		exit(2);
	}
	close(fd);
	/* task complete. */
	DBG(DBG_CHILD, ("child %d got this\n", res.task.id)) {
		dumpscores(res.besties, hbos.D, hbos.R);
	}
	exit(0);
}

/* divide up task. Allocates and initializes slots for us.
 * returns: <0=error, >0=#slots created, =0 nothing to do.
*/
int
divvy(int depth)
{
	int i, j, k, slice;
	int n, range;

	/* first, set the limits on the vals */
	hbos.vals.vals[depth].lo = hbos.pl[depth].val.lo;
	hbos.vals.vals[depth].hi = hbos.pl[depth].val.hi;

	/* can't Spawn if not old (deep) enougn */
	if ((hbos.task.cpus <= 1) || (depth < MATURE)) {
		/* no extra cpus. do the whole thing. */
		hbos.vals.vals[depth].from = hbos.pl[depth].val.lo;
		hbos.vals.vals[depth].to = hbos.pl[depth].val.hi;
		return 0;
	}

	DBG(DBG_SPAWN, ("divvy says yes\n"));
	/* we are going to do it. Set up tasks for each child */
	assert(hbos.slots == NULL);

	range = (hbos.vals.vals[depth].to - hbos.vals.vals[depth].from) + 1;
	if (range > hbos.task.cpus) {
		/* more r's than cpus. use all of them (cpus) */
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
	DBG(DBG_SPAWN, ("making %d slots with %d cpus %d range\n", n, j, k));
	for (i = 0; i < n; i++) {
		hbos.slots[i].task.id = i;
		hbos.slots[i].task.lvl = depth; 
		hbos.slots[i].task.rhi = hbos.vals.vals[depth].hi;
		hbos.slots[i].task.rlo = hbos.vals.vals[depth].lo;
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
			if (i == 0) {
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
		hbos.slots[i].status = SS_SET;
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

int
recover()
{
	/* Not implemented yet. */
	return -1;
}



/* hi ho hi ho
 * We need to know start depth to know when to quit.
 * modified for Spawning.  Restart not fully working.
 */
/*
 * a bit hard here... even without restart.  Use task0 as an anchor,
 * but don't Spawn before maturity.  If restarting most things will
 * already be set. Restart+Spawn=recover (which I haven't written yet).
 * Unless restarting, depth and hbos.r should be the same.
 */
int
work2(int depth)
{
	sc_t *scores;
	int rv;

	if (!restarting) {
		assert(depth == hbos.r);
	}
	if ((hbos.d < 0) || (depth < 0)) return -1;
	if ((hbos.d == 0) && (depth == 0)) return -1;
	if (depth >= hbos.limits[hbos.d]) return -1;

/*
	if (restarting && (depth > hbos.r)) {
		restarting = 0;
	}
*/
	if (restarting) {
		DBG(DBG_MRF, ("-> restart depth=%d, r=%d\n", depth+1, hbos.r+1));
	} else {
		DBG(DBG_MRF, ("-> call with d=%d r=%d\n", hbos.d+1, hbos.r+1));
	}
	if (! restarting) {
		/* under restart, should already be set. */
		if (hbos.r == 0) {
			hbos.pl[hbos.r].val.lo = hbos.pl[hbos.r].val.hi = 1;
		} else  {
			hbos.pl[hbos.r].val.lo = hbos.pl[hbos.r-1].val.v + 1;
			hbos.pl[hbos.r].val.hi = hbos.pl[hbos.r-1].sc[hbos.d].gap;
		}
		hbos.pl[hbos.r].val.v = hbos.pl[hbos.r].val.lo;
		/* those are the outer limits. copy to vals */
		hbos.vals.vals[depth].lo = hbos.pl[depth].val.lo;
		hbos.vals.vals[depth].hi = hbos.pl[depth].val.hi;
		/* see if we are tasked. */
		if (hbos.task.lvl == depth) {
			hbos.vals.vals[depth].from = hbos.task.from;
			hbos.vals.vals[depth].to = hbos.task.to;
		} else {
			hbos.vals.vals[depth].from = hbos.vals.vals[depth].lo;
			hbos.vals.vals[depth].to = hbos.vals.vals[depth].hi;
		}

		/* sometimes there's nothing to do */
		if (hbos.vals.vals[depth].from > hbos.vals.vals[depth].to) {
DBG(DBG_MRF, ("<- returning early d=%d,r=%d, nothing to do\n", hbos.d+1, hbos.r+1));
			return 0;
		}
	} /* end ! restarting */
	/* now see if we need to Spawn. */
	rv = divvy(depth);
	if (rv > 0) {
		if (restarting) {
			rv = recover();
			DBG(DBG_CPR, (" at r=%d recover %d\n", depth+1, rv));
		} else  {
			DBG(DBG_SPAWN, (" at r=%d Spawning %d\n", depth+1, rv));
			rv = spawn(-1); /* Spawn!Spawn!Spawn! */
		}
		if (rv != 0) {
			/* parents get to babysit. */
			return babysit();
		} else {
			/* set bounds by task. */
			hbos.vals.vals[depth].from = hbos.task.from;
			hbos.vals.vals[depth].to = hbos.task.to;
		}
	}

	DBG(DBG_MRF, ("[id=%d] Iterate (d=%d)r=%d val from %d to %d\n", hbos.task.id, hbos.d, depth+1, hbos.vals.vals[depth].from, hbos.vals.vals[depth].to));
	
	if (! restarting) {
		if (depth == checker) {
			wall = HIT;
		}
		hbos.pl[depth].val.v = hbos.vals.vals[depth].from;
	}
	for (/* already set */;
	    hbos.pl[depth].val.v <= hbos.vals.vals[depth].to;
	    hbos.pl[depth].val.v++) {
		/* here is where we CPR */
		if ((! restarting) && (hbos.stat.iters > 0)) {
			/* no checkpoint under restart */
			if (cpval && ((hbos.stat.iters % cpval) == 0)) {
				DBG(DBG_CPR, ("checkpoint at %lu iterations (depth=%d)\n", hbos.stat.iters, depth));
				wall = HIT;
			}
			if (stop && (hbos.stat.iters >= stop)) {
				stop = 0;
				DBG(DBG_CPR, ("checkpoint and stop at %lu iterations(depth=%d)\n", hbos.stat.iters, depth));
				wall = QUIT;
			}
		}
		if (wall >= HIT) {
			checkpoint(cpfn);
			if (wall >= QUIT) {
				exit(0);
			}
			wall = 0;	// reset.
		}
		/* restart where we left off */
		if ((restarting) && (depth == hbos.r)) {
			restarting = 0;
		}
		if (!restarting) {
			hbos.vals.vals[hbos.r].v = hbos.pl[hbos.r].val.v;
			fillin(hbos.d, hbos.r, hbos.pl[hbos.r].val.v);
			DBG(DBG_DUMPF, ("frame %d\n",hbos.r )) {
				dumpframe(hbos.r,hbos.d+1);
			}
			if (hbos.r+1 >= hbos.limits[hbos.d]) {
				continue;
			}
			hbos.r++;
		}
		DBG(DBG_MRF, ("recursing with d=%d,r=%d V[r]=%d ", hbos.d+1, depth+1+1, hbos.pl[depth].val.v)) {
dumpvals(hbos.r);
}
		work2(depth+1);
		hbos.r--;
		hbos.stat.iters++;
		assert(!restarting);
		/* see if we can Retire. */
		if ((depth > 0) && (depth == hbos.task.lvl-1)) {
			retire();
		}

		if (dbg & DBG_HASH) {
			if ((hbos.stat.iters % hashval) == 0) {
				printf("#");
				fflush(stdout);
			}
		}
	}
	DBG(DBG_DUMPV, ("current vals ")) {
		dumpvals(hbos.r);
	}
	DBG(DBG_CTR, ("performed %lu iterations\n", hbos.stat.iters));
	DBG(DBG_MRF, ("<- returning from d=%d,r=%d\n", hbos.d+1, hbos.r+1));
	return 0;
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
	hbos.pl[0].val.v = 1;

	hbos.besties.b[0][0].score = 2;
	hbos.besties.b[0][0].vals[0].v = 1;

	/* task0 covers the base */
	hbos.task = task0;
	strcpy(hbos.task.fname, cpfn);
	hbos.task.cpus = ncpus;
	hbos.ncpus = ncpus;

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

	/* set timeout limit */
	alarm(timeout);

	/* might be good enough for now. */
	DBG(DBG_INIT, ("init complete\n"));
}

void
usage(char *me)
{
	fprintf(stderr, "usage: %s [-wvqtcVhb] [-H interval] [-C cnt] [-n cpus]"
		"[-T secs] [-l limit] [-F file ] [-D val] Darts Regions\n", me);
	fprintf(stderr, "usage: %s -R [-wvqtcVhb] [-H interval] [-C cnt]"
		"[-T secs] [-l limit] [-F file ] [-D val]\n", me);
	fprintf(stderr, "\t-v: verbose. use multiple times for more.\n"
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
		"\t-T secs: checkpoint and quit after secs runtime\n"
		"\t-w: wipe (cleanup) any residual files and exit\n"
	);
}

void
quithdlr(int sig)
{
	wall = QUIT;
}

/* whole program or process setup. always done. */
void
setup()
{
	struct sigaction qsa;
	/* signal handlers. Do a graceful shutdown. */
	qsa.sa_handler = quithdlr;
	sigemptyset(&qsa.sa_mask);
	qsa.sa_flags = 0;

	sigaction(SIGINT, &qsa, NULL);
	sigaction(SIGHUP, &qsa, NULL);
	sigaction(SIGTERM, &qsa, NULL);
	sigaction(SIGALRM, &qsa, NULL);
}

int
main(int argc, char **argv)
{
	int D, R;
	char c;
	int verbose = 0;

	while ((c = getopt(argc, argv, "vqtcVhH:bD:l:RF:C:n:T:")) != -1) {
		switch(c) {
		case 'T':
			timeout = strtol(optarg, NULL, 0);
			break;
		case 'w':
			wipe = 1;
			break;
		case 'n':
			ncpus = strtol(optarg, NULL, 0);
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

	if (restarting) {
		DBG(DBG_CPR, ("restarting\n"));
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

	/* always call setup */
	setup();

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
	work2(0);
	if (dbg & DBG_TIME) {
		hbos.stat.end = gethrtime();
		hbos.stat.runtime += (hbos.stat.end - hbos.stat.begin);
		hbos.stat.end = hbos.stat.begin = 0;
	}

	if (dbg & DBG_HASH) {
		printf("\n");
	}
	dumpscores(hbos.besties, hbos.D, hbos.R);

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

