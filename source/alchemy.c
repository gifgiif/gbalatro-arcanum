#include "alchemy.h"

#include <limits.h>
#include <string.h>

#define INFO(id_name, display_name, short_code, text, tier, min_sel, max_sel, needs_joker) \
    [ALCHEMICAL_##id_name] = {                                               \
        .name = display_name,                                                \
        .code = short_code,                                                  \
        .description = text,                                                 \
        .rarity = tier,                                                      \
        .cost = 4 + tier,                                                    \
        .min_selected = min_sel,                                             \
        .max_selected = max_sel,                                             \
        .blind_only = true,                                                  \
        .requires_joker = needs_joker                                        \
    }

static const AlchemicalInfo alchemical_registry[ALCHEMICAL_ID_COUNT] = {
    INFO(IGNIS, "Ignis", "IG", "Gain +1 discard this blind", ALCHEMICAL_COMMON, 0, 0, false),
    INFO(AQUA, "Aqua", "AQ", "Gain +1 hand this blind", ALCHEMICAL_COMMON, 0, 0, false),
    INFO(TERRA, "Terra", "TE", "Current blind target -15%", ALCHEMICAL_COMMON, 0, 0, false),
    INFO(AERO, "Aero", "AE", "Draw up to 4 cards into your current hand", ALCHEMICAL_COMMON, 0, 0, false),
    INFO(QUICKSILVER, "Quicksilver", "QS", "+2 hand size this blind and draw to fill", ALCHEMICAL_UNCOMMON, 0, 0, false),
    INFO(SALT, "Salt", "SA", "Gain $4 now", ALCHEMICAL_COMMON, 0, 0, false),
    INFO(SULFUR, "Sulfur", "SU", "Keep 1 hand and gain $2 per hand lost", ALCHEMICAL_UNCOMMON, 0, 0, false),
    INFO(PHOSPHORUS, "Phosphorus", "PH", "Shuffle all discards into deck", ALCHEMICAL_COMMON, 0, 0, false),
    INFO(BISMUTH, "Bismuth", "BI", "Select 1-2 active cards: temporary x1.5 mult", ALCHEMICAL_UNCOMMON, 1, 2, false),
    INFO(COBALT, "Cobalt", "CO", "Select poker hand: permanently up to +2 levels", ALCHEMICAL_RARE, 1, 5, false),
    INFO(ARSENIC, "Arsenic", "AR", "Swap remaining hands and discards", ALCHEMICAL_UNCOMMON, 0, 0, false),
    INFO(ANTIMONY, "Antimony", "AN", "Left Joker triggers +1 time this blind", ALCHEMICAL_RARE, 0, 0, true),
    INFO(SOAP, "Soap", "SO", "Select 1-3: replace from shuffled deck", ALCHEMICAL_COMMON, 1, 3, false),
    INFO(MANGANESE, "Manganese", "MN", "Select 1-4 clean active cards: x1.5 held", ALCHEMICAL_UNCOMMON, 1, 4, false),
    INFO(WAX, "Wax", "WA", "Select 1: make 2 temp copies and expand hand", ALCHEMICAL_RARE, 1, 1, false),
    INFO(BORAX, "Borax", "BO", "Select 1-4 ranked cards: use common suit", ALCHEMICAL_COMMON, 1, 4, false),
    INFO(GLASS, "Glass", "GL", "Select 1 clean active card: x2 when scored", ALCHEMICAL_UNCOMMON, 1, 1, false),
    INFO(MAGNET, "Magnet", "MA", "Select 1 ranked card: draw up to 2 matches", ALCHEMICAL_UNCOMMON, 1, 1, false),
    INFO(GOLD, "Gold", "GO", "Select 1-4 clean active cards: held end +$2", ALCHEMICAL_UNCOMMON, 1, 4, false),
    INFO(SILVER, "Silver", "SI", "Select 1-4 clean active cards: Lucky scored", ALCHEMICAL_UNCOMMON, 1, 4, false),
    INFO(OIL, "Oil", "OI", "Remove debuffs and face-down from hand", ALCHEMICAL_COMMON, 0, 0, false),
    INFO(ACID, "Acid", "AC", "Hide selected rank and refill hand from deck", ALCHEMICAL_RARE, 1, 1, false),
    INFO(BRIMSTONE, "Brimstone", "BR", "+2 hands/discards and mute first active Joker", ALCHEMICAL_RARE, 0, 0, true),
    INFO(URANIUM, "Uranium", "UR", "Select modified 1: copy mods to 3 clean cards", ALCHEMICAL_RARE, 1, 1, false),
};

#undef INFO

/* All 24 effects now pass the same bounded/no-op validation used at runtime. */
#define ALCHEMICAL_SHOP_POOL_MASK ((1U << ALCHEMICAL_ID_COUNT) - 1U)

static int saturating_add_int(int value, int amount)
{
    if (amount > 0 && value > INT_MAX - amount)
        return INT_MAX;
    if (amount < 0 && value < INT_MIN - amount)
        return INT_MIN;
    return value + amount;
}

static int clamp_nonnegative(int value)
{
    return value < 0 ? 0 : value;
}

