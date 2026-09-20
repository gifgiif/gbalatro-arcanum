CC := gcc
CFLAGS := -I../../include -I. -g -O3 -std=gnu23 -Wall -Werror

SRC := planet_test.c ../../source/planet.c
OUT := build/planet_test

$(OUT): $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^

build:
	mkdir -p build

clean:
	rm -f $(OUT)
