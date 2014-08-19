
/* implementation of bit array (bar).  Vastly simplified from previous. */

#include <stdlib.h>	/* realloc, malloc */
#include <string.h>	/* memset, memmove */
#include <unistd.h>	/* syscalls */

#include "bar.h"

#define WORD_SIZE	(sizeof(word_t)*8)
#define	INIT_CAP	8	/* initial capacity. */
#define	MAX_BITS	(64*1024*1024)
#define B2W(b)		(((b)+WORD_SIZE-1)/WORD_SIZE)
#define POW2(n)		(1UL << (WORD_SIZE - __builtin_clzl(n)))
#define MIN(a, b)  (((a) <= (b)) ? (a) : (b))
#define MAX(a, b)  (((a) >= (b)) ? (a) : (b)) 


/* one of the few places where we care about word size. 
 * because the bswap builtins are defined by number of bits, 
 * not the type. It would be nice if they were consistent.
 * So we make our own bswapl.
 */
#if defined(__SIZEOF_LONG__)
#if __SIZEOF_LONG__ == 4
#define BAR_WS	2
#define bswapl	__builtin_bswap32
#else	/* sizeof long==4 */
#define bswapl	__builtin_bswap64
#define BAR_WS	3
#endif	/* sizeof long==4 */
#else	/* no sizeof long. try something more common. */
#ifdef __LP64__
#define bswapl	__builtin_bswap64
#define BAR_WS	3
#else	/* ! __LP64__ */
#define bswapl	__builtin_bswap32
#define BAR_WS	2
#endif	/* __LP64__ */
#endif	/* sizeof long */

/* debug macro. optimizes out when DEBUG is not defined. */
#ifdef DEBUG
#define DBG(pargs) 		\
	if (printf("%s:",__func__) + printf pargs)
#else /* DEBUG */
#define DBG(f, pargs)		\
	if (0)
#endif /* non-DEBUG */

/* an empty bar_t. */
const bar_t nobar = {
	0, 0, 0, NULL
};

bar_t *baralloc(uint_t numbits)
{
	bar_t *b;

	b = malloc(sizeof(bar_t));
	if (b == NULL) {
		return NULL;
	}
	b->numbits = numbits;
	b->usedwords = B2W(numbits);
	b->capacity = POW2(b->usedwords + 1);
	b->capacity = MAX(b->capacity, INIT_CAP);
	b->words = malloc( b->capacity * sizeof(word_t));
	if (b->words == NULL) {
		free(b);
		return NULL;
	}
	memset(b->words, 0, b->capacity * sizeof(word_t));
	return b;
}

void barfree(bar_t *bar)
{
	if (bar != NULL) {
		if (bar->words != NULL) {
			free(bar->words);
		}
		free(bar);
	}
}

bar_t *barsize(bar_t *bar, uint_t numbits)
{
	uint_t used, cap, shift;
	word_t *ptr;

	if (bar->numbits == numbits) return bar;
	used = B2W(numbits);
	if (numbits > bar->numbits) {
		if (used > bar->usedwords) {
			if (used > bar->capacity) {
				cap = MAX(INIT_CAP, POW2(used +1));
				ptr = realloc(bar->words, cap * sizeof(word_t));
				if (ptr == NULL) {
					return NULL;
				}
				bar->words = ptr;
				memset(&(bar->words[bar->capacity]), 0, (cap - bar->capacity)*sizeof(word_t));
				bar->capacity = cap;
			}
			bar->usedwords = used;
		}
	} else {
		if (used < bar->usedwords) {
			memset(&(bar->words[used]), 0, (bar->usedwords - used)*sizeof(word_t));
			bar->usedwords = used;
		}
		shift = numbits % WORD_SIZE;
		if (shift) {
			bar->words[bar->usedwords-1] <<= (WORD_SIZE - shift);
			bar->words[bar->usedwords-1] >>= (WORD_SIZE - shift);
		}
	}
	bar->numbits = numbits;
	return bar;
}

void barnull(bar_t *bar)
{
	free(bar->words);
	*bar = nobar;
}

uint_t barlen(bar_t *bar)
{
	return bar->numbits;
}

int barcpy(bar_t *dest, bar_t *src)
{
	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	memmove(dest->words, src->words, src->usedwords * sizeof(word_t));
	dest->numbits = src->numbits;
	return dest->numbits;
}

