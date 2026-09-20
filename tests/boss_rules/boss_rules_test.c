#include "boss_rules.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void test_empty_history_has_no_target(void)
{
    uint16_t counts[FLUSH_FIVE + 1] = {0};
    assert(boss_ox_choose_hand(counts, FLUSH_FIVE + 1) == NONE);
    assert(boss_ox_choose_hand(NULL, FLUSH_FIVE + 1) == NONE);
    assert(boss_ox_choose_hand(counts, HIGH_CARD) == NONE);
}

static void test_highest_count_is_selected(void)
{
    uint16_t counts[FLUSH_FIVE + 1] = {0};
    counts[PAIR] = 7;
    counts[FLUSH] = 12;
    counts[STRAIGHT] = 9;
    assert(boss_ox_choose_hand(counts, FLUSH_FIVE + 1) == FLUSH);
}

static void test_tie_is_stable_and_bounded(void)
{
    uint16_t counts[FLUSH_FIVE + 2] = {0};
    counts[TWO_PAIR] = UINT16_MAX;
    counts[FLUSH] = UINT16_MAX;
    counts[FLUSH_FIVE + 1] = UINT16_MAX;
    assert(boss_ox_choose_hand(counts, FLUSH_FIVE + 2) == TWO_PAIR);
}

int main(void)
{
    test_empty_history_has_no_target();
    test_highest_count_is_selected();
    test_tie_is_stable_and_bounded();
    puts("boss rules tests passed");
    return 0;
}
