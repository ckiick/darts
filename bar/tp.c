
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
sstr_r(char *src, char *dest, int shift, int nb)
{
	strncpy(dest, "00000000000000000000000000", shift);
	dest[shift] = '\0';
	strncpy(&(dest[shift]), src, nb-shift);
	dest[nb] = '\0';
}


void
sstr_l(char *src, char *dest, int shift, int nb)
{
	strncpy(dest, "00000000000000000000000000", nb);
	dest[nb]='\0';
	strncpy(&(dest[0]), &(src[shift]), nb-shift);
}

void
orstr(char *src1, char *src2, char *dest, int ws)
{
	int i;
	char a, b, r;

	for (i = 0; i < ws; i++) {
		a = src1[i];
		b = src2[i];
		if ((a == '0') && (b == '0')) {
			dest[i] = '\0';
		}
		if ((a != '0') && (b != '0')) {
			dest[i] = 'X';
		}
		if ((a == '0') && (b != '0')) {
			dest[i] = b;
		}
		if ((a != '0') && (b == '0')) {
			dest[i] = a;
		}
	}
	dest[i] = '\0';
}

void pw(char W[MAXUW][MWS], int uw)
{
	int i;
	for (i = 0; i < uw; i++) {
		printf("[%s]",W[i]);
	}
}

void
wmove(char SRCW[MAXUW][MWS], char DESTW[MAXUW][MWS], int from, int to, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		strcpy(DESTW[to+i], SRCW[from+i]);
	}
}

void
wset(char MW[MAXUW][MWS], int start, int len, int ws)
{
	int i;

	for (i = 0; i < len; i++) {
		strncpy(MW[start+i], "00000000000000000000000000", ws);
		MW[start+i][ws] = '\0';
	}
}

void
wlsr(char Win[MAXUW][MWS], char Wout[MAXUW][MWS], int dist, int nb, int ws)
{
	int i, j;
	char wsl[27];
	char wsr[27];
	int used, shift;
	int uw = B2W(nb, ws);

	shift = dist % ws;
	used = B2W(dist, ws);

	/* detect burn thru */
	for (i = 0; i < uw; i++) {
		strncpy(Wout[i], "..........................", ws);
		Wout[i][ws] = '\0';
	}
/*
printf("\twlsr: right %d shift of : ", dist);
pw(Win, uw);
printf("\n");
*/
	if (shift == 0) {
		wmove(Win, Wout, used, 0, uw-used);
/*
printf("\twlsr:wmoved %d from %d to %d: ", uw-used, used, 0);
pw(Win, uw);
printf(" to " );
pw(Wout, uw);
printf("\n");
*/
	} else {
/*
printf("\twlsr:composing from %d to %d\n", uw-used, 1);
*/
		for (j = uw-used; j > 0; j--) {
			sstr_l(Win[j+used-1], wsl, ws - shift, ws);
			sstr_r(Win[j+used-2], wsr, shift, ws);
			orstr(wsl, wsr, Wout[j-1], ws);
		}
	}
/*
printf("\twlsr:wset %d at %d: ", used, uw-used);
pw(Wout, uw);
printf("\n");
*/
	wset(Wout, uw-used, used, ws  );
}

void
wlsl(char Win[MAXUW][MWS], char Wout[MAXUW][MWS], int dist, int nb, int ws)
{
	int i, j;
	char wsl[27];
	char wsr[27];
	int used, shift;
	int uw = B2W(nb, ws);

	shift = dist % ws;
	used = B2W(dist, ws);

	/* detect burn thru */
	for (i = 0; i < uw; i++) {
		strncpy(Wout[i], "..........................", ws);
		Wout[i][ws] = '\0';
	}
printf("\twlsl: left %d shift of : ", dist);
pw(Win, uw);
printf("\n");
	if (shift == 0) {
		wmove(Win, Wout, used, 0, uw-used);
printf("\twlsl:wmoved %d from %d to %d: ", uw-used, used, 0);
pw(Win, uw);
printf(" to " );
pw(Wout, uw);
printf("\n");

	} else {
		for (j = uw; j > used; j--) {
			sstr_l(Win[j-used], wsl, shift, ws);
			sstr_r(Win[j-used-1], wsr, ws-shift, ws);
			orstr(wsl, wsr, Wout[j-1], ws);
printf("%d = %d>>%d | %d<<%d : [%s]=[%s]|[%s]\n", j-1, j-used, ws-shift, j-used-1, shift, Wout[j-1], wsr, wsl);
		}
	sstr_l(Win[j-used], Wout[j-1], shift, ws);
printf("%d = %d<<%d: [%s]<>%d=[%s]\n", j-1, j-used, shift, Win[j-used], shift, Wout[j-1]);

printf("\twlsl:composed from %d to %d: ", uw, used);
pw(Wout, uw);
printf("\n");
			used--;
	}
	wset(Wout, 0, used, ws  );
printf("\twlsl:set %d at %d: ", used, 0);
pw(Wout, uw);
printf("\n");
}