bar_t *bardup(bar_t *bar)
{
	bar_t *newbar;

	newbar = baralloc(bar->numbits);
	if (newbar != NULL) {
		barcpy(newbar, bar);
	}
	return newbar;
}

uint_t barget(bar_t *bar, uint bit_index)
{
	uint_t wndx, bit;
	if (bit_index >= bar->numbits) {
		return 0;
	}
	wndx = bit_index / WORD_SIZE;
	bit = bit_index % WORD_SIZE;
	if (( 1UL << bit) & bar->words[wndx]) {
		return 1;
	} else {
		return 0;
	}
}

int barassign(bar_t *bar, uint_t bit_index, uint val)
{
	uint_t wndx, bit;
	word_t mask;
	if (bit_index >= bar->numbits) {
		if (barsize(bar, bit_index+1) == NULL) {
			return -1;
		}
	}
	wndx = bit_index / WORD_SIZE;
	bit = bit_index % WORD_SIZE;
	mask = 1UL << bit;
	if (bar->words[wndx] & mask) {
		bit = 1;
	} else {
		bit = 0;
	}
	if (val) {
		/* set bit to 1 */
		bar->words[wndx] |= mask;
	} else {
		bar->words[wndx] &= ~mask;
	}
	return bit;
}

int barflip(bar_t *bar, uint bit_index)
{
	uint_t wndx, bit;
	word_t mask;
	if (bit_index >= bar->numbits) {
		if (barsize(bar, bit_index+1) == NULL) {
			return -1;
		}
	}
	wndx = bit_index / WORD_SIZE;
	bit = bit_index % WORD_SIZE;
	mask = 1UL << bit;

	bar->words[wndx] ^= mask;
	if (bar->words[wndx] & mask) {
		return 1;
	} else {
		return 0;
	}
}

void barfill(bar_t *bar, uint_t val)
{
	uint_t shift;

	memset(bar->words, val ? 0xFF : 0x00, bar->usedwords * sizeof(word_t));
	shift = bar->numbits % WORD_SIZE;
	if (shift) {
		bar->words[bar->usedwords-1] <<= (WORD_SIZE - shift);
		bar->words[bar->usedwords-1] >>= (WORD_SIZE - shift);
	}
}

int barnot(bar_t *dest, bar_t *src)
{
	uint_t ndx, shift;

	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	for (ndx = 0; ndx < dest->usedwords; ndx++) {
		dest->words[ndx] = ~ src->words[ndx];
	}
	shift = dest->numbits % WORD_SIZE;
	if (shift) {
		dest->words[dest->usedwords-1] <<= (WORD_SIZE - shift);
		dest->words[dest->usedwords-1] >>= (WORD_SIZE - shift);
	}
	return dest->numbits;
}

int barand(bar_t *dest, bar_t *src1, bar_t *src2)
{
	uint_t ndx, sz, minw;

	sz = MAX(src1->numbits, src2->numbits);
	if (sz >= dest->numbits) {
		if (barsize(dest, sz) == NULL) {
			return -1;
		}
	}
	minw = MIN( src1->usedwords, src2->usedwords);
	for (ndx = 0; ndx < minw; ndx++) {
		dest->words[ndx] = src1->words[ndx] & src2->words[ndx];
	}
	return sz;
}

int baror(bar_t *dest, bar_t *src1, bar_t *src2)
{
	uint_t ndx, sz, minw;
	bar_t *bigger;

	sz = MAX(src1->numbits, src2->numbits);
	if (sz >= dest->numbits) {
		if (barsize(dest, sz) == NULL) {
			return -1;
		}
	}
	minw = MIN( src1->usedwords, src2->usedwords);
	for (ndx = 0; ndx < minw; ndx++) {
		dest->words[ndx] = src1->words[ndx] | src2->words[ndx];
	}
	if (src1->usedwords == src2->usedwords) {
		return sz;
	}
	if (src1->usedwords > src2->usedwords) {
		bigger = src1;
	} else {
		bigger = src1;
	}
	memmove(&(dest->words[minw]), &(bigger->words[minw]), (bigger->usedwords - minw) *sizeof(word_t));

	return dest->numbits;
}

