
/* implementation of bit array (bar).  Vastly simplified from previous. */

#include "bar.h"

typedef unsigned long word_t;
typedef struct {
	word_t	numbits;
	word_t	usedwords;
	word_t	capacity;
	word_t	*words;
} bar_t;

#define WORD_SIZE	(sizeof(word_t)*8)

#define	INIT_CAP	8	/* initial capacity. */
#define B2W(bits)	(((bits)+WORD_SIZE)/WORD_SIZE)
#define POW2(n)		(1UL << (WORD_SIZE - __builtin_clzl(n)))

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
	bar_t *ptr;

	if (bar->numbits == numbits) return;
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
				memset(&(bar->words[bar->numbits]), 0, (used - bar->usedwords)*sizeof(word_t));
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

uint_t barlen(bar_t *bar)
{
	return bar->numbits;
}

uint_t barcpy(bar_t *dest, bar_t *src)
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
	wndx = bar->numbits / WORD_SIZE;
	bit = bar->numbits % WORD_SIZE;
	if (( 1UL << bit) & bar->words[wndx]) {
		return 1;
	} else {
		return 0;
	}
}

int barassign(bar_t *bar, uint_t bit_index, uint val)
{
	uint_t wndx, bit, mask;
	if (bit_index >= bar->numbits) {
		if (barsize(bar, bit_index) == NULL) {
			return -1;
		}
	}
	wndx = bit_index / WORD_SIZE;
	bit = bit_index % WORD_SIZE;
	mask = 1UL << bit;
	bit = bar->words[wndx] & mask;
	if (val) {
		/* set bit to 1 */
		bar->words[wndx] |= mask;
	} else {
		bar->words[wndx] &= ~mask;
	}
	if (bit) {
		return 1;
	} else {
		return 0;
	}
}

#define barset(b, i)	barassign(b, i, 1);

#define barclr(b, i)	barassign(b, i, 0);

int barflip(bar_t *bar, uint bit_index);
{
	uint_t wndx, bit, mask;
	if (bit_index >= bar->numbits) {
		if (barsize(bar, bit_index) == NULL) {
			return -1;
		}
	}
	wndx = bit_index / WORD_SIZE;
	bit = bit_index % WORD_SIZE;
	mask = 1UL << bit;

	bar->words[wndx] ^= mask;
	bit = bar->words[wndx] & mask;

	if (bit) {
		return 1;
	} else {
		return 0;
	}
}

void barfill(bar_t *bar, uint_t val)
{
	memset(bar->words, val ? 0xFF : 0x00, bar->usedwords * sizeof(word_t));
}

#define barzero(b)	barfill(b, 0);

int barnot(bar_t *dest, bar_t *src)
{
	uint_t ndx, shift;

	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	for (ndx = 0; ndx < bar->usedwords; ndx++) {
		dest->words[ndx] = ~ src->words[ndx];
	}
	shift = numbits % WORD_SIZE;
	if (shift) {
		bar->words[bar->usedwords-1] <<= (WORD_SIZE - shift);
		bar->words[bar->usedwords-1] >>= (WORD_SIZE - shift);
	}
	return bar->numbits;
}

int barand(bar_t *dest, bar_t *src1, bar_t src2)
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
		dest->words[ndx] = src1->words[ndx] & src2->words[ndx];
	}
	return sz;
}

int baror(bar_t *dest, bar_t *src1, bar_t src2)
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

int barxor(bar_t *dest, bar_t *src1, bar_t src2)
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

int barlsr(bar_t *dest, bar_t *src, uint_t dist)
{
	uint_t used, ndx, shift;

	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	shift = dist % WORD_SIZE;
	used = dist / WORD_SIZE;
	if (shift == 0) {
		memmove(&(dest->words[0]), &(src->words[dist / WORD_SIZE]), used * sizeof(word_t));
		memset( &(dest->words[used]), 0, (dist/WORD_SIZE) * sizeof(word_t));
		return dest->numbits;
	}

	for (ndx = 0 ; ndx < dest->usedwords - used - 1; ndx++) {
		dest->words[ndx] = (src->words[ndx + used] >> shift) | (src->words[ndx+used+1] << (WORD_SIZE - shift));
	}
	dest->words[ndx] = src->words[ndx+used] >> shift;
	memset(&(dest->words[ndx+1]), 0, used * sizeof(word_t));
	return dest->numbits;
}

void barlsl(bar_t *dest, bar_t *src, uint_t dist)
{
	uint_t used, ndx, shift;

	if (src->numbits != dest->numbits) {
		if (barsize(dest, src->numbits) == NULL) {
			return -1;
		}
	}
	shift = dist % WORD_SIZE;
	used = dist / WORD_SIZE;
	if (shift == 0) {
		memmove(&(dest->words[used]), &(src->words[ndx]), (dest->usedwords - used) * sizeof(word_t));
		memset(&(dest->words[0]), 0, used * sizeof(word_t));
		return dest->numbits;
	}

	for (ndx = dest->usedwords-1; ndx > 0; ndx++) {
		dest->words[ndx] = (src->words[ndx] >> shift) | (src->words[ndx+used+1] << (WORD_SIZE - shift));
	}
	dest->words[ndx] = src->words[ndx+used] >> shift;
	memset(&(dest->words[ndx+1]), 0, used * sizeof(word_t));
	return dest->numbits;
}

void barlsle(bar_t *dest, bar_t *src, uint_t dist);
int barcmp(bar_t *bar1, bar_t *bar2);
uint_t barprint(bar_t *bar, char *str, int base);
uint_t barscan(bar_t *bar, char *str, int base);
int barwrite(bar_t *bar, int fd);
int barread(bar_t *bar, int fd);
unsigned long bar2long(bar_t *bar);
void long2bar(bar_t *bar, unsigned long word);
void barcvt(bar_t *dest, bar_t *src);
uint_t barpopc(bar_t *bar);
int barffs(bar_t *bar);
int barfns(bar_t *bar, uint_t bit_index);
int barffz(bar_t *bar);
int barfnz(bar_t *bar, uint_t bit_index);

