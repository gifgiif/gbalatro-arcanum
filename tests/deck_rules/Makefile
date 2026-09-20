CC := gcc
CFLAGS := -I../../include -I. -g -O3 -std=gnu23 -Wall -Werror

SRC := deck_rules_test.c ../../source/deck_rules.c
OUT := build/deck_rules_test

$(OUT): $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^

build:
	mkdir -p build

clean:
	rm -f $(OUT)
