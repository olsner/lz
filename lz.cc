#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
using namespace std;

#include "sprintfxx.h"

namespace {

#define LOG_DEBUG 0
#define debug(fmt, ...) do { if (LOG_DEBUG) fprintf++(stderr, fmt, ## __VA_ARGS__); } while (0)

typedef unsigned Cost;
// x is the cost value for one whole byte
#define BYTE_COST(x) ((x) * 8)
#define BIT_COST(x) (x)
#define VAL_COST(x) ilog2(x)

// 1 + ilog2(v), ilog2(0) = 0
int ilog2(int v) {
	return (v > 128) + (v > 64) + (v > 32) + (v > 16) + (v > 8) + (v > 4) + (v > 2) + (v > 1) + (v > 0);
}

Cost lit_cost(uint8_t lit) {
	// VAL_COST(lit);
	return lit < 0x80 ? BYTE_COST(1) : BYTE_COST(2);
}
Cost length_cost(int len) {
	return BYTE_COST((len + 254) / 255);
}
Cost new_offset_cost(int len) {
	return BYTE_COST(1) + (len >= 127 ? length_cost(len-127) : 0);
}
// Amount to add to cost when increasing 'len' by 1
Cost incr_length_cost(int len) {
	return length_cost(len + 1) - length_cost(len);
}
Cost zeroes_cost(size_t zeroes) {
	return 1 + length_cost(zeroes);
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
	unsigned prev : 8; // index of previous entry
	unsigned next : 8; // index of next entry, filled in by trace

	bool operator==(const lze& other) const {
		assert(valid && other.valid);
		return offset == other.offset && zeroes == other.zeroes;
	}
	bool operator<(const lze& other) const {
		return cost < other.cost /*||
			(cost == other.cost && zeroes > other.zeroes)*/;
	}
	string dump() const {
		return sprintf++("%3d|%2d|%d|%d", offset, zeroes, cost, prev);
	}

};

const unsigned LBMASK = 65535;
// N seems to help quite a lot, that's not very good for performance.
const size_t N = 32;
// Cost of a match is:
// 1 byte offset header (at least)
// 1 byte 0 + at least 1 byte run length
// (= 3 or more bytes)
// compared to 1-2 bytes per delta, it's possible to make up for it
// with 2 bytes (deltas that both require 2 bytes), but it's unlikely.
// (Might be more likely on binary files though.)
const size_t MIN_MATCH = 4;

void init(lze* begin, lze* end) {
	begin[0].valid = true;
}

int insert_lze(lze *dest, size_t n, lze result) {
	for (size_t i = 0; i < n; i++) {
		if (result < dest[i]) {
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

size_t strpfxlen(const char* a, const char* b) {
	size_t n = 0;
	while (*a == *b) a++, b++, n++;
	return n;
}

lze extend_zero(const lze& prev, unsigned i) {
	return lze {
		prev.offset,
		prev.zeroes + 1u,
		prev.cost
			+ BYTE_COST(prev.zeroes ? 0 : 1)
			+ incr_length_cost(prev.zeroes),
		true, i, 0,
	};
}

lze extend_delta(const lze& prev, unsigned i, uint8_t delta) {
	return lze { prev.offset, 0, prev.cost + lit_cost(delta), true, i, 0 };
}

lze new_offset(const lze& prev, uint8_t i, size_t offset, size_t match_len) {
	return lze {
		offset,
		1,
		prev.cost + new_offset_cost(offset) + zeroes_cost(1),
		true, i, 0,
	};
}

template <typename T>
void foreach_match(const char* const begin, const char* const end, size_t n, T&& cb) {
	const char *p = begin;
	while ((p = (const char *)memmem(p, end - p, end, n))) {
		cb(p++);
	}
}

void add(const lze *prevs, const char *begin, const char *pos, const char *end, lze *dest) {
	int added = 0;
	// Length-extend each match by one (delta) literal
	for (uint8_t i = 0; i < N; i++) {
		const lze& prev = prevs[i];
		if (!prev.valid) break;

		size_t offset = prev.offset;
		uint8_t lastc = offset < size_t(pos - begin) ? pos[-1-offset] : 0;
		uint8_t delta = pos[0] ^ lastc;
		lze e = delta ? extend_delta(prev, i, delta) : extend_zero(prev, i);
		debug("extend offset %d to %d with delta %02x: %s\n", offset, pos - begin, delta, e.dump());
		added = insert_lze(dest, added, e);
	}
	if (size_t(end - pos) < MIN_MATCH) {
		return;
	}
	// Look for new offsets to encode
	foreach_match(begin, pos, MIN_MATCH, [&](const char *b) {
		int offset = pos - b - 1;
		size_t match_len = strpfxlen(b, pos);
		debug("%zu byte match at %d (from %d): offset=%d\n", match_len, b - begin, pos - begin, offset);
		for (uint8_t i = 0; i < N; i++) {
			const lze& prev = prevs[i];
			if (!prev.valid) break;
			if (offset == prev.offset) continue;

			lze e = new_offset(prev, i, offset, match_len);
			added = insert_lze(dest, added, e);
		}
	});
}

void dump_entries(const lze *begin, const lze *end) {
	int n = 0;
	while (begin < end)
	{
		begin += N;
		printf("%4d:", n++);
		for (size_t i = 0; i < N; i++) {
			if (!begin[i].valid) break;
			printf++("\t%s", begin[i].dump());
		}
		printf("\n");
	}
}

unsigned trace(lze* begin, lze* end) {
	unsigned p = 0, n = 0;
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
	assert(offset);
	if (offset < 127)
		res.push_back(0x80 | offset);
	else
	{
		res.push_back(0xff);
		offset -= 127;
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
	if (LOG_DEBUG) dump_entries(table, end);
	unsigned n = trace(table, end);

	string output;
	size_t offset = 0;
	int zeroes = 0;
	size_t i = 0;
	unsigned max_depth = 0;
	auto emit_zeroes = [&]() {
		if (zeroes) {
			debug("%5u: %d zeroes, from %d\n", i - 1, zeroes, i - zeroes);
			gen_zeroes(output, zeroes);
			zeroes = 0;
		}
	};
	for (; i < input.size(); i++) {
		const lze& e = table[N * (i + 1) + n];
		if (n > max_depth) {
			debug("%5u: %s \t< new max depth\n", i, e.dump());
			max_depth = max(max_depth, n);
		} else {
			debug("%5u: %s\n", i, e.dump());
		}
		if (offset != e.offset) {
			emit_zeroes();
			debug("%5u: new offset %d (output@%zx)\n", i, e.offset, output.size());
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
			output.push_back(delta);
		} else {
			zeroes++;
		}
		debug("%5u: %02x[%d] ^ %02x = %02x\n", i, lastc, i - 1 - offset, c, delta);
		n = e.next;
	}
	emit_zeroes();
	debug("Best coding: %s\n", last->dump());
	debug("Max depth used: %u of %zu\n", max_depth, N);
	return output;
}

int main(int argc, const char *argv[]) {
	const string input = argc > 1 ? read_file(argv[1]) : read_stdin();
	fprintf(stderr, "%zu bytes of input\n", input.size());
	const string output = compress(input);
	fprintf(stderr, "%zu bytes of output\n", output.size());
	if (argc > 2)
		write_file(argv[2], output);
	else
		write_file("lz.out", output);
}