int barxor(bar_t *dest, bar_t *src1, bar_t *src2)
{
	uint_t ndx, sz, minw;
	bar_t *bigger;

	sz = MAX(src1->numbits, src2->numbits);
	if (sz >= dest->numbits) {
		if (barsize(dest, sz) == NULL) {
			return -1;
		}
	}
	minw = MIN(src1->usedwords, src2->usedwords);
	for (ndx = 0; ndx < minw; ndx++) {
		dest->words[ndx] = src1->words[ndx] ^ src2->words[ndx];
	}
	if (src1->usedwords == src2->usedwords) {
		return sz;
	}
	if (src1->usedwords > src2->usedwords) {
		bigger = src1;
	} else {
		bigger = src1;
	}
	memmove(&(dest->words[minw]), &(bigger->words[minw]), (bigger->usedwords - minw) *sizeof(word_t));

	return dest->numbits;
}

int barlsr(bar_t *dest, bar_t *src, uint_t dist)
{
	uint_t used, ndx, shift;
	uint_t offset;
	int mark;		/* must be signed. */

	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	if (src->numbits == 0) {
		return 0;
	}
	if (dist >= dest->numbits) {
		barzero(dest);
		return dest->numbits;
	}
	shift = dist % WORD_SIZE;
	used = B2W(dist);
	offset = dist / WORD_SIZE;

	if (shift == 0) {
		memmove(&(dest->words[0]), &(src->words[used]), (dest->usedwords - used) * sizeof(word_t));
		memset(&(dest->words[dest->usedwords-used]), 0, used * sizeof(word_t));
		return dest->numbits;
	}
	for (ndx = 0 ; ndx < dest->usedwords; ndx++) {
			mark = ndx + offset;
			if (mark >= dest->usedwords) {
				dest->words[ndx] = 0UL;
			} else if (mark == dest->usedwords - 1) {
				dest->words[ndx] = src->words[mark] >> shift;
			} else {
				dest->words[ndx] = src->words[mark] >> shift |
				    src->words[mark+1] << (WORD_SIZE - shift);
			}
	}
	return dest->numbits;
}

int barlsl(bar_t *dest, bar_t *src, uint_t dist)
{
	uint_t used, shift;
	uint_t offset, rem;
	int ndx, mark;		/* must be signed. */

	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	if (src->numbits == 0) {
		return 0;
	}
	if (dist >= dest->numbits) {
		barzero(dest);
		return dest->numbits;
	}
	shift = dist % WORD_SIZE;
	used = B2W(dist);
	offset = dist / WORD_SIZE;

	if (shift == 0) {
		memmove(&(dest->words[used]), &(src->words[0]), (dest->usedwords - used) * sizeof(word_t));
		memset(&(dest->words[0]), 0, used * sizeof(word_t));
	} else {
		for (ndx = dest->usedwords - 1; ndx >=0; ndx--) {
			mark = ndx - offset;
			if (mark < 0) {
				dest->words[ndx] = 0UL;
			} else if (mark == 0) {
				dest->words[ndx] = src->words[mark] << shift;
			} else {
				dest->words[ndx] = src->words[mark] << shift |
				    src->words[mark-1] >> (WORD_SIZE - shift);
			}
		}
	}
	/* trim */
	rem = dest->numbits % WORD_SIZE;
	if (rem) {
		dest->words[dest->usedwords-1] <<= (WORD_SIZE - rem);
		dest->words[dest->usedwords-1] >>= (WORD_SIZE - rem);
	}
	return dest->numbits;
}

int barlsle(bar_t *dest, bar_t *src, uint_t dist)
{
	uint_t used, shift;
	uint_t offset, rem;
	int ndx, mark;		/* must be signed. */
	uint_t srcuw;		/* src==dest can be confusing */

	srcuw = src->usedwords;
	if (src->numbits + dist != dest->numbits) {
		if (barsize(dest, src->numbits+dist) == NULL) {
			return -1;
		}
	}
	if (src->numbits == 0) {
		return 0;
	}
	shift = dist % WORD_SIZE;
	used = B2W(dist);
	offset = dist / WORD_SIZE;

	if (shift == 0) {
		memmove(&(dest->words[used]), &(src->words[0]), srcuw * sizeof(word_t));
	} else {
		for (ndx = dest->usedwords - 1; ndx >=0; ndx--) {
			mark = ndx - offset;
			if (mark < 0) {
				dest->words[ndx] = 0UL;
			} else if (mark == 0) {
				dest->words[ndx] = src->words[mark] << shift;
			} else {
				dest->words[ndx] = src->words[mark] << shift |
				    src->words[mark-1] >> (WORD_SIZE - shift);
			}
		}
	}
	/* trim */
	rem = dest->numbits % WORD_SIZE;
	if (rem) {
		dest->words[dest->usedwords-1] <<= (WORD_SIZE - rem);
		dest->words[dest->usedwords-1] >>= (WORD_SIZE - rem);
	}
	return dest->numbits;
}

