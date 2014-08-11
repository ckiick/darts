

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
		assert(rv == 0);
		assert(barlen(&bar1) == 0);
	}
	/* now set some bits. Observe size */
	for (i = 0; i <= maxtb; i++) {
		rv = barset(&bar1, i);
		assert(rv == 0);
		assert(barlen(&bar1) == i+1);
		rv = barget(&bar1, i);
		assert(rv == 1);
		assert(barlen(&bar1) == i+1);
	}
	/* clear them. */
	for (i = 0; i <= maxtb; i++) {
		rv = barclr(&bar1, i);
		assert(rv == 1);
		assert(barlen(&bar1) >= i);
		rv = barget(&bar1, i);
		assert(rv == 0);
	}
	/* doit again backwards */
	for (i = maxtb; i >=0; i--) {
		rv = barset(&bar1, i);
		assert(rv == 0);
	}
	for (i = maxtb; i >=0; i--) {
		rv = barget(&bar1, i);
		assert(rv == 1);
	}
	for (i = maxtb; i >=0; i--) {
		rv = barclr(&bar1, i);
		assert(rv == 1);
	}
	for (i = maxtb; i >=0; i--) {
		rv = barget(&bar1, i);
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
			assert(rv == 0);
		}
	}
	rv = barget(&bar1, maxtb * 1497);
	assert(rv == 0);
	assert(barlen(&bar1) <= maxtb + 1);

	return 0;
}

/*
 * later on we'll add some verbiage and maybe debug stuff.
 */
int
main(int argc, char **argv)
{
	int rv;

	rv = verify();
	if (rv != 0) return rv;

/* we have a test function for each interface. */

	rv = test_baralloc_free();
	if (rv != 0) return rv;

	rv = test_barsize();
	if (rv != 0) return rv;

	/* we don't explicitly test barlen becuase it's used in all the
	 *other tests.
	 */

	/* we can create bars, see if we can use the actual bits. */
	rv = test_barget_set_clr();
	if (rv != 0) return rv;

	return 0;
}
