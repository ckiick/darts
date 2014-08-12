

/* test file for bit array code (bar) */

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <inttypes.h>

/*
	TODO:
test_bardup
test_barcpy
test_barcmp
test_barprint
test_barscan 
CLI options processing
change to test harness design
more specific test cases (overlap, d=s, word and capacity boundaries.
*/

#include "bar.h"
/* include it twice to ensure no double defs. */
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

int verbose = VDEBUG;

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
test_barsize_null()
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

	barnull(&bar1);
	assert(barlen(&bar1) == 0);
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

	barnull(&bar1);
	assert(barlen(&bar1) == 0);

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

	barnull(&bar1);
	assert(barlen(&bar1) == 0);

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

	barnull(&bar1);
	assert(barlen(&bar1) == 0);

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

	barnull(&bar1);
	assert(barlen(&bar1) == 0);
	barnull(&bar2);
	assert(barlen(&bar2) == 0);
	barnull(&bar3);
	assert(barlen(&bar3) == 0);

	return 0;
}

int
test_barand()
{
	bar_t *barp1;
	bar_t bar1, bar2, bar3;
	int i;
	uint_t b;
	int tlen = 128;
	int rv;

	bzero(&bar1, sizeof(bar_t));
	bzero(&bar2, sizeof(bar_t));
	bzero(&bar3, sizeof(bar_t));

	/* handle empty case */
	rv = barand(&bar3, &bar2, &bar1);
	assert(rv == 0);
	assert(barlen(&bar3) == 0);

	barp1 = barsize(&bar1, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar1) == tlen);
	barfill(&bar1, 1);

	/* second argument is empty. */
	rv = barand(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == 0);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	/* swap args */
	rv = barand(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == 0);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* second arg non-empty, same size */
	barp1 = barsize(&bar2, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen);

	rv = barand(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	/* swap */
	rv = barand(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* make 2nd arg larger */
	barp1 = barsize(&bar2, tlen*2);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*2);

	rv = barand(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar1) == tlen);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	/* swap */
	rv = barand(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar1) == tlen);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}


	/* great. now do it all over again, with second arg all 1's. */

	barfill(&bar2, 1);
	/* second arg non-empty, same size */
	barp1 = barsize(&bar1, tlen*2);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*2);
	barfill(&bar1, 1);

	rv = barand(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	/* swap */
	rv = barand(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}

	/* make 2nd arg larger */
	barp1 = barsize(&bar2, tlen*3);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*3);

	rv = barand(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*3);
	assert(barlen(&bar1) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen*2; i < tlen*3; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* swap */
	rv = barand(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*3);
	assert(barlen(&bar1) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen*2; i < tlen*3; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	barnull(&bar1);
	assert(barlen(&bar1) == 0);
	barnull(&bar2);
	assert(barlen(&bar2) == 0);
	barnull(&bar3);
	assert(barlen(&bar3) == 0);

	return 0;
}

int
test_baror()
{
	bar_t *barp1;
	bar_t bar1, bar2, bar3;
	int i;
	uint_t b;
	int tlen = 254;
	int rv;

	bzero(&bar1, sizeof(bar_t));
	bzero(&bar2, sizeof(bar_t));
	bzero(&bar3, sizeof(bar_t));

	/* handle empty case */
	rv = baror(&bar3, &bar2, &bar1);
	assert(rv == 0);
	assert(barlen(&bar3) == 0);

	barp1 = barsize(&bar1, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar1) == tlen);
	barfill(&bar1, 1);

	/* second argument is empty. */
	rv = baror(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == 0);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	/* swap args */
	rv = baror(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == 0);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}

	/* second arg non-empty, same size */
	barp1 = barsize(&bar2, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen);

	rv = baror(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	/* swap */
	rv = baror(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}

	/* make 2nd arg larger */
	barp1 = barsize(&bar2, tlen*2);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*2);

	rv = baror(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar1) == tlen);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	/* swap */
	rv = baror(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar1) == tlen);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* great. now do it all over again, with second arg all 1's. */

	barfill(&bar2, 1);
	/* second arg non-empty, same size */
	barp1 = barsize(&bar1, tlen*2);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*2);
	barfill(&bar1, 1);

	rv = baror(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	/* swap */
	rv = baror(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}

	/* make 2nd arg larger */
	barp1 = barsize(&bar2, tlen*3);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*3);

	rv = baror(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*3);
	assert(barlen(&bar1) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen*2; i < tlen*3; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* swap */
	rv = baror(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*3);
	assert(barlen(&bar1) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen*2; i < tlen*3; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	barnull(&bar1);
	assert(barlen(&bar1) == 0);
	barnull(&bar2);
	assert(barlen(&bar2) == 0);
	barnull(&bar3);
	assert(barlen(&bar3) == 0);

	return 0;
}

int
test_barxor()
{
	bar_t *barp1;
	bar_t bar1, bar2, bar3;
	int i;
	uint_t b;
	int tlen = 513;
	int rv;

	bzero(&bar1, sizeof(bar_t));
	bzero(&bar2, sizeof(bar_t));
	bzero(&bar3, sizeof(bar_t));

	/* handle empty case */
	rv = baror(&bar3, &bar2, &bar1);
	assert(rv == 0);
	assert(barlen(&bar3) == 0);

	barp1 = barsize(&bar1, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar1) == tlen);
	barfill(&bar1, 1);

	/* second argument is empty. */
	rv = barxor(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == 0);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	/* swap args */
	rv = barxor(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == 0);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}

	/* second arg non-empty, same size */
	barp1 = barsize(&bar2, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen);

	rv = baror(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	/* swap */
	rv = barxor(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}

	/* make 2nd arg larger */
	barp1 = barsize(&bar2, tlen*2);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*2);

	rv = barxor(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar1) == tlen);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	/* swap */
	rv = barxor(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar1) == tlen);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 1);
	}
	for (i = tlen; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* great. now do it all over again, with second arg all 1's. */

	barfill(&bar2, 1);

	/* second arg non-empty, same size */
	barp1 = barsize(&bar1, tlen*2);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*2);
	barfill(&bar1, 1);

	rv = barxor(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	/* swap */
	rv = barxor(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar1));
	assert(barlen(&bar2) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar1));

	for (i = 0; i < tlen*2; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* make 2nd arg larger */
	barp1 = barsize(&bar2, tlen*3);
	assert(barp1 != NULL);
	assert(barlen(&bar2) == tlen*3);

	rv = barxor(&bar3, &bar1, &bar2);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*3);
	assert(barlen(&bar1) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*3; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	/* swap */
	rv = barxor(&bar3, &bar2, &bar1);
	assert(rv == barlen(&bar2));
	assert(barlen(&bar2) == tlen*3);
	assert(barlen(&bar1) == tlen*2);
	assert(barlen(&bar3) == barlen(&bar2));

	for (i = 0; i < tlen*3; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}

	barnull(&bar1);
	assert(barlen(&bar1) == 0);
	barnull(&bar2);
	assert(barlen(&bar2) == 0);
	barnull(&bar3);
	assert(barlen(&bar3) == 0);

	return 0;
}

