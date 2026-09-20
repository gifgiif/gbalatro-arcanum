CC := gcc
CFLAGS := -I../../include -I. -g -O3 -std=gnu23 -Wall -Werror

SRC := boss_rules_test.c ../../source/boss_rules.c
OUT := build/boss_rules_test

$(OUT): $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^

build:
	mkdir -p build

clean:
	rm -f $(OUT)
