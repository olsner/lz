CFLAGS = -g -O2
CXXFLAGS = -g -O2 -std=gnu++11

all: programs test
programs: decode null_encode lz lit_conv
lz: sprintfxx.h
clean::
	rm decode null_encode lz lit_conv

test: test1 test2
test1: null_encode decode
	./null_encode <basics.txt >test_null_encoded.txt
	./decode <test_null_encoded.txt >test_output.txt 2>decode_stderr.txt
	cmp basics.txt test_output.txt
test2: lz decode
	./lz basics.txt test_lz.txt >lz_stderr.txt 2>&1
	./decode <test_lz.txt >test_output_lz.txt 2>lz_decode_stderr.txt
	cmp basics.txt test_output_lz.txt
