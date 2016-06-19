CFLAGS = -g -O0
CXXFLAGS = -g -O0 -std=gnu++11

all: decode null_encode lz
clean::
	rm decode null_encode lz
