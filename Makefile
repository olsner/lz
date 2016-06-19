CFLAGS = -g -O2
CXXFLAGS = -g -O2 -std=gnu++11

all: decode null_encode lz
lz: sprintfxx.h
clean::
	rm decode null_encode lz
