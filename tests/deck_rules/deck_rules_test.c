#include "deck_rules.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void test_base_decks(void)
{
    assert(deck_get_discards_per_blind(DECK_TYPE_RED, 4) == 5);
    assert(deck_get_hands_per_blind(DECK_TYPE_BLUE, 4) == 5);
    assert(deck_get_starting_money(DECK_TYPE_YELLOW, 4) == 14);
    assert(deck_get_hand_cashout(DECK_TYPE_GREEN, 2, 3) == 7);
    assert(!deck_allows_interest(DECK_TYPE_GREEN));
    assert(deck_get_hands_per_blind(DECK_TYPE_BLACK, 4) == 3);
    assert(deck_get_joker_capacity(DECK_TYPE_BLACK, 5) == 6);
    assert(deck_get_hand_size(DECK_TYPE_PAINTED, 8) == 10);
    assert(deck_get_joker_capacity(DECK_TYPE_PAINTED, 5) == 4);
}

static void test_bounds(void)
{
    assert(deck_get_hands_per_blind(DECK_TYPE_BLACK, 1) == 1);
    assert(deck_get_joker_capacity(DECK_TYPE_PAINTED, 1) == 1);
    assert(deck_allows_interest(DECK_TYPE_RED));
    assert(deck_get_hand_cashout(DECK_TYPE_RED, 3, 4) == 3);
    assert(deck_get_starting_money(DECK_TYPE_YELLOW, INT_MAX) == INT_MAX);
    assert(deck_get_hands_per_blind(DECK_TYPE_BLUE, INT_MAX) == INT_MAX);
    assert(deck_get_discards_per_blind(DECK_TYPE_RED, INT_MAX) == INT_MAX);
    assert(deck_get_hand_size(DECK_TYPE_PAINTED, INT_MAX) == INT_MAX);
    assert(deck_get_joker_capacity(DECK_TYPE_BLACK, INT_MAX) == INT_MAX);
    assert(deck_get_hand_cashout(DECK_TYPE_GREEN, INT_MAX, INT_MAX) == INT_MAX);
}

int main(void)
{
    test_base_decks();
    test_bounds();
    puts("deck rules tests passed");
    return 0;
}
