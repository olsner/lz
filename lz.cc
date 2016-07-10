#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
using namespace std;

#include "sprintfxx.h"

namespace {

typedef unsigned Cost;
// x is the cost value for one whole byte
#define BYTE_COST(x) ((x) * 8)
#define BIT_COST(x) (x)
#define VAL_COST(x) ilog2(x)

uint8_t lit_conv_table[256];

void init_lit_table(uint8_t *dest)
{
#if 0
	uint8_t rev[256];
	for (size_t n = 256; n--;) {
		rev[n] = n;
	}
	sort(rev, rev + 256,
			[](uint8_t l, uint8_t r) -> bool {
				int pl = __builtin_popcount(l), pr = __builtin_popcount(r);
				return pl < pr || (pl == pr && l < r);
				// return (pl << 8) | l < (pr << 8) | r;
			});
	for (size_t n = 256; n--;) {
		dest[rev[n]] = n;
	}
#else
	for (size_t n = 256; n--;) {
		dest[n] = n;
	}
#endif

#if 0
	for (int v = 0; v < 256; v++) {
		printf("%d: %d\n", v, dest[v]);
	}
#endif
}

// 1 + ilog2(v), ilog2(0) = 0
int ilog2(int v) {
	return (v > 128) + (v > 64) + (v > 32) + (v > 16) + (v > 8) + (v > 4) + (v > 2) + (v > 1) + (v > 0);
}

Cost lit_cost(uint8_t lit) {
	return BYTE_COST(1); // VAL_COST(lit_conv_table[lit]);
}
Cost length_cost(int len) {
	return BYTE_COST((len + 254) / 255);
}
Cost new_offset_cost(int len) {
	return BYTE_COST(1) + (len > 127 ? length_cost(len-128) : 0);
}
// Amount to add to cost when increasing 'len' by 1
Cost incr_length_cost(int len) {
	return length_cost(len + 1) - length_cost(len);
}

string read_file(FILE *fp, bool close_after = false) {
	string res;
	char tmp[256];
	ssize_t n;
	while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
		res.append(tmp, n);
	}
	if (close_after) fclose(fp);
	return res;
}
string read_stdin() {
	return read_file(stdin);
}
string read_file(const char *path) {
	return read_file(fopen(path, "rb"), true);
}

void write_file(FILE *fp, const string& data) {
	fwrite(data.data(), data.size(), 1, fp);
}
void write_file(const char *fname, const string& data) {
	FILE *fp = fopen(fname, "wb");
	write_file(fp, data);
	fclose(fp);
}

struct lze
{
	size_t offset : 16; // current offset
	size_t zeroes : 16; // number of consecutive zero deltas
	unsigned cost : 24; // cost in bytes to get this far
	unsigned valid : 1; // 1 if valid, 0 if not filled in
	unsigned prev : 4; // index of previous entry
	unsigned next : 3; // index of next entry, filled in by trace

	bool operator==(const lze& other) const {
		assert(valid && other.valid);
		return offset == other.offset && zeroes == other.zeroes;
	}
	string dump(const lze* prev = nullptr) const {
		return sprintf++("%3d|%4d|%d", offset, zeroes, cost);
		// TODO Figure out which prev was the base for this (or actually save it)
	}

};

const unsigned LBMASK = 65535;
// Number of entries to try in "dynamic programming" solver
const size_t N = 8;
const size_t MIN_MATCH = 4;

void init(lze* begin, lze* end) {
	begin[0].valid = true;
}

int insert_lze(lze *dest, size_t n, lze result) {
	for (size_t i = 0; i < n; i++) {
		if (result.cost < dest[i].cost) {
			swap(dest[i], result);
		}
		if (result == dest[i]) {
			// Exact result already present
//			printf++("Result %s already at %d\n", result.dump(), i);
			return n;
		}
	}
	if (n < N)
		dest[n++] = result;
	return n;
}

lze new_offset(const lze *p, uint8_t i, size_t offset, size_t match_len) {
	const lze& prev = p[i];
	return lze {
		offset,
		1,
		prev.cost + new_offset_cost(offset) + 1 + incr_length_cost(0),
		true, i, 0,
	};
}

size_t strpfxlen(const char* a, const char* b) {
	size_t n = 0;
	while (*a == *b) a++, b++, n++;
	return n;
}

