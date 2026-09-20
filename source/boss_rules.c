#include "boss_rules.h"

enum HandType boss_ox_choose_hand(
    const uint16_t* hand_play_counts,
    size_t hand_type_count
)
{
    if (hand_play_counts == NULL || hand_type_count <= HIGH_CARD)
        return NONE;

    size_t limit = hand_type_count;
    if (limit > (size_t)FLUSH_FIVE + 1)
        limit = (size_t)FLUSH_FIVE + 1;

    enum HandType best_hand = NONE;
    uint16_t best_count = 0;
    for (size_t i = HIGH_CARD; i < limit; i++)
    {
        if (hand_play_counts[i] > best_count)
        {
            best_count = hand_play_counts[i];
            best_hand = (enum HandType)i;
        }
    }
    return best_hand;
}