const AlchemicalInfo* alchemical_get_info(enum AlchemicalId id)
{
    if ((unsigned int)id >= ALCHEMICAL_ID_COUNT)
        return NULL;
    return &alchemical_registry[id];
}

const char* alchemical_rarity_name(enum AlchemicalRarity rarity)
{
    switch (rarity)
    {
        case ALCHEMICAL_COMMON:
            return "COMMON";
        case ALCHEMICAL_UNCOMMON:
            return "UNCOMMON";
        case ALCHEMICAL_RARE:
            return "RARE";
        default:
            return "UNKNOWN";
    }
}

bool alchemical_is_shop_enabled(enum AlchemicalId id)
{
    return (unsigned int)id < ALCHEMICAL_ID_COUNT &&
           (ALCHEMICAL_SHOP_POOL_MASK & (1U << id)) != 0;
}

int alchemical_get_sell_value(enum AlchemicalId id)
{
    const AlchemicalInfo* info = alchemical_get_info(id);
    if (info == NULL)
        return 0;
    int value = info->cost / 2;
    return value > 0 ? value : 1;
}

int alchemical_wax_hand_capacity(int hand_size, int held_cards, int max_hand_size)
{
    if (hand_size < 1 || held_cards < 0 || max_hand_size < 2 ||
        held_cards > max_hand_size - 2)
    {
        return -1;
    }

    int required_capacity = held_cards + 2;
    int capacity = hand_size > required_capacity ? hand_size : required_capacity;
    return capacity <= max_hand_size ? capacity : -1;
}

bool alchemical_can_expand_hand(int hand_size, int amount, int max_hand_size)
{
    if (hand_size < 0 || amount <= 0 || max_hand_size < 0)
        return false;
    return hand_size <= max_hand_size - amount;
}

bool alchemical_can_add_bounded(int value, int amount, int maximum)
{
    if (value < 0 || amount <= 0 || maximum < 0 || amount > maximum)
        return false;
    return value <= maximum - amount;
}

bool alchemical_card_flag_can_apply(
    uint8_t current_flags,
    uint8_t target_flag,
    bool has_permanent_equivalent,
    uint8_t mutually_exclusive_flags
)
{
    /* Only one concrete flag may be requested at a time. */
    if (target_flag == 0 || (target_flag & (uint8_t)(target_flag - 1U)) != 0)
        return false;
    if (has_permanent_equivalent)
        return false;

    /* The target itself is always exclusive even if the caller omits it. */
    uint8_t blocked = mutually_exclusive_flags | target_flag;
    return (current_flags & blocked) == 0;
}

bool alchemical_level_hand(uint8_t* stored_level, int amount, int maximum)
{
    if (stored_level == NULL || amount <= 0 || maximum < 0 ||
        maximum > UINT8_MAX ||
        *stored_level >= maximum)
    {
        return false;
    }

    int next = *stored_level;
    next += amount > maximum - next ? maximum - next : amount;
    if (next == *stored_level)
        return false;
    *stored_level = (uint8_t)next;
    return true;
}

int alchemical_most_common_index(const int* counts, int count)
{
    if (counts == NULL || count <= 0)
        return -1;

    int best = 0;
    for (int i = 1; i < count; i++)
        if (counts[i] > counts[best])
            best = i;
    return best;
}

bool alchemical_copy_target_is_clean(
    uint8_t alchemy_flags,
    uint8_t enhancement,
    uint8_t edition,
    uint8_t seal
)
{
    return alchemy_flags == 0 && enhancement == 0 && edition == 0 && seal == 0;
}

bool alchemical_acid_can_leave_playable(int remaining_cards, int required_cards)
{
    if (remaining_cards < 0 || required_cards < 1)
        return false;
    return remaining_cards >= required_cards;
}

bool alchemical_phosphorus_can_rescue_hand(
    int held_cards,
    int discarded_cards,
    int required_cards
)
{
    if (held_cards < 0 || discarded_cards <= 0 || required_cards < 1)
        return false;
    return held_cards >= required_cards ||
           discarded_cards >= required_cards - held_cards;
}

bool alchemical_wax_can_rescue_hand(
    int hand_size,
    int held_cards,
    int wax_cards,
    int deck_size,
    int required_cards,
    int max_hand_size,
    int max_cards
)
{
    if (hand_size < 1 || held_cards < 1 || wax_cards < 0 || deck_size < held_cards ||
        required_cards < 1 || max_hand_size < 1 || max_cards < 1)
        return false;
    if (held_cards >= required_cards)
        return true;

    for (int used = 0; used < wax_cards; used++)
    {
        if (deck_size > max_cards - 2)
            return false;
        int expanded =
            alchemical_wax_hand_capacity(hand_size, held_cards, max_hand_size);
        if (expanded < 0)
            return false;
        hand_size = expanded;
        held_cards += 2;
        deck_size += 2;
        if (held_cards >= required_cards)
            return true;
    }
    return false;
}

