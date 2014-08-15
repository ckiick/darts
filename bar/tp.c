
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

typedef unsigned long word_t;
typedef unsigned int uint_t;

#define	MWS	27
#define MAXUW	16
#define B2W(b,w)	(((b)+(w)-1)/(w))

#define SHUTUP	1

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

wclean(char MW[MAXUW][MWS], int nb, int ws)
{
	int i,j,k;
	int uw;

	uw = B2W(nb, ws);

	k = 0;
	for (i = 0; i < uw; i++) {
		for (j = ws-1; j >= 0 ; j--) {
			k++;
			if ((MW[i][j] == '0') && (k >nb)) {
				MW[i][j] = '_';
// printf("corrected %d %d\n", i, j);
			} else if ( (MW[i][j] == '_') && (k <= nb)) {
				MW[i][j] = '0';
// printf("corrected %d %d\n", i, j);
			}
		}
	}
}

void
wlsr2(char Win[MAXUW][MWS], char Wout[MAXUW][MWS], int dist, int nb, int ws)
{
	int i;
	uint_t dw = dist/ws;
	uint_t shift = dist % ws;
	uint_t mw;
	int uw = B2W(nb, ws);
	char wsr[27];
	char wsl[27];
	uint_t used;

	used = B2W(dist, ws);
	for (i = 0; i < uw; i++) {
		strncpy(Wout[i], "..........................", ws);
		Wout[i][ws] = '\0';
	}
	if (shift == 0) {
		wmove(Win, Wout, used, 0, uw-used);
		wset(Wout, uw - used, used, ws  );
		return;
	}

	/* right to left */
	for (i = 0; i < uw; i++) {
		mw = i + dw;
		if (mw >= uw) {
			strncpy(Wout[i], "00000000000000000000000000", ws);
			Wout[i][ws] = '\0';
		} else if (mw == uw-1) {
			sstr_r(Win[mw], Wout[i], shift, ws);
		} else {
			sstr_r(Win[mw], wsr, shift, ws);
			sstr_l(Win[mw+1], wsl, ws-shift, ws);
			orstr(wsl, wsr, Wout[i], ws);
		}
	}
}

void
wlsl2(char Win[MAXUW][MWS], char Wout[MAXUW][MWS], int dist, int nb, int ws)
{
	int i;
	uint_t dw = dist/ws;
	uint_t shift = dist % ws;
	int mw;		/* must be signed */
	int uw = B2W(nb, ws);
	char wsr[27];
	char wsl[27];
	uint_t used;

	used = B2W(dist, ws);
	for (i = 0; i < uw; i++) {
		strncpy(Wout[i], "..........................", ws);
		Wout[i][ws] = '\0';
	}
	if (shift == 0) {
		wmove(Win, Wout, 0, used, uw-used);
		wset(Wout, 0, used, ws);
	} else {

	/* left to right */
		for (i = uw-1; i >= 0; i--) {
			mw = i - dw;
			if (mw < 0) {
				strncpy(Wout[i], "00000000000000000000000000", ws);
				Wout[i][ws] = '\0';
			} else if (mw == 0) {
				sstr_l(Win[0], Wout[i], shift, ws);
			} else {
				sstr_r(Win[mw-1], wsr, ws-shift, ws);
				sstr_l(Win[mw], wsl, shift, ws);
				orstr(wsl, wsr, Wout[i], ws);
			}
		}
	}
	/* trim */
	if (nb%ws) {
		for (i = 0 ; i < ws - nb%ws; i++) {
			Wout[uw-1][i] = '_';
		}
	}
//Wout[uw-1][0] = '_';
}


