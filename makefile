clean:
	rm -f index.o tindex tsearch

index.o: src/index.cpp src/index.h
	clang++ -std=c++20 -c src/index.cpp -o index.o

tindex: src/tindex.cpp index.o
	clang++ -std=c++20 src/tindex.cpp index.o -o tindex

tsearch: src/tsearch.cpp index.o
	clang++ -std=c++20 src/tsearch.cpp index.o -o tsearch

all: tsearch tindex
