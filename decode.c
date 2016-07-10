#include <ctype.h>
#include <stdio.h>

#define R ((c = getchar()) != EOF)
#define ST(l) l: RDO
#define RDO if (!R) goto fail; else
#define TO(s, cond) if (cond) goto s

#define LOG_DEBUG 0
#define debug(fmt, ...) do { if (LOG_DEBUG) fprintf(stderr, fmt, ## __VA_ARGS__); } while (0)

static int getint() {
	int c, res = 0;
	ST(more)
	{
		res += c;
		TO(more, c == 255);
	}
	return res;
fail:
	debug("getint got EOF\n");
	return -1;
}

#define LBMASK 65535
static unsigned lbpos = 0;
static char lb[LBMASK + 1];

// lbpos is the next character to write
// offset==0 means offset to the last character, i.e. lbpos-1
void out(int offset, int d) {
	const int lboff = (lbpos - 1 - offset) & LBMASK;
	const int c = d ^ lb[lboff];
	debug("%5d: %02x[%d] ^ %02x = %02x (%c)\n",
			lbpos,
			lb[lboff], lboff,
			d,
			c, isprint(c) ? c : '.');
	putchar(c);

	lb[lbpos] = c;
	lbpos = (lbpos + 1) & LBMASK;
}

int main() {
	int c;
	int offset = 0;
#define OUTX(c) out(offset, c)
	while (R) {
		if (c == 0xff) {
			offset = 127 + getint();
			debug("offset := %d\n", offset);
		} else if (c & 0x80) {
			int x = c & 0x7f;
			if (x) {
				offset = x;
				debug("offset := %d\n", offset);
			} else {
				RDO OUTX(c | 0x80);
			}
		} else if (c) {
			OUTX(c);
		} else {
			int count = getint();
			while (count--) OUTX(0);
		}
	}
	return 0;
fail:
	return 1;
}
