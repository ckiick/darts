

/* test file for bit array code (bar) */

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <inttypes.h>

#include "bar.h"
/* include it twice to ensure no leaks. */
#include "bar.h"

/* useful macros */
#define ISP2(x)	(((x) & ((x) - 1)) == 0)
#ifndef UINT_MAX
#define UINT_MAX	(~0U)
#endif

#define vprintf(lvl, fmt, ...)	\
	if (verbose >= (lvl)) printf(fmt, ##__VA_ARGS__)

#define VERB(lvl, fmt, ...)     \
        if (( verbose >= lvl) ? 1 + printf(fmt, ##__VA_ARGS__) : 0)

#define	VNONE	0
#define VERR	1
#define VVERB	2
#define VDEBUG	3
#define VNOISY	4
#define VDUMP	5

int verbose = 1;

/* some basic stuff. */
int
verify()
{
	bar_t bar1;
	bar_t *barp;

	/* how to you assert power of two? */
	assert(ISP2(sizeof(bar_t)));
	bzero(&bar1, sizeof(bar_t));
	assert(barlen(&bar1) == 0);
	barp = &bar1;
	assert(barlen(barp) == 0);

	return 0;
}

/* naturally, we test them together. */
/* returns 0 on success, aborts on error due to assertion failure. */
int
test_baralloc_free()
{
	uint_t len;
	uint_t testlens[] = { 0, 0, 1, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33,
			63, 64, 65, 127, 128, 129, 255, 256, 257, 1023,
			2048, 4096, 8192, 3000, 555, 177, 37, 5, 1, 0,
			UINT_MAX}; 
	int i;
	bar_t *barp1 = NULL, *barp2 = NULL;

	/* loop through the testlens and allocate/free each one. */
	for (i = 0; testlens[i] < UINT_MAX; i++) {
		len = testlens[i];
		barp1 = baralloc(len);
		assert(barp1 != NULL);
		assert(barlen(barp1) == len);
		barp2 = baralloc(len);
		assert(barp2 != NULL);
		assert(barlen(barp2) == len);
		barfree(barp1);
		barfree(barp2);
		barp1 = NULL;
		barp2 = NULL;
	}
	return 0;
}

int
test_barsize()
{
	uint_t len;
	uint_t testlens[] = { 0, 1, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33,
			 0, 63, 64, 65, 127, 128, 129, 255, 256, 257, 513, 0,
			 1023, 2048, 4096, 8192, 0, 3000, 555, 177, 37, 5, 1,
			 0, 1, 0, 1, 0,
			UINT_MAX}; 
	int i;
	bar_t bar1;
	bar_t *barp2 = NULL;

	bzero(&bar1, sizeof(bar_t));
	for (i = 0; testlens[i] < UINT_MAX; i++) {
		len = testlens[i];
		barp2 = barsize(&bar1, len);
		assert(barp2 != NULL);
		assert(barp2 == &bar1);
		assert(barlen(&bar1) == len);
		barp2 = NULL;
	}

	return 0;
}

int
test_barget_set_clr()
{
	/* implicit testing of barsize, used internally. */
	bar_t bar1;
	int rv;
	uint_t maxtb = 3484;	/* picked at random */
	int i, j;

	bzero(&bar1, sizeof(bar_t));

	assert(barlen(&bar1) == 0);
	/* first test default get behaviour */
	for (i = 0; i <= maxtb; i++) {
		rv = barget(&bar1, i);
		vprintf(VDEBUG, "barget for bit %d returns %d, expected %d\n", i, rv, 0);
		assert(rv == 0);
		assert(barlen(&bar1) == 0);
	}
	/* now set some bits. Observe size */
	for (i = 0; i <= maxtb; i++) {
		rv = barset(&bar1, i);
		vprintf(VDEBUG, "barset for bit %d returns %d, expected %d\n", i, rv, 0);
		assert(rv == 0);
		assert(barlen(&bar1) == i+1);
		rv = barget(&bar1, i);
		vprintf(VDEBUG, "barget for bit %d returns %d, expected %d\n", i, rv, 1);
		assert(rv == 1);
		assert(barlen(&bar1) == i+1);
	}
	/* clear them. */
	for (i = 0; i <= maxtb; i++) {
		rv = barclr(&bar1, i);
		vprintf(VDEBUG, "barclr for bit %d returns %d, expected %d\n", i, rv, 1);
		assert(rv == 1);
		assert(barlen(&bar1) >= i);
		rv = barget(&bar1, i);
		vprintf(VDEBUG, "barget for bit %d returns %d, expected %d\n", i, rv, 0);
		assert(rv == 0);
	}
	/* doit again backwards */
	for (i = maxtb; i >=0; i--) {
		rv = barset(&bar1, i);
		vprintf(VDEBUG, "barset for bit %d returns %d, expected %d\n", i, rv, 0);
		assert(rv == 0);
	}
	for (i = maxtb; i >=0; i--) {
		rv = barget(&bar1, i);
		vprintf(VDEBUG, "barget for bit %d returns %d, expected %d\n", i, rv, 1);
		assert(rv == 1);
	}
	for (i = maxtb; i >=0; i--) {
		rv = barclr(&bar1, i);
		vprintf(VDEBUG, "barclr for bit %d returns %d, expected %d\n", i, rv, 1);
		assert(rv == 1);
	}
	for (i = maxtb; i >=0; i--) {
		rv = barget(&bar1, i);
		vprintf(VDEBUG, "barget for bit %d returns %d, expected %d\n", i, rv, 0);
		assert(rv == 0);
	}
	/* now do some jumping around. */
	for (j = 2; j < 17; j++) {
		for (i = 0; i < maxtb; i += j) {
			rv = barset(&bar1, i);
		}
	}
	for (j = 2; j < 17; j++) {
		for (i = 0; i < maxtb; i += j) {
			rv = barget(&bar1, i);
			vprintf(VDEBUG, "barget for bit %d returns %d, expected %d\n", i, rv, 1);
			assert(rv == 1);
		}
	}
	for (j = 2; j < 17; j++) {
		for (i = 0; i < maxtb; i += j) {
			rv = barclr(&bar1, i);
		}
	}
	for (j = 2; j < 17; j++) {
		for (i = 0; i < maxtb; i += j) {
			rv = barget(&bar1, i);
			vprintf(VDEBUG, "barget for bit %d returns %d, expected %d\n", i, rv, 0);
			assert(rv == 0);
		}
	}
	rv = barget(&bar1, maxtb * 1497);
	assert(rv == 0);
	assert(barlen(&bar1) <= maxtb + 1);

	return 0;
}

int
test_barfill()
{
	bar_t *barp1;
	bar_t bar1;
	int i;
	uint_t b;
	int tlen = 365;

	bzero(&bar1, sizeof(bar_t));

	barp1 = barsize(&bar1, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar1) == tlen);
	for (i = 0; i < tlen; i++) {
		b = barget(&bar1, i);
		assert(b == 0);
	}
	barfill(&bar1, 1);
	assert(barlen(&bar1) == tlen);
	for (i = 0; i < tlen; i++) {
		b = barget(&bar1, i);
		assert(b == 1);
	}
	barzero(&bar1);
	assert(barlen(&bar1) == tlen);
	for (i = 0; i < tlen; i++) {
		b = barget(&bar1, i);
		assert(b == 0);
	}
	return 0;
}

int
test_barflip()
{
	bar_t *barp1;
	bar_t bar1;
	int i;
	uint_t b;
	int tlen = 1111;

	bzero(&bar1, sizeof(bar_t));
	barp1 = barsize(&bar1, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar1) == tlen);

	/* all zeros init. set every other bit to 1 */
	for (i = 0; i < tlen; i += 2) {
		b = barset(&bar1, i);
		assert(b == 0);
	}
	assert(barlen(&bar1) == tlen);
	/* flip all the bits */
	for (i = 0; i < tlen; i++) {
		b = barflip(&bar1, i);
		assert(b == i%2);
	}
	assert(barlen(&bar1) == tlen);
	/* every other bit should now be 0 */
	for (i = 0; i < tlen; i += 2) {
		b = barget(&bar1, i);
		assert(b == 0);
	}
	/* the rest should be 1 */
	for (i = 1; i < tlen; i += 2) {
		b = barget(&bar1, i);
		assert(b == 1);
	}
	/* flip every other bit */
	for (i = 0; i < tlen; i += 2) {
		b = barflip(&bar1, i);
		assert(b == 1);
	}
	/* and now they should all be 1 */
	for (i = 0; i < tlen; i++) {
		b = barget(&bar1, i);
		assert(b == 1);
	}
	/* flip them all back to 0 */
	for (i = 0; i < tlen; i++) {
		b = barflip(&bar1, i);
		assert(b == 0);
	}
	for (i = 0; i < tlen; i++) {
		b = barget(&bar1, i);
		assert(b == 0);
	}

	return 0;
}

int
test_barnot()
{
	bar_t *barp1;
	bar_t bar1, bar2, bar3;
	int i;
	uint_t b;
	int tlen = 87;
	int rv;

	bzero(&bar1, sizeof(bar_t));
	bzero(&bar2, sizeof(bar_t));
	bzero(&bar3, sizeof(bar_t));

	/* what does not of nothing do ? */
	rv = barnot(&bar2, &bar1);
	assert(barlen(&bar1) == 0);
	assert(barlen(&bar2) == 0);

	barp1 = barsize(&bar1, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar1) == tlen);

	/* all zeros init. set every other bit to 1 */
	for (i = 0; i < tlen; i += 2) {
		b = barset(&bar1, i);
		assert(b == 0);
	}
	assert(barlen(&bar1) == tlen);
	rv = barnot(&bar2, &bar1);
	assert(rv == tlen);
	assert(barlen(&bar1) == tlen);
	assert(barlen(&bar2) == barlen(&bar1));

	/* every other bit should now be 0 */
	for (i = 0; i < tlen; i += 2) {
		b = barget(&bar2, i);
		assert(b == 0);
	}
	/* the rest should be 1 */
	for (i = 1; i < tlen; i += 2) {
		b = barget(&bar2, i);
		assert(b == 1);
	}
	for (i = 1; i < tlen; i++) {
		assert(barget(&bar1, i) != barget(&bar2, i));
	}
	/* not again */
	assert(barlen(&bar2) == tlen);
	rv = barnot(&bar3, &bar2);
	assert(rv == tlen);
	assert(barlen(&bar2) == barlen(&bar3));

	/* and now compare to original */
	for (i = 0; i < tlen; i++) {
		assert(barget(&bar3, i) == barget(&bar1, i));
	}

	return 0;
}