// bar1 and bar2 do not have to be the same size.
int barcmp(bar_t *bar1, bar_t *bar2)
{
	uint_t ndx = bar1->usedwords;

	if ((bar1->numbits == 0) && (bar2->numbits == 0)) {
		return 0;
	}

	if (bar1->usedwords > bar2->usedwords) {
		for (ndx = bar1->usedwords; ndx > bar2->usedwords; ndx--) {
			if (bar1->words[ndx-1]) {
				return 1;
			}
		}
	} else if (bar1->usedwords < bar2->usedwords) {
		for (ndx = bar2->usedwords; ndx > bar1->usedwords; ndx--) {
			if (bar2->words[ndx-1]) {
				return -1;
			}
		}
	}
//	ndx = MIN(bar1->usedwords, bar2->usedwords);
	/* should be even now.*/
	for (; ndx > 0; ndx--) {
		if (bar1->words[ndx-1] > bar2->words[ndx-1]) {
			return 1;
		} else if (bar1->words[ndx-1] < bar2->words[ndx-1]) {
			return -1;
		}
	}
	return 0;
}

static const char _digits[17] = "0123456789ABCDEF";
static int _w2str(char *str, word_t w, uint_t bits, uint_t logbase)
{
	word_t mask = 0xfUL >> (4 - logbase);
	int i = 0, j;
	int digs;

	if (bits == 0) bits = WORD_SIZE;
	digs = (bits+logbase-1) / logbase;
	for (j = digs; j > 0; j--) {
		str[i++] = _digits[(w >> ((j-1)*logbase)) & mask];
	}
	str[i] = '\0';
	return i;
}

uint_t barprint(bar_t *bar, char *str, int base)
{
	uint_t i = 0, ndx, end, logbase;

	if (str == NULL) return 0;
	if (base == 2) {
		logbase = 1;
//	} else if (base == 8) {
//		logbase = 3;
	} else if (base == 16) {
		logbase = 4;
	} else {
		return 0;
	}

	if (bar->usedwords == 0) {
		str[0]='0'; str[1] = '\0';
		return 1;
	}
	end = bar->numbits % WORD_SIZE;
	i += _w2str(&(str[i]), bar->words[bar->usedwords-1], end, logbase);
	for (ndx =  bar->usedwords-1; ndx > 0; ndx--) {
		i += _w2str( &(str[i]), bar->words[ndx-1], WORD_SIZE, logbase);
	}
	return i;
}

#define C2V(c)  ((c <= '9') ? (c - '0') : ((c&0xDF) - 'A' + 10))
static int _str2w(char *str, word_t *w, uint_t bits, uint_t logbase)
{
	int i = 0, j = 0;

	if (bits == 0) bits = WORD_SIZE;
	*w = 0;
	while (j < bits) {
		*w <<= logbase;
		*w |= C2V(str[i]);
		j += logbase;
		i++;
	}
	return i;
}

uint_t barscan(bar_t *bar, char *str, int base)
{
	uint_t i = 0, len, ndx, end, logbase;
	char digits[17] = "0123456789ABCDEF";

	if (str == NULL) return 0;
	if (base == 2) {
		logbase = 1;
//	} else if (base == 8) {
//		logbase = 3;
	} else if (base == 16) {
		logbase = 4;
	} else {
		return 0;
	}
	digits[base] = '\0';
	len = strspn(str, digits);
	if (len * logbase != bar->numbits) {
		if (barsize(bar, len * logbase) == NULL) {
			return 0;
		}
	}
	if (len == 0) return 0;

	ndx = bar->usedwords;
	end = bar->numbits % WORD_SIZE;
	i += _str2w(&(str[i]), &(bar->words[ndx-1]), end, logbase);
	for (--ndx; ndx > 0; ndx--) {
		len -= WORD_SIZE / logbase;
		i+= _str2w(&(str[i]), &(bar->words[ndx-1]), WORD_SIZE, logbase);
	}
	str[i] = '\0';
	return bar->numbits;
}

