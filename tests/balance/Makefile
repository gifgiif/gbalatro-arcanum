CC := gcc
CFLAGS := -I../../include -I. -g -O3 -std=gnu23 -Wall -Werror

SRC := balance_test.c \
       ../../source/alchemy.c \
       ../../source/voucher.c \
       ../../source/planet.c \
       ../../source/deck_rules.c
OUT := build/balance_test

$(OUT): $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^

build:
	mkdir -p build

clean:
	rm -f $(OUT)
