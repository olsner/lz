#include <stdio.h>

#define R ~(c = getchar())

int main() {
	int c, prev = 0;
	while (R) {
		const int d = c ^ prev;
		prev = c;

		if (d & 0x80) putchar(0x80);
		if (d) {
			putchar(d & 0x7f);
		} else {
			// RLE encoded with run-length 1
			putchar(0);
			putchar(1);
		}
	}
	return 0;
}