/*
 * later on we'll add some verbiage and maybe debug stuff.
 */
int
main(int argc, char **argv)
{
	int rv;

	vprintf(VVERB, "starting bar test program\n");

	vprintf(VVERB, "basic verification test\n");
	rv = verify();
	if (rv != 0) {
		vprintf(VERR, "verification test failed\n");
		return rv;
	}
	vprintf(VVERB, "verification passed\n");

/* we have a test function for each interface. */

	vprintf(VVERB, "test baralloc_free\n");
	rv = test_baralloc_free();
	if (rv != 0) {
		vprintf(VERR, "baralloc_free test failed\n");
		return rv;
	}
	vprintf(VVERB, "baralloc_free test passed\n");

	vprintf(VVERB, "test barsize\n");
	rv = test_barsize();
	if (rv != 0) {
		vprintf(VERR, "barsize test failed\n");
		return rv;
	}
	vprintf(VVERB, "barsize test passed\n");

	/* we don't explicitly test barlen becuase it's used in all the
	 *other tests.
	 */

	/* we can create bars, see if we can use the actual bits. */
	vprintf(VVERB, "test barget_set_clr\n");
	rv = test_barget_set_clr();
	if (rv != 0) {
		vprintf(VERR, "barget_set_clr test failed\n");
		return rv;
	}
	vprintf(VVERB, "barget_set_clr test passed\n");

	vprintf(VVERB, "test barfill\n");
	rv = test_barfill();
	if (rv != 0) {
		vprintf(VERR, "barfill test failed\n");
		return rv;
	}
	vprintf(VVERB, "barfill test passed\n");

	vprintf(VVERB, "test barflip\n");
	rv = test_barflip();
	if (rv != 0) {
		vprintf(VERR, "barflip test failed\n");
		return rv;
	}
	vprintf(VVERB, "barflip test passed\n");

	vprintf(VVERB, "test barnot\n");
	rv = test_barnot();
	if (rv != 0) {
		vprintf(VERR, "barnot test failed\n");
		return rv;
	}
	vprintf(VVERB, "barnot test passed\n");


	vprintf(VVERB, "test program complete.\n");
	return 0;
}

