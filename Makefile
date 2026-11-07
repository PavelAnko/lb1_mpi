CC      := mpicc
CC_HOST := gcc
CFLAGS  := -O2 -std=c11 -Wall -Wextra -Isrc
SRCS    := src/main.c src/array_sum.c src/block.c src/benchmark.c
HEADERS := src/array_sum.h src/block.h src/benchmark.h
TARGET  := array_sum

NP    ?= 4
N     ?= 1000000000
ITERS ?= 3

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

test: tests/test_block.c src/block.c src/block.h
	$(CC_HOST) -std=c11 -Wall -Wextra -Isrc -o test_block tests/test_block.c src/block.c
	./test_block

run: $(TARGET)
	mpirun -np $(NP) ./$(TARGET) $(N) $(ITERS)

clean:
	rm -f $(TARGET) $(TARGET).exe test_block test_block.exe src/*.o
