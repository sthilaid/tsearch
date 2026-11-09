
tindex: src/tindex.h src/tindex.cpp
	clang++ -std=c++20 src/tindex.cpp -o tindex

tsearch: src/tsearch.cpp
	clang++ -std=c++20 src/tsearch.cpp -o tsearch

all: tsearch tindex
