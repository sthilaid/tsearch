clean:
	rm -rf bin/index.o bin/tindex bin/tsearch .tindex perf.data perf.data.old

index.o: src/index.cpp src/index.h
	clang++ -O2 -g -fno-omit-frame-pointer -std=c++20 -c src/index.cpp -o bin/index.o

tindex: src/tindex.cpp index.o
	clang++ -O2 -g -fno-omit-frame-pointer -std=c++20 src/tindex.cpp bin/index.o -o bin/tindex

tsearch: src/tsearch.cpp index.o
	clang++ -O2 -g -fno-omit-frame-pointer -std=c++20 src/tsearch.cpp bin/index.o -o bin/tsearch

perf: tindex
	perf record -F 999 -g -- bin/tindex -o .tindex `find ../linux/ -name *.c`

all: tsearch tindex
