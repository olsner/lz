#include <stdio.h>

#include <string>
using namespace std;

namespace {

typedef int Cost;
// x is the cost value for one whole byte
#define BYTE_COST(x) ((x) * 8)
#define BIT_COST(x) (x)

string read_stdin() {
	string res;
	char tmp[256];
	ssize_t n;
	while ((n = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
		res.append(tmp, n);
	}
	return res;
}

struct lze
{
	int offset; // current offset
	int length; // length since change of offset
	int cost; // cost in bytes to get this far

	bool operator==(const lze& other) const {
		return offset == other.offset && length == other.length && cost == other.cost;
	}
	void dump(const lze* prev = nullptr) const {
		printf("%d|%d|%d", offset, length, cost);
		// TODO Figure out which prev was the base for this (or actually save it)
	}

};

const unsigned LBMASK = 65535;
// Number of entries to try in "dynamic programming" solver
const size_t N = 4;

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
			printf("Result ");
			return n;
		}
	}
	if (n < N)
		dest[n++] = result;
	return n;
}

Cost lit_cost(int lit) {
	return BIT_COST(lit ? 8 : 1);
}
Cost length_cost(int len) {
	return BYTE_COST(1 + len/255);
}
// Amount to add to cost when increasing 'len' by 1
Cost incr_length_cost(int len) {
	return BYTE_COST(!((len + 1) % 255));
}

lze extend(const lze& prev, const char *begin, const char *pos) {
	return lze { prev.offset,
		prev.length + 1,
		prev.cost + lit_cost(pos[-prev.offset] ^ pos[0])
			+ incr_length_cost(prev.length) };
}

lze new_offset(const lze& prev, int offset, int lit) {
	return lze {
		offset,
		0,
		prev.cost + length_cost(offset) + 1 + lit_cost(lit) };
}

void add(const lze *prev, const char *begin, const char *pos, lze *dest) {
	int added = 0;
	// Length-extend each match by one (delta) literal
	for (int i = 0; i < N; i++) {
		added = insert_lze(dest, added, extend(prev[i], begin, pos));
	}
	// Look for new offsets to encode
	for (const char *b = pos; b >= begin; b--) {
		if (*b == pos[1]) {
			for (int i = 0; i < N; i++) {
				added = insert_lze(dest, added,
					new_offset(prev[i], 1 + pos - b, *pos));
			}
		}
	}
}

void dump_entries(const lze *begin, const lze *end) {
	unsigned n = 0;
	const lze *prev = nullptr;
	for (; begin < end; begin += N)
	{
		printf("%4u: ", n++);
		for (int i = 0; i < N; i++) {
			begin->dump(prev);
			printf("\t");
		}
		printf("\n");
		prev = begin;
	}
}

}

void compress(const string& input) {
	lze* table = new lze[(LBMASK + 1) * N];
	init(table, table + N);
	for (int i = 0; i < input.size(); i++) {
		lze* prev = table + (i & LBMASK) * N;
		lze* cur = prev + N;
		add(prev, &input[0], &input[i], cur);
	}
	dump_entries(table, table + std::min(input.size(), size_t(LBMASK) + 1) * N);
}

int main() {
	const string input = read_stdin();
	printf("%zu bytes of input\n", input.size());
	compress(input);
}
