clean:
	rm -rf bin/index.o bin/tindex bin/tsearch .tindex perf.data perf.data.old

ifeq ($(DEBUG), 1)
OPTIM=-g
else
OPTIM=-O2
endif

CC=clang++
#CC=F:\.nuget\dd\llvm.22.1.2-ubi03\tools\bin\clang++.exe

#PROFILE=-fno-omit-frame-pointer -g
PROFILE=

.PHONY=all clean perf

bin:
	mkdir -p bin

bin/index.o: src/index.cpp src/index.h | bin
	${CC} ${OPTIM} ${PROFILE} -std=c++20 -c src/index.cpp -o bin/index.o

bin/tindex: src/tindex.cpp bin/index.o | bin
	${CC} ${OPTIM} ${PROFILE} -std=c++20 src/tindex.cpp bin/index.o -o bin/tindex

bin/tsearch: src/tsearch.cpp bin/index.o | bin
	${CC} ${OPTIM} ${PROFILE} -std=c++20 src/tsearch.cpp bin/index.o -o bin/tsearch

perf: tindex
	perf record -F 999 -g -- bin/tindex -o .tindex `find ../linux/ -name *.c`

all: bin/tsearch bin/tindex