unsigned long bar2ul(bar_t *bar)
{
	if (bar == NULL) return 0;
	if (bar->usedwords == 0) return 0;
	return (bar->words[0]);
}

int ul2bar(bar_t *bar, unsigned long word)
{
	if (bar->numbits != WORD_SIZE) {
		if (barsize(bar, WORD_SIZE) == NULL) {
			return -1;
		}
	}
	bar->words[0] = word;
	return WORD_SIZE;
}

int barfns(bar_t *bar, uint_t bit_index)
{
	uint_t ndx;
	int fb;
	int shift;

	if (bar->numbits == 0) {
		return -1;
	}
	if (bit_index >= bar->numbits) {
		return -1;
	}
	/* three sections, any of which may be missing.
	 * partial word at beginning, full words in middle, partial
	 * word at end.
	 */
	ndx = bit_index / WORD_SIZE;
	shift = (bit_index % WORD_SIZE);
	if (shift) {
		fb = __builtin_ffsl(bar->words[ndx] >> shift);
		if (fb != 0) {
			return bit_index + (fb - 1);
		}
		ndx++;
	}
	if (ndx >= bar->usedwords) return -1;
	/* full words */
	for ( ; ndx < bar->usedwords - 1; ndx++) {
		if (bar->words[ndx] != 0UL) {
			fb = __builtin_ffsl(bar->words[ndx]);
			return (fb - 1) + (ndx * WORD_SIZE);
		}
	}
	/* last word. may or may not be partial */
	shift = (bar->numbits % WORD_SIZE);
	if (shift) shift = WORD_SIZE - shift;
	fb = __builtin_ffsl(bar->words[ndx] << shift);
	if (fb != 0) {
		return (fb - shift) -1 + ndx * WORD_SIZE;
	}
	return -1;
}

int barfnz(bar_t *bar, uint_t bit_index)
{
	uint_t ndx;
	int fb;
	int shift;

	if (bar->numbits == 0) {
		return -1;
	}
	if (bit_index >= bar->numbits) {
		return -1;
	}
	/* three sections, any of which may be missing.
	 * partial word at beginning, full words in middle, partial
	 * word at end.  And the special case where beginning IS the end.
	 */
	ndx = bit_index / WORD_SIZE;
	shift = (bit_index % WORD_SIZE);
	if (shift) {
		word_t w = ~(bar->words[ndx]) >> shift;
		fb = __builtin_ffsl(w);
		if (fb != 0) {
			if ((ndx == (bar->usedwords - 1)) &&
		 	   (fb +shift > bar->numbits % WORD_SIZE)) {
				return -1;
			} else {
				return (fb + shift - 1) + ndx*WORD_SIZE;
			}
		}
		ndx++;
	}
	if (ndx >= bar->usedwords) return -1;
	/* full words */
	for ( ; ndx < bar->usedwords - 1; ndx ++) {
		if (bar->words[ndx] != ~0UL) {
			fb = __builtin_ffsl(~(bar->words[ndx]));
			return (fb - 1) + (ndx * WORD_SIZE);
		}
	}
	/* last word. may or may not be partial */
	shift = (bar->numbits % WORD_SIZE);
	if (shift) shift = WORD_SIZE - shift;
	fb = __builtin_ffsl( (~bar->words[ndx]) << shift);
	if (fb != 0) {
		return (fb - shift) -1 + ndx * WORD_SIZE;
	}
	return -1;
}

uint_t barpopc(bar_t *bar)
{
	uint_t ndx;
	int count = 0;
	int shift;

	if (bar->numbits == 0) {
		return 0;
	}

	for (ndx = 0; ndx < bar->usedwords - 1; ndx ++) {
		count += __builtin_popcountl(bar->words[ndx]);
	}
	/* last word. may or may not be partial */
	shift = (bar->numbits % WORD_SIZE);
	if (shift) shift = WORD_SIZE - shift;
	count += __builtin_popcountl(bar->words[ndx] << shift);

	return count;
}

