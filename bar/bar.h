
#ifndef __BAR_H
#define __BAR_H

#include <inttypes.h>
#include <sys/types.h>

typedef unsigned long word_t;

typedef unsigned int uint_t;

typedef struct {
	word_t	numbits;
	word_t	usedwords;
	word_t	capacity;
	word_t	*words;
} bar_t;

/* to help with initialization. */
extern const bar_t nobar;

/* function templates */
bar_t *baralloc(uint_t numbits);
void barfree(bar_t *bar);
bar_t *barsize(bar_t *bar, uint_t numbits);
void barnull(bar_t *bar);
uint_t barlen(bar_t *bar);
bar_t *bardup(bar_t *bar);
uint_t barcpy(bar_t *dest, bar_t *src);
uint_t barget(bar_t *bar, uint bit_index);
int barassign(bar_t *bar, uint_t bit_index, uint val);
#define barset(b, i)	barassign(b, i, 1);
#define barclr(b, i)	barassign(b, i, 0);
int barflip(bar_t *bar, uint bit_index);
void barfill(bar_t *bar, uint_t val);
#define barzero(b)	barfill(b, 0);
int barnot(bar_t *dest, bar_t *src);
int barand(bar_t *dest, bar_t *src1, bar_t *src2);
int baror(bar_t *dest, bar_t *src1, bar_t *src2);
int barxor(bar_t *dest, bar_t *src1, bar_t *src2);
int barlsr(bar_t *dest, bar_t *src, uint_t dist);
int barlsl(bar_t *dest, bar_t *src, uint_t dist);
int barlsle(bar_t *dest, bar_t *src, uint_t dist);
int barcmp(bar_t *bar1, bar_t *bar2);
uint_t barprint(bar_t *bar, char *str, int base);
uint_t barscan(bar_t *bar, char *str, int base);

/* missing features? */
/*
	DEBUG barcheck() - validation check on bar.
*/

#ifdef NOTYET
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
#endif
#endif /* __BAR_H */