void add(const lze *prevs, const char *begin, const char *pos, const char *end, lze *dest) {
	int added = 0;
	// Length-extend each match by one (delta) literal
	for (uint8_t i = N; i--;) {
		const lze& prev = prevs[i];
		if (!prev.valid) continue;

		size_t offset = prev.offset;
		uint8_t lastc = offset < size_t(pos - begin) ? pos[-1-offset] : 0;
		uint8_t delta = pos[0] ^ lastc;
		lze e = lze {
			offset,
			delta ? 0u : prev.zeroes + 1,
			prev.cost +
				(delta ? lit_cost(delta) : incr_length_cost(prev.zeroes)),
			true, i, 0,
		};
//		printf++("extend offset %d to %d with delta %02x: %s\n", offset, pos - begin, delta, e.dump(prevs));
		added = insert_lze(dest, added, e);
	}
	if (size_t(end - pos) < MIN_MATCH) {
		return;
	}
	// Look for new offsets to encode
	for (const char *b = pos - 1; b >= begin; b--) {
		if (!memcmp(b, pos, MIN_MATCH)) {
			int offset = pos - b - 1;
			size_t match_len = strpfxlen(b, pos);
			printf++("%zu byte match at %d (from %d): offset=%d\n", match_len, b - begin, pos - begin, offset);
			for (uint8_t i = N; i--;) {
				const lze& prev = prevs[i];
				if (!prev.valid) continue;
				if (offset == prev.offset) continue;

				lze e = new_offset(prevs, i, offset, match_len);
				added = insert_lze(dest, added, e);
			}
		}
	}
}

void dump_entries(const lze *begin, const lze *end) {
	unsigned n = 0;
	const lze *prev = nullptr;
	for (; begin < end; begin += N)
	{
		printf("%4u:", n++);
		for (size_t i = 0; i < N; i++) {
			printf++("\t%s", begin[i].dump(prev));
		}
		printf("\n");
		prev = begin;
	}
}

int trace(lze* begin, lze* end) {
	int p = 0, n = 0;
	while (end > begin) {
		end -= N;
		end[p].next = n;
		n = p;
		p = end[p].prev;
	}
	return n;
}

void genint(string& res, int i) {
	while (i >= 255) {
		res.push_back(255);
		i -= 255;
	}
	res.push_back(i);
}

void gen_new_offset(string& res, int offset) {
	if (offset < 128)
		res.push_back(0x80 | offset);
	else
	{
		res.push_back(0xff);
		offset -= 128;
		genint(res, offset);
	}
}

void gen_zeroes(string& res, int n) {
	res.push_back(0);
	genint(res, n);
}

}

string compress(const string& input) {
	assert(input.size() <= LBMASK);
	lze* table = (lze *)calloc(sizeof(lze), (LBMASK + 1) * N);
	init(table, table + N);
	for (size_t i = 0; i < input.size(); i++) {
		lze* prev = table + (i & LBMASK) * N;
		lze* cur = prev + N;
		add(prev, &input[0], &input[i], &input[input.size()], cur);
		// Decision about committing to a parse at cur[0] (the best so far),
		// flushing back to the last flush.
	}
	lze* end = table + std::min(input.size(), size_t(LBMASK) + 1) * N;
	lze* last = end - N;
	dump_entries(table, end);
	int n = trace(table, end);

	string output;
	size_t offset = 0;
	int zeroes = 0;
	size_t i = 0;
	auto emit_zeroes = [&]() {
		if (zeroes) {
			printf("%5u: %d zeroes, from %d\n", i - 1, zeroes, i - zeroes);
			gen_zeroes(output, zeroes);
			zeroes = 0;
		}
	};
	for (; i < input.size(); i++) {
		const lze& e = table[N * i + n];
		printf++("%5u: %s\n", i, e.dump());
		if (offset != e.offset) {
			emit_zeroes();
			printf++("%5u: new offset %d (output@%zx)\n", i, e.offset, output.size());
			gen_new_offset(output, e.offset);
			offset = e.offset;
		}
		const uint8_t c = input[i];
		const uint8_t lastc = i > offset ? input[i-1-offset] : 0;
		const uint8_t delta = c ^ lastc;
		if (delta) {
			emit_zeroes();
			if (delta & 0x80) {
				output.push_back(0x80);
			}
			output.push_back(lit_conv_table[delta]);
		} else {
			zeroes++;
		}
		printf++("%5u: %02x[%d] ^ %02x = %02x\n", i, lastc, i - 1 - offset, c, delta);
		n = e.next;
	}
	emit_zeroes();
	printf++("Best coding: %s\n", last->dump(last - N));
	return output;
}

int main(int argc, const char *argv[]) {
	init_lit_table(lit_conv_table);

	const string input = argc > 1 ? read_file(argv[1]) : read_stdin();
	fprintf(stderr, "%zu bytes of input\n", input.size());
	const string output = compress(input);
	fprintf(stderr, "%zu bytes of output\n", output.size());
	if (argc > 2)
		write_file(argv[2], output);
	else
		write_file("lz.out", output);
}
