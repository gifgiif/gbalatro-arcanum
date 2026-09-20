#include "deck_rules.h"

#include <limits.h>

static int saturating_add_int(int value, int amount)
{
    if (amount > 0 && value > INT_MAX - amount)
        return INT_MAX;
    if (amount < 0 && value < INT_MIN - amount)
        return INT_MIN;
    return value + amount;
}

int deck_get_starting_money(enum DeckType deck, int base_money)
{
    return deck == DECK_TYPE_YELLOW ? saturating_add_int(base_money, 10) : base_money;
}

int deck_get_hands_per_blind(enum DeckType deck, int base_hands)
{
    int hands = saturating_add_int(base_hands, deck == DECK_TYPE_BLUE ? 1 : 0);
    hands = saturating_add_int(hands, deck == DECK_TYPE_BLACK ? -1 : 0);
    return hands < 1 ? 1 : hands;
}

int deck_get_discards_per_blind(enum DeckType deck, int base_discards)
{
    return saturating_add_int(base_discards, deck == DECK_TYPE_RED ? 1 : 0);
}

int deck_get_hand_size(enum DeckType deck, int base_hand_size)
{
    return saturating_add_int(base_hand_size, deck == DECK_TYPE_PAINTED ? 2 : 0);
}

int deck_get_joker_capacity(enum DeckType deck, int base_capacity)
{
    if (deck == DECK_TYPE_BLACK)
        return saturating_add_int(base_capacity, 1);
    if (deck == DECK_TYPE_PAINTED)
        return base_capacity > 1 ? base_capacity - 1 : 1;
    return base_capacity;
}

int deck_get_hand_cashout(enum DeckType deck, int hands, int discards)
{
    if (deck != DECK_TYPE_GREEN)
        return hands;
    int hand_reward =
        hands > INT_MAX / 2 ? INT_MAX
        : hands < INT_MIN / 2 ? INT_MIN
                              : hands * 2;
    return saturating_add_int(hand_reward, discards);
}

bool deck_allows_interest(enum DeckType deck)
{
    return deck != DECK_TYPE_GREEN;
}
