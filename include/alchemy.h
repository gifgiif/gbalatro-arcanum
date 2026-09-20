#ifndef ALCHEMY_H
#define ALCHEMY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALCHEMICAL_HELD_LIMIT     3
#define ALCHEMICAL_HAND_TYPE_COUNT 14
#define ALCHEMICAL_INVALID_ID     (-1)

/*
 * Independent GBA implementation inspired by Codex Arcanum.
 * See ALCHEMY_CREDITS.md and ALCHEMY_ADAPTATIONS.md.
 */
enum AlchemicalId
{
    ALCHEMICAL_IGNIS,
    ALCHEMICAL_AQUA,
    ALCHEMICAL_TERRA,
    ALCHEMICAL_AERO,
    ALCHEMICAL_QUICKSILVER,
    ALCHEMICAL_SALT,
    ALCHEMICAL_SULFUR,
    ALCHEMICAL_PHOSPHORUS,
    ALCHEMICAL_BISMUTH,
    ALCHEMICAL_COBALT,
    ALCHEMICAL_ARSENIC,
    ALCHEMICAL_ANTIMONY,
    ALCHEMICAL_SOAP,
    ALCHEMICAL_MANGANESE,
    ALCHEMICAL_WAX,
    ALCHEMICAL_BORAX,
    ALCHEMICAL_GLASS,
    ALCHEMICAL_MAGNET,
    ALCHEMICAL_GOLD,
    ALCHEMICAL_SILVER,
    ALCHEMICAL_OIL,
    ALCHEMICAL_ACID,
    ALCHEMICAL_BRIMSTONE,
    ALCHEMICAL_URANIUM,
    ALCHEMICAL_ID_COUNT
};

enum AlchemicalRarity
{
    ALCHEMICAL_COMMON,
    ALCHEMICAL_UNCOMMON,
    ALCHEMICAL_RARE
};

typedef struct
{
    const char* name;
    const char* code;
    const char* description;
    enum AlchemicalRarity rarity;
    uint8_t cost;
    uint8_t min_selected;
    uint8_t max_selected;
    bool blind_only;
    bool requires_joker;
} AlchemicalInfo;

typedef struct
{
    int8_t held[ALCHEMICAL_HELD_LIMIT];
    uint8_t count;
    uint8_t used_count;
    uint8_t hand_levels[ALCHEMICAL_HAND_TYPE_COUNT];
} AlchemicalInventory;

/*
 * Scalar effect adapter. Complex deck/card operations stay in game.c, while
 * bounded arithmetic and inventory rules remain host-testable.
 */
typedef struct
{
    int hands;
    int discards;
    int hand_size;
    int money;
    int cards_to_draw;
    uint32_t blind_requirement;
} AlchemicalScalarState;

const AlchemicalInfo* alchemical_get_info(enum AlchemicalId id);
const char* alchemical_rarity_name(enum AlchemicalRarity rarity);
bool alchemical_can_use(
    enum AlchemicalId id,
    bool blind_active,
    int selected_count,
    bool has_joker
);
bool alchemical_is_shop_enabled(enum AlchemicalId id);
int alchemical_get_sell_value(enum AlchemicalId id);
int alchemical_wax_hand_capacity(int hand_size, int held_cards, int max_hand_size);
bool alchemical_can_expand_hand(int hand_size, int amount, int max_hand_size);
bool alchemical_can_add_bounded(int value, int amount, int maximum);
bool alchemical_card_flag_can_apply(
    uint8_t current_flags,
    uint8_t target_flag,
    bool has_permanent_equivalent,
    uint8_t mutually_exclusive_flags
);
bool alchemical_level_hand(uint8_t* stored_level, int amount, int maximum);
int alchemical_most_common_index(const int* counts, int count);
bool alchemical_copy_target_is_clean(
    uint8_t alchemy_flags,
    uint8_t enhancement,
    uint8_t edition,
    uint8_t seal
);
bool alchemical_acid_can_leave_playable(int remaining_cards, int required_cards);
bool alchemical_phosphorus_can_rescue_hand(
    int held_cards,
    int discarded_cards,
    int required_cards
);
bool alchemical_wax_can_rescue_hand(
    int hand_size,
    int held_cards,
    int wax_cards,
    int deck_size,
    int required_cards,
    int max_hand_size,
    int max_cards
);

void alchemical_inventory_reset(AlchemicalInventory* inventory);
bool alchemical_inventory_add(AlchemicalInventory* inventory, enum AlchemicalId id);
bool alchemical_inventory_remove(AlchemicalInventory* inventory, int slot);
bool alchemical_inventory_sell(AlchemicalInventory* inventory, int slot);

bool alchemical_apply_scalar(enum AlchemicalId id, AlchemicalScalarState* state);

#endif // ALCHEMY_H
