#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
using namespace std;

#include "sprintfxx.h"

namespace {

typedef int Cost;
// x is the cost value for one whole byte
#define BYTE_COST(x) ((x) * 8)
#define BIT_COST(x) (x)
#define VAL_COST(x) ilog2(x)

// 1 + ilog2(v), ilog2(0) = 0
int ilog2(int v) {
	return (v > 128) + (v > 64) + (v > 32) + (v > 16) + (v > 8) + (v > 4) + (v > 2) + (v > 1) + (v > 0);
}

Cost lit_cost(int lit) {
	return VAL_COST(lit);
}
Cost length_cost(int len) {
	return len > 255 ? BYTE_COST(1 + len/255) : VAL_COST(len);
}
// Amount to add to cost when increasing 'len' by 1
Cost incr_length_cost(int len) {
	// return BYTE_COST(!((len + 1) % 255));
	return length_cost(len + 1) - length_cost(len);
}

string read_stdin() {
	string res;
	char tmp[256];
	ssize_t n;
	while ((n = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
		res.append(tmp, n);
	}
	return res;
}

void write_file(const char *fname, const string& data) {
	FILE *fp = fopen(fname, "wb");
	fwrite(data.data(), data.size(), 1, fp);
	fclose(fp);
}

struct lze
{
	int offset; // current offset
	int length; // length since change of offset
	int cost; // cost in bytes to get this far
	char prev : 4; // index of previous entry
	char next : 4; // index of next entry, filled in by trace

	bool operator==(const lze& other) const {
		return offset == other.offset && length == other.length && cost == other.cost;
	}
	string dump(const lze* prev = nullptr) const {
		return sprintf++("%3d|%4d|%d", offset, length, cost);
		// TODO Figure out which prev was the base for this (or actually save it)
	}

};

const unsigned LBMASK = 65535;
// Number of entries to try in "dynamic programming" solver
const size_t N = 4;
const size_t MIN_MATCH = 4;

void init(lze* begin, lze* end) {
	while (begin < end) {
		*begin++ = lze { 0, 0, 0 };
	}
}

int insert_lze(lze *dest, int n, lze result) {
	for (int i = 0; i < n; i++) {
		if (result.cost < dest[i].cost) {
			swap(dest[i], result);
		} else if (result == dest[i]) {
			// Exact result already present
//			printf++("Result %s already at %d\n", result.dump(), i);
			return n;
		}
	}
	if (n < N)
		dest[n++] = result;
	return n;
}

lze extend(const lze& prev, const char *begin, const char *pos) {
	char lastc = prev.offset < pos - begin ? pos[-1-prev.offset] : 0;
//	printf++("Extending %s with %02x ^ %02x at %td\n",
//		prev.dump(), lastc, pos[0], pos - begin);
	return lze { prev.offset,
		prev.length + 1,
		prev.cost + lit_cost(lastc ^ pos[0])
			+ incr_length_cost(prev.length) };
}

lze new_offset(const lze& prev, int offset, int lit) {
	return lze {
		offset,
		0,
		prev.cost + length_cost(offset) + length_cost(0) + lit_cost(lit) };
}

void add(const lze *prev, const char *begin, const char *pos, lze *dest) {
	int added = 0;
	// Length-extend each match by one (delta) literal
	for (int i = 0; i < N; i++) {
		lze e = extend(prev[i], begin, pos);
		e.prev = i;
		added = insert_lze(dest, added, e);
	}
	// Look for new offsets to encode
	for (const char *b = pos; b >= begin; b--) {
		if (!memcmp(b, pos+1, MIN_MATCH)) {
			for (int i = 0; i < N; i++) {
				lze e = new_offset(prev[i], 1 + pos - b, *pos);
				e.prev = i;
//				printf++("match at %d (from %d): %s\n", b - begin, pos - begin, e.dump(prev));
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
		for (int i = 0; i < N; i++) {
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

string gen(int offset, int length, const char *begin, const char *pos) {
	string res;
	genint(res, offset);
	genint(res, length);
	pos -= length;
	res.push_back(*pos++);
	while (length--) {
		int c = offset > pos - begin ? 0 : pos[-1-offset];
		res.push_back(*pos++ ^ c);
	}
	return res;
}

}

string compress(const string& input) {
	lze* table = new lze[(LBMASK + 1) * N];
	init(table, table + N);
	for (int i = 0; i < input.size(); i++) {
		lze* prev = table + (i & LBMASK) * N;
		lze* cur = prev + N;
		add(prev, &input[0], &input[i], cur);
		// Decision about committing to a parse at cur[0] (the best so far),
		// flushing back to the last flush.
	}
	lze* end = table + std::min(input.size(), size_t(LBMASK) + 1) * N;
	lze* last = end - N;
	//dump_entries(table, end);
	printf++("Best coding: %s\n", last->dump(last - N));
	int n = trace(table, end);

	string output;
	int offset = 0;
	int last_length = 0;
	for (int i = 0; i < input.size(); i++) {
		const lze& e = table[N * i + n];
		last_length++;
		// This happens at the *end* of each run at the same offset.
		// That is, we should output the last_length characters of the last run
		// after the header, then start collecting the next run.
		if (offset != e.offset) {
			printf++("%5u: %s\n", i, e.dump());
			output += gen(offset, last_length, &input[0], &input[i]);
			last_length = 0;
			offset = e.offset;
		}
		n = e.next;
	}
	return output;
}

int main() {
#if 0
	for (int v = 0; v < 256; v++) {
		printf("%d: %d\n", v, ilog2(v));
	}
#endif

	const string input = read_stdin();
	fprintf(stderr, "%zu bytes of input\n", input.size());
	const string output = compress(input);
	fprintf(stderr, "%zu bytes of output\n", output.size());
	write_file("lz.out", output);
}