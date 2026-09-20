#ifndef BOSS_RULES_H
#define BOSS_RULES_H

#include "hand_types.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Pick The Ox target once, when its Blind starts.  The first hand type wins
 * ties so the result is stable and never changes after playing a hand.
 */
enum HandType boss_ox_choose_hand(
    const uint16_t* hand_play_counts,
    size_t hand_type_count
);

#endif // BOSS_RULES_H
