CC := gcc
CFLAGS := -I../../include -I. -g -O3 -std=gnu23 -Wall -Werror

SRC := alchemy_test.c ../../source/alchemy.c
OUT := build/alchemy_test

$(OUT): $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^

build:
	mkdir -p build

clean:
	rm -f $(OUT)