void
wlsr(char Win[MAXUW][MWS], char Wout[MAXUW][MWS], int dist, int nb, int ws)
{
	int i, j;
	char wsl[27];
	char wsr[27];
	int uw = B2W(nb, ws);
	int used, shift;

	used = B2W(dist, ws);
	shift = dist % ws;

	/* detect burn thru */
	for (i = 0; i < uw; i++) {
		strncpy(Wout[i], "..........................", ws);
		Wout[i][ws] = '\0';
	}
#ifndef SHUTUP
printf("\twlsr: right %d shift of : ", dist);
pw(Win, uw);
printf("\n");
#endif
	if (shift == 0) {
		wmove(Win, Wout, used, 0, uw-used);
#ifndef SHUTUP
printf("\twlsr:wmoved %d from %d to %d: ", uw-used, used, 0);
pw(Win, uw);
printf(" to " );
pw(Wout, uw);
printf("\n");
#endif
	} else {
#ifndef SHUTUP
printf("\twlsr:composing from %d to %d\n", uw-used, 1);
#endif
		for (j = 0; j < uw-used; j++) {
			sstr_r(Win[j+used-1], wsr, shift, ws);
			sstr_l(Win[j+used], wsl, ws - shift, ws);
			orstr(wsl, wsr, Wout[j], ws);
		}
#ifndef SHUTUP
printf("%d = %d>>%d | %d<<%d : [%s]=[%s]|[%s]\n", j-1, j+used-2, shift, j+used-1, ws-shift, Wout[j-1], wsr, wsl);
#endif
	}
#ifndef SHUTUP
printf("\twlsr:wset %d at %d: ", used, uw-used);
pw(Wout, uw);
printf("\n");
#endif
	wset(Wout, uw - used, used, ws  );
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
/*
printf("\twlsl: left %d shift of : ", dist);
pw(Win, uw);
printf("\n");
*/
	if (shift == 0) {
		wmove(Win, Wout, 0, used, uw-used);
/*
printf("\twlsl:wmoved %d from %d to %d: ", uw-used, 0, used);
pw(Win, uw);
printf(" to " );
pw(Wout, uw);
printf("\n");
*/
	} else {
		for (j = uw; j > used; j--) {
			sstr_l(Win[j-used], wsl, shift, ws);
			sstr_r(Win[j-used-1], wsr, ws-shift, ws);
			orstr(wsl, wsr, Wout[j-1], ws);
/*
printf("%d = %d>>%d | %d<<%d : [%s]=[%s]|[%s]\n", j-1, j-used, ws-shift, j-used-1, shift, Wout[j-1], wsr, wsl);
*/
		}
	sstr_l(Win[j-used], Wout[j-1], shift, ws);
/*
printf("%d = %d<<%d: [%s]<>%d=[%s]\n", j-1, j-used, shift, Win[j-used], shift, Wout[j-1]);

printf("\twlsl:composed from %d to %d: ", uw, used);
pw(Wout, uw);
printf("\n");
*/
			used--;
	}
	wset(Wout, 0, used, ws  );
/*
printf("\twlsl:set %d at %d: ", used, 0);
pw(Wout, uw);
printf("\n");
*/
	/* trim. */
	sstr_l(Wout[uw-1], wsl, ws-nb%ws, ws);
	sstr_r(wsl, Wout[uw-1], ws-nb%ws, ws);
	for (j = 0; j < ws - nb%ws; j++) {
		if (Wout[uw-1][j] == '0') {
			Wout[uw-1][j] = '_';
		}
	}
/*
printf("\twlsl:trimmed %d of %d: ", uw-1, ws-nb%ws);
pw(Wout, uw);
printf("\n");
*/
}