int
test_barshift()
{
	bar_t *barp1;
	bar_t bar1, bar2, bar3;
	int i, j, k;
	uint_t b;
	int tlen = 39;
	int rv;

	bzero(&bar1, sizeof(bar_t));
	bzero(&bar2, sizeof(bar_t));
	bzero(&bar3, sizeof(bar_t));

	/* handle empty case */
	rv = barlsr(&bar3, &bar1, tlen);
	assert(rv == 0);
	assert(barlen(&bar3) == 0);
	rv = barlsl(&bar3, &bar1, tlen);
	assert(rv == 0);
	assert(barlen(&bar3) == 0);
	rv = barlsle(&bar3, &bar1, tlen);
	assert(rv == 0);
	assert(barlen(&bar3) == 0);

	barp1 = barsize(&bar1, tlen);
	assert(barp1 != NULL);
	assert(barlen(&bar1) == tlen);

	rv = barset(&bar1, 0);
	assert(rv == 0);
	assert(barget(&bar1, 0) == 1);

	/* first shift 1 around. */
	for (i = 0; i < tlen-2; i++) {
		rv = barlsl(&bar3, &bar1, 1);
		assert(rv == barlen(&bar1));
		assert(barlen(&bar1) == barlen(&bar3));
		assert(barget(&bar3, i) == 0);
		assert(barget(&bar3, i+1) == 1);
		assert(barget(&bar3, i+2) == 0);
		rv = barlsr(&bar2, &bar3, i);
		assert(rv == barlen(&bar2));
		assert(barlen(&bar2) == barlen(&bar3));
		assert(barlen(&bar2) == barlen(&bar1));
		assert(barget(&bar3, 0) == 1);
	}
	/* test shifting all the way. */
	barzero(&bar3);
	assert(barlen(&bar3) == tlen);
	rv = barset(&bar1, 0);
	assert(rv == 0);
	assert(barget(&bar1, 0) == 1);

	rv = barlsr(&bar3, &bar1, tlen+1);
	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	rv = barset(&bar1, tlen-1 );
	assert(rv == 0);
	assert(barget(&bar1, tlen-1) == 1);
	barzero(&bar3);
	rv = barlsl(&bar3, &bar1, tlen);
	for (i = 0; i < tlen; i++) {
		b = barget(&bar3, i);
		assert(b == 0);
	}
	/* test different shift sizes */
	for (j = 0; j < tlen; j++) {
		barfill(&bar1, 1);
		rv = barlsl(&bar2, &bar1, j);
		assert(rv == barlen(&bar1));
		assert(barlen(&bar2) == barlen(&bar1));
		for (i = 0; i < tlen - j; i++) {
			b = barget(&bar2, i);
			assert(b == 1);
		}
		for (i = tlen - j; i < tlen; i++) {
			b = barget(&bar2, i);
			assert(b == 0);
		}
		rv = barlsr(&bar3, &bar2, j);
		assert(rv == barlen(&bar2));
		assert(barlen(&bar2) == barlen(&bar3));
		for (i = 0; i < j; i++) {
			b = barget(&bar2, i);
			assert(b == 0);
		}
		for (i = j; i < tlen; i++) {
			b = barget(&bar2, i);
			assert(b == 1);
		}
	}
	/* test lse, and double use */
	j = tlen;
	barfill(&bar1, 1);
	for (i = 0; i < tlen; i++) {
		rv = barlsle(&bar1, &bar1, i);
		j += i;
		assert(rv == j);
		assert(barlen(&bar1) == j);
		rv = barget(&bar1, 0);
		for (k = 0; k < tlen; k++) {
			rv = barget(&bar1, j - k);
			assert(rv == 1);
		}
		for (k = 0; k < j - tlen; k++) {
			rv = barget(&bar1, j - k);
			assert(rv == 0);
		}
	}

	barnull(&bar1);
	assert(barlen(&bar1) == 0);
	barnull(&bar2);
	assert(barlen(&bar2) == 0);
	barnull(&bar3);
	assert(barlen(&bar3) == 0);

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

	vprintf(VVERB, "test barsize_null\n");
	rv = test_barsize_null();
	if (rv != 0) {
		vprintf(VERR, "barsize_null test failed\n");
		return rv;
	}
	vprintf(VVERB, "barsize_null test passed\n");

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

	vprintf(VVERB, "test barand\n");
	rv = test_barand();
	if (rv != 0) {
		vprintf(VERR, "barand test failed\n");
		return rv;
	}
	vprintf(VVERB, "barand test passed\n");

	vprintf(VVERB, "test baror\n");
	rv = test_baror();
	if (rv != 0) {
		vprintf(VERR, "baror test failed\n");
		return rv;
	}
	vprintf(VVERB, "baror test passed\n");

	vprintf(VVERB, "test barxor\n");
	rv = test_barxor();
	if (rv != 0) {
		vprintf(VERR, "barxor test failed\n");
		return rv;
	}
	vprintf(VVERB, "barxor test passed\n");

	vprintf(VVERB, "test barshift\n");
	rv = test_barxor();
	if (rv != 0) {
		vprintf(VERR, "barshift test failed\n");
		return rv;
	}
	vprintf(VVERB, "barshift test passed\n");

	vprintf(VVERB, "test program complete.\n");
	return 0;
}

