#include <stdio.h>

#define R ~(c = getchar())

static void putint(int i) {
	while (i > 255) {
		putchar(255); i -= 255;
	}
	putchar(i);
}

#define LBMASK 65535
static unsigned lbpos;
static char lb[LBMASK + 1];

void putbuf() {
	// offset = 0
	putint(0);
	putint(lbpos - 1);
	int lastc;
	if (lbpos > 0) {
		putchar(lastc = lb[0]);
		int i = 1;
		while (--lbpos) {
			int c = lb[i++];
			putchar(lastc ^ c);
			lastc = c;
		}
	}
	lbpos = 0;
}

int main() {
	int c;
	while (R) {
		lb[lbpos++] = c;
		if (lbpos == LBMASK + 1) putbuf();
	}
	if (lbpos) putbuf();
}
