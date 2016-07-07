#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

void init_lit_table(uint8_t *dest, uint8_t *rev)
{
	for (size_t n = 256; n--;) {
		rev[n] = n;
	}
	std::sort(rev, rev + 256,
			[](uint8_t l, uint8_t r) -> bool {
				int pl = __builtin_popcount(l), pr = __builtin_popcount(r);
				return pl < pr || (pl == pr && l < r);
				// return (pl << 8) | l < (pr << 8) | r;
			});
	for (size_t n = 256; n--;) {
		dest[rev[n]] = n;
	}
}

int main(int argc, const char *argv[]) {
	uint8_t fwd[256];
	uint8_t rev[256];
	uint8_t *table = fwd;

	init_lit_table(fwd, rev);

	if (argc == 2 && !strcmp(argv[1], "-r"))
		table = rev;

	int c;
	while (~(c = getchar())) {
		putchar(table[c]);
	}
	return 0;
}
