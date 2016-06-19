#include <stdio.h>

#define R ((c = getchar()) != EOF)
#define ST(l) l: RDO
#define RDO if (!R) goto fail; else
#define TO(s, cond) if (cond) goto s

#define debug(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

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
static unsigned lbpos;
static char lb[LBMASK + 1];

void out(int c) {
	lb[lbpos] = c;
	lbpos = (lbpos + 1) & LBMASK;
	putchar(c);
}

int main() {
	int c;
	for (;;) {
		int offset = getint();
		if (offset < 0) return 0;
		offset++;
		debug("offset is %d\n", offset);
		int len = getint();
		if (len < 0) goto fail;
		debug("length of block is %d\n", len);
		// one non-xored byte
		RDO out(c);
		// and len xored bytes
		while (len--) {
			RDO {
				int back = lb[(lbpos - offset) & LBMASK];
				debug("In %02x ^ %02x = %02x [%d - %d]\n", c, back, c ^ back, lbpos, offset);
				out(c ^ back);
			}
		}
	}
fail:
	return 1;
}
