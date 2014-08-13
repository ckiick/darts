
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

typedef unsigned long word_t;
typedef unsigned int uint_t;

#define	MWS	27
#define MAXUW	16
#define B2W(b,w)	(((b)+(w)-1)/(w))

/* a run is defined by the word size and num bits */

void
s2W(char *s, char W[MAXUW][MWS], int ws, int nb)
{
	int i, j, k;
	int uw;

	uw = B2W(nb,ws);
	for (i = 0; i < uw; i++) {
		strncpy(W[i], "__________________________", ws);
		W[i][ws] = '\0';
	}
	for (i = 0; i < nb; i++) {
		j = i / ws;
		k = ws - (i % ws) - 1;
		W[j][k] = s[nb - i - 1];
	}
}

void
sstr(char *src, char *dest, int shift, int nb)
{
	strncpy(dest, "00000000000000000000000000", shift);
	dest[shift] = '\0';
	strncpy(&(dest[shift]), src, nb-shift);
	dest[nb] = '\0';
}

void pw(char W[MAXUW][MWS], int uw)
{
	int i;
	for (i = 0; i < uw; i++) {
		printf("[%s]",W[i]);
	}
}

int
main(int argc, char **argv)
{
	char W[MAXUW][MWS];
	char SW[MAXUW][MWS];
	char az[] = "abcdefghijklmnopqrstuvwzyz";
	char sr[27];
	char ssr[27];
	uint_t ws, nb, uw;
	uint_t i, j, k;
	uint_t dist, used, used2, shift;
	int val[10] = { 0 };

	ws = atoi(argv[1]);
	nb = atoi(argv[2]);
	uw = B2W(nb, ws);

	for (i = 0; i < nb; i++) {
		sr[nb - i - 1]  = az[i];
	}
	sr[i] = '\0';
	s2W(sr, W, ws, nb);
	printf("WS=%d nb=%d uw=%d |%s|=", ws, nb, uw, sr);
	pw(W, uw);
	printf("\n");

	printf(" D Sh U1 U2 ");

	printf("uw-u  u-1 uw-U ???? ???? ???? ");

	printf("str\twds\n");

	for (dist = 0; dist <= nb; dist++) {
		sstr(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		shift = dist % ws;
		used = B2W(dist, ws);
		used2 = dist / ws;

		val[0] = uw - used;
		val[1] = used -1;
		val[2] = uw - (used -1);


		printf("%2d %2d %2d %2d ",dist, shift, used, used2);
		for(j = 0; j < 6; j++) {
			printf("%4d ", val[j]);
		}
		printf("%s ", ssr);
		pw(SW, uw);
		printf("\n");
	}
	for (dist = 0; dist <= nb; dist++) {
		printf("D=%2d ", dist);
		printf("%s ", sr);
		pw(W, uw);
		printf("\n");

		sstr(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		printf("     ");
		printf("%s ", ssr);
		pw(SW, uw);
		printf("\n");

		shift = dist % ws;
		used = B2W(dist, ws);
		used2 = dist / ws;

		if (shift == 0) {
			printf("memmove %d words from src[%d] to dest[%d]\n", uw - used, used, 0);
			printf("zero %d at dest[%d]\n", used, uw - used);
		} else {
			printf("loop %d times\n", uw-used);
			for (j = uw-used; j > 0; j--) {
				printf("\tD[%d] = s[%d]|s[%d]\n", j-1, j, j+1);
			}

			printf("zero %d at dest[%d]\n", used, uw - used);
		}
	}
	return 0;
}

