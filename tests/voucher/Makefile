CC := gcc
CFLAGS := -I../../include -I. -g -O3 -std=gnu23 -Wall -Werror

SRC := voucher_test.c ../../source/voucher.c
OUT := build/voucher_test

$(OUT): $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^

build:
	mkdir -p build

clean:
	rm -f $(OUT)