int barswap(bar_t *dest, bar_t *src)
{
	uint_t ndx;
	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	for (ndx = 0; ndx < dest->usedwords; ndx++) {
		dest->words[ndx] = bswapl(src->words[ndx]);
	}
	return dest->numbits;
}

#ifdef DEBUG
/* returns non-zero if there is something wrong with the bar. */
int barcheck(bar_t *bar)
{
	int i, shift;
	if (bar == NULL) return -1;
	if (bar->words == NULL) {
		if (bar->numbits || bar->usedwords || bar->capacity) {
			return -2;
		}
	}
	if (bar->usedwords > bar->capacity) return -3;
	if (B2W(bar->numbits) != bar->usedwords) return -4;

	for (i = bar->usedwords; i < bar->capacity; i++) {
		if (bar->words[i] != 0) {
			return -5;
		}
	}
	shift = bar->numbits % WORD_SIZE;
	if (shift) {
		if ((bar->words[bar->usedwords-1] >> shift) != 0) {
			return -6;
		}
	}
	if (POW2(bar->capacity != bar->capacity)) {
		return -7;
	}

	return 0;
}
#define DCHK(bar)	assert(barchk(&bar) == 0)
#else 	/* DEBUG */
#define	DCHK(bar)
#endif /* DEBUG */
/* save the rest for later. we've made enough bugs for now. */

#define	BAR_BE	0x0
#define BAR_LE	0x1
#define BAR_LSW0	0x00
#define BAR_MWS0	0x10
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BAR_BO	BAR_LE
#else
#define	BAR_BO	BAR_BE
#endif
#define	BAR_WO	BAR_LSW0

#define BA_HDR_SZ	16

#define BH_BO_BIT	0x01
#define BH_WO_BIT	0x02

#define BH_ORDER	8
#define BH_WSIZE	9
#define BH_BARSIZE	10

#define	MAJOR_VER	1
#define MINOR_VER	2

#define	MAJORVER0	('0'+(MAJOR_VER/10))
#define	MAJORVER1	('0'+(MAJOR_VER%10))
#define	MINORVER0	('0'+(MINOR_VER/10))
#define	MINORVER1	('0'+(MINOR_VER%10))

#define BAMAGIC0	0xBA
#define BAMAGIC1	0xF0
#define BAMAGIC2	0xF0
#define BAMAGIC3	0xBA

static const char bar_hdr[BA_HDR_SZ] = {
	BAMAGIC0, BAMAGIC1, BAMAGIC2, BAMAGIC3,
	MAJORVER0, MAJORVER1, MINORVER0, MINORVER1,
	BAR_BO|BAR_WO, BAR_WS, sizeof(bar_t), 0,
	0, 0, 0, 0 };

int barwrite(bar_t *bar, int fd)
{
	size_t rv;
	int byteswritten = 0;
	char hdr[BA_HDR_SZ];

	if (bar == NULL) return 0;
	(void)memcpy(hdr, bar_hdr, BA_HDR_SZ);

	hdr[12] = (uint8_t)(bar->usedwords >> 24)&0xFF;
	hdr[13] = (uint8_t)(bar->usedwords >> 16)&0xFF;
	hdr[14] = (uint8_t)(bar->usedwords >> 8)&0xFF;
	hdr[15] = (uint8_t)(bar->usedwords)&0xFF;

	rv = write(fd, hdr, BA_HDR_SZ);
	if (rv != BA_HDR_SZ) {
		return -2;
	}
	byteswritten += rv;
	rv = write(fd, bar, sizeof(bar_t));
	if (rv != sizeof(bar_t)) {
		return -3;
	}
	byteswritten += rv;
	rv = write(fd, bar->words, bar->usedwords * sizeof(word_t));
	if (rv !=  bar->usedwords * sizeof(word_t)) {
		return -4;
	}
	byteswritten += rv;

	return byteswritten;
}