void
doright(int nb, int ws)
{
	char W[MAXUW][MWS];
	char SW[MAXUW][MWS];
	char SW2[MAXUW][MWS];
	char az[] = "abcdefghijklmnopqrstuvwzyz";
	char sr[27];
	char ssr[27];
	uint_t uw;
	uint_t i, j, k;
	uint_t dist, used, used2, shift;
	int val[10] = { 0 };
	char wsr[27];
	char wsl[27];
	char wres[27];

	uw = B2W(nb, ws);

printf("RIGHT SHIFTS\n");
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
		sstr_r(sr, ssr, dist, nb);
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

		sstr_r(sr, ssr, dist, nb);
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

			for (j = 0; j > uw-used; j++) {
//			for (j = uw-used; j > 0; j--)
				printf("\tD[%d] = s[%d]<<%d|s[%d]>>%d  ", j, j+used,ws-shift, j+used-2, shift);
				printf("[%s]=[%s]|[%s]\n", SW[j], W[j+used], W[j+used-1]);
				sstr_l(W[j+used],wsl, ws-shift, ws);
				sstr_r(W[j+used-1],wsr, shift, ws);
				orstr(wsl, wsr, wres, ws);
				printf("[%s]=[%s]|[%s]=[%s]\n", SW[j], wsl, wsr, wres);
			}

			printf("zero %d at dest[%d]\n", used, uw - used);
		}
	}

	for (dist = 0; dist <= nb; dist++) {
		wlsr(W, SW2, dist, nb, ws);
		pw(W,uw);
		printf(">>%d=", dist);
		pw(SW2, uw);
		printf("\n");
	}
}


void
doleft(int nb, int ws)
{
	char W[MAXUW][MWS];
	char SW[MAXUW][MWS];
	char SW2[MAXUW][MWS];
	char az[] = "abcdefghijklmnopqrstuvwzyz";
	char sr[27];
	char ssr[27];
	uint_t uw;
	uint_t i, j, k;
	uint_t dist, used, used2, shift;
	int val[10] = { 0 };
	char wsr[27];
	char wsl[27];
	char wres[27];

	uw = B2W(nb, ws);

printf("\n\nLEFT SHIFTS\n");
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
		sstr_l(sr, ssr, dist, nb);
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

		sstr_l(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		printf("     ");
		printf("%s ", ssr);
		pw(SW, uw);
		printf("\n");

		shift = dist % ws;
		used = B2W(dist, ws);
		used2 = dist / ws;

		if (shift == 0) {
			printf("memmove %d words from src[%d] to dest[%d]\n", uw - used, 0, used);
			printf("zero %d at dest[%d]\n", used, 0);
		} else {
			printf("loop %d times\n", uw-used);
			for (j = uw; j > used; j--) {
/* here */

				printf("\tD[%d]=s[%d]>>%d|s[%d]<<%d  ", j-1, j-used-1,ws-shift, j-used, shift);
				printf("[%s]=[%s]|[%s]\n", SW[j-1], W[j-used-1], W[j-used]);
				sstr_r(W[j-used-1],wsl, ws-shift, ws);
				sstr_l(W[j-used],wsr, shift, ws);
				orstr(wsl, wsr, wres, ws);
				printf("\t[%s]=[%s]|[%s]=[%s]\n", SW[j-1], wsl, wsr, wres);

			}

			printf("zero %d at dest[%d]\n", used, 0);
		}
	}

	for (dist = 0; dist <= nb; dist++) {
		wlsl(W, SW2, dist, nb, ws);
		pw(W,uw);
		printf("<<%d=", dist);
		pw(SW2, uw);
		printf("\n");
	}

}

int
main(int argc, char **argv)
{
	uint_t ws, nb;

	if (argc < 3) {
		printf("usage: %s ws nb\n", argv[0]);
		return 1;
	}

	ws = atoi(argv[1]);
	nb = atoi(argv[2]);

	doright(nb, ws);

	doleft(nb, ws);

}