bool alchemical_can_use(
    enum AlchemicalId id,
    bool blind_active,
    int selected_count,
    bool has_joker
)
{
    const AlchemicalInfo* info = alchemical_get_info(id);
    if (info == NULL)
        return false;
    if (info->blind_only && !blind_active)
        return false;
    if (selected_count < info->min_selected)
        return false;
    if (info->max_selected > 0 && selected_count > info->max_selected)
        return false;
    if (info->requires_joker && !has_joker)
        return false;
    return true;
}

void alchemical_inventory_reset(AlchemicalInventory* inventory)
{
    if (inventory == NULL)
        return;
    memset(inventory, 0, sizeof(*inventory));
    for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
        inventory->held[i] = ALCHEMICAL_INVALID_ID;
}

bool alchemical_inventory_add(AlchemicalInventory* inventory, enum AlchemicalId id)
{
    if (inventory == NULL || alchemical_get_info(id) == NULL ||
        inventory->count >= ALCHEMICAL_HELD_LIMIT)
    {
        return false;
    }
    inventory->held[inventory->count++] = (int8_t)id;
    return true;
}

static bool alchemical_inventory_remove_internal(
    AlchemicalInventory* inventory,
    int slot,
    bool count_as_used
)
{
    if (inventory == NULL || inventory->count > ALCHEMICAL_HELD_LIMIT || slot < 0 ||
        slot >= inventory->count)
        return false;
    for (int i = slot; i < inventory->count - 1; i++)
        inventory->held[i] = inventory->held[i + 1];
    inventory->held[--inventory->count] = ALCHEMICAL_INVALID_ID;
    if (count_as_used && inventory->used_count < UINT8_MAX)
        inventory->used_count++;
    return true;
}

bool alchemical_inventory_remove(AlchemicalInventory* inventory, int slot)
{
    return alchemical_inventory_remove_internal(inventory, slot, true);
}

bool alchemical_inventory_sell(AlchemicalInventory* inventory, int slot)
{
    return alchemical_inventory_remove_internal(inventory, slot, false);
}

bool alchemical_apply_scalar(enum AlchemicalId id, AlchemicalScalarState* state)
{
    if (state == NULL)
        return false;

    /*
     * Work on a copy and commit only after a real effect succeeds.  Callers use
     * the return value to decide whether to consume the card; mutating even one
     * field on a failed/no-op use would create a free partial effect.
     */
    AlchemicalScalarState next = *state;
    next.hands = clamp_nonnegative(next.hands);
    next.discards = clamp_nonnegative(next.discards);
    next.hand_size = clamp_nonnegative(next.hand_size);
    next.money = clamp_nonnegative(next.money);
    next.cards_to_draw = clamp_nonnegative(next.cards_to_draw);
    bool changed = false;

    switch (id)
    {
        case ALCHEMICAL_IGNIS:
            if (next.discards != INT_MAX)
            {
                next.discards = saturating_add_int(next.discards, 1);
                changed = true;
            }
            break;
        case ALCHEMICAL_AQUA:
            if (next.hands != INT_MAX)
            {
                next.hands = saturating_add_int(next.hands, 1);
                changed = true;
            }
            break;
        case ALCHEMICAL_TERRA:
        {
            if (next.blind_requirement <= 1)
                break;
            uint32_t old_requirement = next.blind_requirement;
            next.blind_requirement = (next.blind_requirement / 100U) * 85U +
                                     ((next.blind_requirement % 100U) * 85U) / 100U;
            if (next.blind_requirement == 0)
                next.blind_requirement = 1;
            changed = next.blind_requirement != old_requirement;
            break;
        }
        case ALCHEMICAL_AERO:
            if (next.cards_to_draw != INT_MAX)
            {
                next.cards_to_draw = saturating_add_int(next.cards_to_draw, 4);
                changed = true;
            }
            break;
        case ALCHEMICAL_QUICKSILVER:
            if (next.hand_size != INT_MAX)
            {
                next.hand_size = saturating_add_int(next.hand_size, 2);
                changed = true;
            }
            break;
        case ALCHEMICAL_SALT:
            if (next.money != INT_MAX)
            {
                next.money = saturating_add_int(next.money, 4);
                changed = true;
            }
            break;
        case ALCHEMICAL_SULFUR:
        {
            int removed_hands = next.hands > 1 ? next.hands - 1 : 0;
            if (removed_hands == 0)
                break;
            next.hands = next.hands > 0 ? 1 : 0;
            int reward = removed_hands > INT_MAX / 2 ? INT_MAX : removed_hands * 2;
            next.money = saturating_add_int(next.money, reward);
            changed = true;
            break;
        }
        case ALCHEMICAL_ARSENIC:
        {
            if (next.hands == next.discards)
                break;
            int old_hands = next.hands;
            next.hands = next.discards;
            next.discards = old_hands;
            changed = true;
            break;
        }
        case ALCHEMICAL_BRIMSTONE:
            if (next.hands != INT_MAX || next.discards != INT_MAX)
            {
                next.hands = saturating_add_int(next.hands, 2);
                next.discards = saturating_add_int(next.discards, 2);
                changed = true;
            }
            break;
        default:
            break;
    }

    if (changed)
        *state = next;
    return changed;
}
