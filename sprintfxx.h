#define _ /*
# C++ified snprintf/printf functions.
#
# Adds "printf++" and "snprintf++" that extends printf with C++ support.
#
# The extended functions accept std::string values directly # without having to
# call string::c_str() manually. The extended snprintf++ also adds the
# ability to write directly into std::strings.
#
# When run through bash, this file compiles and runs its own test suite.

# C++ user? SKIP FORWARD, this is filthy shell code.
# ---8<---

tmp=`mktemp`
if which valgrind &>/dev/null; then
	valgrind="valgrind --track-fds=yes --leak-check=yes"
fi
g++ -g -Os -Wall -Dtest_snprintfxx -x c++ "$0" -std=c++11 -o $tmp && $valgrind $tmp
e=$?
rm $tmp
exit $e

# ---8<---
# C++ user? C++ starts down here:
#*/

#include <cstdio>
#include <string>
#include <stdarg.h>

const char *VarargifyXX(const std::string &s) { return s.c_str(); }
template <typename T> T VarargifyXX(T t) { return t; }

template <typename T>
struct VarargWrap;
template <typename Ret, typename... Args>
struct VarargWrap<Ret (*)(Args..., ...)>
{
	typedef Ret (*FunP)(Args..., ...);
	FunP fun;

	VarargWrap(FunP fp): fun(fp) {}

	template <typename... VarArgs>
	Ret operator()(Args... args, VarArgs... varargs) {
		return fun(args..., VarargifyXX(varargs)...);
	}
};
template <typename T>
VarargWrap<T> vararg_wrap(T&& t) {
	return t;
}

struct SnprintfXX {
	template <typename... Args>
	void operator()(std::string& buf, const char *fmt, Args... args) {
		char *bufp = NULL;
		size_t bufsize = 0;
		FILE *fp = open_memstream(&bufp, &bufsize);
		fprintf(fp, fmt, VarargifyXX(args)...);
		fclose(fp);
		buf.assign(bufp, bufsize);
		free(bufp);
	}

	template <typename... Args>
	int operator()(char *buf, size_t size, const char *fmt, Args... args) {
		return snprintf(buf, size, fmt, VarargifyXX(args)...);
	}
};
struct SnprintfWrapper {
	template <typename... Args>
	int operator()(char *buf, size_t size, const char *fmt, Args... args) {
		return snprintf(buf, size, fmt, args...);
	}

	SnprintfXX operator++(int) {
		return SnprintfXX();
	}
};
struct SprintfXX {
	template <typename... Args>
	std::string operator()(const char *fmt, Args... args) {
		char *bufp = NULL;
		size_t bufsize = 0;
		FILE *fp = open_memstream(&bufp, &bufsize);
		fprintf(fp, fmt, VarargifyXX(args)...);
		fclose(fp);
		std::string buf(bufp, bufsize);
		free(bufp);
		return buf;
	}
};
struct SprintfWrapper {
	SprintfXX operator++(int) {
		return SprintfXX();
	}
};

struct PrintfWrapper {
	template <typename... Args>
	int operator()(const char *fmt, Args... args) {
		// With 'args' as a pack rather than a vararg, we can't use the nice
		// gcc format warnings. There are also problems with our additions -
		// gcc by itself might e.g. warn that std::string is not valid for %s.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
		return printf(fmt, args...);
#pragma GCC diagnostic pop
	}

	VarargWrap<decltype(&printf)> operator++(int) {
		return &printf;
	}
};
SnprintfWrapper __snprintf;
SprintfWrapper __sprintf;
PrintfWrapper __printf;

#define snprintf __snprintf
#define sprintf __sprintf
#define printf __printf

#ifdef test_snprintfxx
#include <assert.h>

int main() {
	// TODO Make this rather print a "ok" or not status, this doesn't include
	// much in way of "expected results" except that it should compile.
	printf("Stupid test suite...\n");

	// Compile test only
	const char *c = VarargifyXX(std::string("asdf"));
	(void)c;

	// Test some basic functionality
	std::string buf;
	snprintf++(buf, "test %d %x %o", 1, 2, 3);
	printf++("buf: \"%s\"\n", buf);
	const std::string bak = buf;
	buf = std::string(buf.size(), ' ');
	snprintf++(&buf[0], buf.size() + 1, "test %d %x %o", 1, 2, 3);
	printf++("buf: \"%s\" (%zu)\n", buf, buf.size());
	printf++("bak: \"%s\" (%zu)\n", bak, bak.size());
	assert(buf == bak);
	const std::string testConst = "const";
	std::string testNonConst = "non-const";
	char cbuf[256];
	snprintf++(cbuf, sizeof(cbuf), "%s string and %s string",
		testConst, testNonConst);
	printf++("cbuf: \"%s\"\n", cbuf);

	std::string test("text in stdstring");
	snprintf++(buf, "test \"%s\"", test);
	printf++("buf: \"%s\"\n", buf);
}
#endif