int
doright(int nb, int ws)
{
	int bork = 0;
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

#ifndef SHUTUP
printf("RIGHT SHIFTS\n");
#endif
	for (i = 0; i < nb; i++) {
		sr[nb - i - 1]  = az[i];
	}
	sr[i] = '\0';
	s2W(sr, W, ws, nb);
#ifndef SHUTUP
	printf("WS=%d nb=%d uw=%d |%s|=", ws, nb, uw, sr);
	pw(W, uw);
	printf("\n");
	printf(" D Sh U1 U2 ");
	printf("uw-u  u-1 uw-U nb-d <%%ws ???? ");
	printf("str\twds\n");
#endif
#ifndef SHUTUP
	for (dist = 0; dist <= nb; dist++) {
		sstr_r(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		shift = dist % ws;
		used = B2W(dist, ws);
		used2 = dist / ws;

		val[0] = uw - used;
		val[1] = used -1;
		val[2] = uw - (used -1);
		val[3] = nb - dist;
		val[4] = val[3] % ws;
		

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

	printf("verifiying.... \n");
	for (dist = 0; dist <= nb; dist++) {
		sstr_r(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		wlsr(W, SW2, dist, nb, ws);
		wclean(SW2, nb, ws);

		for (i = 0; i < uw; i++) {
			if (strcmp(SW[i], SW2[i])) {
				printf("exception! at d=%d, w=%d ", dist, i);
				printf(" [%s] != [%s]\n", SW[i], SW2[i]);
			}
		}

		pw(W,uw);
		printf(">>%d=", dist);
		pw(SW2, uw);
		printf("\n");
	}

	printf("verifying Second method... \n");
#endif
	for (dist = 0; dist <= nb; dist++) {
		sstr_r(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		wlsr2(W, SW2, dist, nb, ws);
		wclean(SW2, nb, ws);

		for (i = 0; i < uw; i++) {
			if (strcmp(SW[i], SW2[i])) {
				printf("exception! at d=%d, w=%d ", dist, i);
				printf(" [%s] != [%s]\n", SW[i], SW2[i]);
				bork++;
			}
		}

#ifndef SHUTUP
		pw(W,uw);
		printf(">>%d=", dist);
		pw(SW2, uw);
		printf("\n");
#endif
	}
	return bork;
}


int
doleft(int nb, int ws)
{
	int bork = 0;
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

#ifndef SHUTUP
printf("\n\nLEFT SHIFTS\n");
#endif
	for (i = 0; i < nb; i++) {
		sr[nb - i - 1]  = az[i];
	}
	sr[i] = '\0';
	s2W(sr, W, ws, nb);
#ifndef SHUTUP
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

	printf("verifiying.... \n");
	for (dist = 0; dist <= nb; dist++) {
		sstr_r(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		wlsl(W, SW2, dist, nb, ws);
		wclean(SW2, nb, ws);

		for (i = 0; i < uw; i++) {
			if (strcmp(SW[i], SW2[i])) {
				printf("exception! at d=%d, w=%d ", dist, i);
				printf(" [%s] != [%s]\n", SW[i], SW2[i]);
			}
		}

		pw(W,uw);
		printf(">>%d=", dist);
		pw(SW2, uw);
		printf("\n");
	}
	printf("verifying Second method... \n");
#endif
	for (dist = 0; dist <= nb; dist++) {
		sstr_l(sr, ssr, dist, nb);
		s2W(ssr, SW, ws, nb);
		wlsl2(W, SW2, dist, nb, ws);
		wclean(SW2, nb, ws);

		for (i = 0; i < uw; i++) {
			if (strcmp(SW[i], SW2[i])) {
				printf("exception! at d=%d, w=%d ", dist, i);
				printf(" [%s] != [%s]\n", SW[i], SW2[i]);
				bork++;
			}
		}

#ifndef SHUTUP
		pw(W,uw);
		printf("<<%d=", dist);
		pw(SW2, uw);
		printf("\n");
#endif
	}
	return bork;
}

int
main(int argc, char **argv)
{
	uint_t ws, nb;
	int dr, dl;

	if (argc < 3) {
		printf("usage: %s ws nb\n", argv[0]);
		return 1;
	}

	ws = atoi(argv[1]);
	nb = atoi(argv[2]);

	dr = doright(nb, ws);

	dl = doleft(nb, ws);

	printf("%d %d %d\n", ws, nb, dr+dl);

	return dr+dl;
}