int barread(bar_t *bar, int fd)
{
	size_t rrv;
	int rv;
	char whdr[16];
	word_t *barbuf = NULL;
	bar_t *barrv;
	word_t filewords;
	word_t filebits;
	word_t tmpwords;
	int byteorder, wordorder, wordsize;
	int majorver, minorver;

	if (bar == NULL) return -1;
	rrv = read(fd, whdr, BA_HDR_SZ);
	if (rrv != BA_HDR_SZ) {
		return -1;
	}

	/* before we do anything else, validate the header */
	rv = memcmp(bar_hdr, whdr, 4);
	if (rv != 0) {
		return -2;
	}
	majorver = ((whdr[4] - '0')*10) + (whdr[5]-'0');
	minorver = ((whdr[6] - '0')*10) + (whdr[7]-'0');

	if (majorver > MAJOR_VER) {
		return -3;
	}

	if ((whdr[BH_WSIZE] < 2) || (whdr[BH_WSIZE] > 3)) {
		return -4;
	}

	/* that's enough to let us at least read the file. */
	barbuf = malloc( (size_t)whdr[BH_BARSIZE]);
	if (barbuf == NULL) {
		return -5;
	}
	rrv = read(fd, barbuf, (size_t)whdr[BH_BARSIZE]);
	if (rrv != whdr[BH_BARSIZE]) {
		free(barbuf);
		return -6;
	}
	byteorder = whdr[BH_ORDER] & BH_BO_BIT;
	wordorder = whdr[BH_ORDER] & BH_WO_BIT;
	switch (whdr[BH_WSIZE]) {
	case 2:	wordsize = 32;
		break;
	case 3: wordsize = 64;
		break;
	default:
		free(barbuf);
		return -7;
		break;
	}
	filewords = (whdr[12]<<24) | (whdr[12]<<16) | (whdr[12]<<8) | whdr[15];
	/* decode at least the first word. */
	filebits =  barbuf[0];
	if (wordsize != WORD_SIZE) {
		if (wordsize == 64) {
			uint64_t *w64 = (uint64_t *)barbuf;
			filebits = (word_t)w64[0];
		} else {
			uint64_t *w32 = (uint32_t *)barbuf;
			filebits = (word_t)w32[0];
		}
	}

	/* we can let the bar_t go now */
	free(barbuf);

	/* just so we don't allocate the universe... */
	if (filebits > MAX_BITS) {
		return -8;
	}

	/* more error checking */
	if ((filewords * wordsize) < filebits) {
		return -9;
	}

	/* to have enough room to byteswap, over-allocate a little.*/
	tmpwords = ((filebits + 63)/64);
	if (WORD_SIZE == 32) {
		tmpwords *= 2;
	}
	barrv = barsize(bar, tmpwords);
	if (barrv  == NULL) {
		return -10;
	}

	/* finally time to read the data. */
	rv = read(fd,  bar->words, filewords * (wordsize/8));
	if (rv != filewords * (wordsize/8)) {
		barnull(bar);
		return -11;
	}

	/* detect the common case. */
	if ((wordorder == BAR_WO) && (byteorder == BAR_BO) &&
	    (wordsize == WORD_SIZE)) {
		barsize(bar, filebits);
		return filebits;
	}

	/* now do any necessary conversions. */
	/* on BE systems, changing word size requires 32-bit word swapping */
	if ((wordsize != WORD_SIZE) && (byteorder == BAR_BE)) {
		uint32_t *w;
		int i, n;
		uint32_t swapw;
		w = (uint32_t *)(bar->words);
		n = ((filebits + 63)/64) * 2;
		for (i = 0; i < n; i+=2) {
			swapw = w[i];
			w[i] = w[i+1];
			w[i+1] = swapw;
		}
	}

	if (wordorder != BAR_WO) {
		/* reverse it. remember strtac? */
		int i; word_t swapw;

		for (i = 0; i < bar->usedwords/2; i++) {
			swapw = bar->words[i];
			bar->words[i] = bar->words[(bar->usedwords-1)-i];
			bar->words[(bar->usedwords-1)-i] = swapw;
		}
	}

	if (byteorder != BAR_BO) {
		int i;
		for (i = 0; i < filewords; i++) {
			bar->words[i] = bswapl(bar->words[i]);
		}
	}
	barsize(bar, filebits); /* trim. */
	return filebits;
}


#ifdef NOTDEF
almost done.
int barwrite(bar_t *bar, int fd);
int barread(bar_t *bar, int fd);
#endif
