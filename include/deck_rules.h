#ifndef DECK_RULES_H
#define DECK_RULES_H

#include <stdbool.h>

enum DeckType
{
    DECK_TYPE_RED,
    DECK_TYPE_BLUE,
    DECK_TYPE_YELLOW,
    DECK_TYPE_GREEN,
    DECK_TYPE_BLACK,
    DECK_TYPE_PAINTED,
    DECK_TYPE_MAX
};

int deck_get_starting_money(enum DeckType deck, int base_money);
int deck_get_hands_per_blind(enum DeckType deck, int base_hands);
int deck_get_discards_per_blind(enum DeckType deck, int base_discards);
int deck_get_hand_size(enum DeckType deck, int base_hand_size);
int deck_get_joker_capacity(enum DeckType deck, int base_capacity);
int deck_get_hand_cashout(enum DeckType deck, int hands, int discards);
bool deck_allows_interest(enum DeckType deck);

#endif
