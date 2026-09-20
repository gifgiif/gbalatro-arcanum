#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "alchemy.h"

static void test_registry_has_all_24_cards(void)
{
    int rarity_counts[3] = {0};
    assert(ALCHEMICAL_ID_COUNT == 24);
    for (int id = 0; id < ALCHEMICAL_ID_COUNT; id++)
    {
        const AlchemicalInfo* info = alchemical_get_info(id);
        assert(info != NULL);
        assert(info->name != NULL && strlen(info->name) > 0);
        assert(info->code != NULL && strlen(info->code) == 2);
        assert(info->description != NULL && strlen(info->description) > 0);
        /* ';' is the custom font's .9 glyph, not printable punctuation. */
        assert(strchr(info->description, ';') == NULL);
        assert(info->cost == 4 + info->rarity);
        assert(alchemical_get_sell_value(id) == info->cost / 2);
        assert(info->blind_only);
        assert(info->rarity >= ALCHEMICAL_COMMON && info->rarity <= ALCHEMICAL_RARE);
        rarity_counts[info->rarity]++;
    }
    assert(rarity_counts[ALCHEMICAL_COMMON] == 9);
    assert(rarity_counts[ALCHEMICAL_UNCOMMON] == 9);
    assert(rarity_counts[ALCHEMICAL_RARE] == 6);
    assert(alchemical_get_info(ALCHEMICAL_ID_COUNT) == NULL);
    assert(alchemical_get_info((enum AlchemicalId)-1) == NULL);
    assert(!alchemical_is_shop_enabled((enum AlchemicalId)-1));

    int shop_enabled = 0;
    for (int id = 0; id < ALCHEMICAL_ID_COUNT; id++)
        shop_enabled += alchemical_is_shop_enabled(id);
    assert(shop_enabled == ALCHEMICAL_ID_COUNT);
    assert(alchemical_is_shop_enabled(ALCHEMICAL_COBALT));
    assert(alchemical_is_shop_enabled(ALCHEMICAL_ANTIMONY));
    assert(alchemical_is_shop_enabled(ALCHEMICAL_MANGANESE));
    assert(alchemical_is_shop_enabled(ALCHEMICAL_BORAX));
    assert(alchemical_is_shop_enabled(ALCHEMICAL_SILVER));
    assert(alchemical_is_shop_enabled(ALCHEMICAL_URANIUM));
}

static void test_interaction_heavy_card_rules(void)
{
    /* Steel/Glass/Gold/Lucky are alternatives for one temporary enhancement. */
    const uint8_t steel = 1U << 1;
    const uint8_t glass = 1U << 2;
    const uint8_t gold = 1U << 3;
    const uint8_t lucky = 1U << 4;
    const uint8_t enhancement_mask = steel | glass | gold | lucky;
    assert(alchemical_card_flag_can_apply(0, steel, false, enhancement_mask));
    assert(!alchemical_card_flag_can_apply(steel, steel, false, enhancement_mask));
    assert(!alchemical_card_flag_can_apply(glass, steel, false, enhancement_mask));
    assert(!alchemical_card_flag_can_apply(gold, lucky, false, enhancement_mask));
    assert(!alchemical_card_flag_can_apply(0, glass, true, enhancement_mask));
    assert(!alchemical_card_flag_can_apply(0, 0, false, enhancement_mask));
    assert(!alchemical_card_flag_can_apply(0, steel | glass, false, enhancement_mask));
    /* Edition/suit/protection flags remain independent of enhancements. */
    assert(alchemical_card_flag_can_apply(1U, steel, false, enhancement_mask));

    uint8_t level = 18;
    assert(alchemical_level_hand(&level, 2, 20));
    assert(level == 20);
    assert(!alchemical_level_hand(&level, 2, 20));
    level = 19;
    assert(alchemical_level_hand(&level, 2, 20));
    assert(level == 20);
    assert(!alchemical_level_hand(NULL, 2, 20));
    assert(!alchemical_level_hand(&level, 0, 20));
    assert(!alchemical_level_hand(&level, 1, 300));

    const int suit_counts[] = {13, 13, 14, 12};
    assert(alchemical_most_common_index(suit_counts, 4) == 2);
    const int tied_counts[] = {13, 13, 13, 13};
    assert(alchemical_most_common_index(tied_counts, 4) == 0);
    assert(alchemical_most_common_index(NULL, 4) == -1);
    assert(alchemical_most_common_index(tied_counts, 0) == -1);

    assert(alchemical_copy_target_is_clean(0, 0, 0, 0));
    assert(!alchemical_copy_target_is_clean(1, 0, 0, 0));
    assert(!alchemical_copy_target_is_clean(0, 1, 0, 0));
    assert(!alchemical_copy_target_is_clean(0, 0, 1, 0));
    assert(!alchemical_copy_target_is_clean(0, 0, 0, 1));
}

static void test_shop_economy_has_no_instant_resale_arbitrage(void)
{
    for (int id = 0; id < ALCHEMICAL_ID_COUNT; id++)
    {
        const AlchemicalInfo* info = alchemical_get_info(id);
        assert(info != NULL);
        assert(info->cost >= 4 && info->cost <= 6);
        int sell = alchemical_get_sell_value(id);
        assert(sell >= 1);
        assert(sell <= info->cost / 2);

        /* The maximum 50% Voucher discount must not make buy-then-sell profit. */
        int discounted = (info->cost + 1) / 2;
        assert(sell <= discounted);
    }

    /* Repeated permanent upgrades saturate instead of wrapping. */
    uint8_t level = 0;
    for (int i = 0; i < 100000; i++)
        (void)alchemical_level_hand(&level, 2, 20);
    assert(level == 20);
}

static void test_use_conditions(void)
{
    assert(!alchemical_can_use(ALCHEMICAL_IGNIS, false, 0, false));
    assert(!alchemical_can_use((enum AlchemicalId)-1, true, 0, true));
    assert(alchemical_can_use(ALCHEMICAL_IGNIS, true, 0, false));
    assert(!alchemical_can_use(ALCHEMICAL_BISMUTH, true, 0, false));
    assert(alchemical_can_use(ALCHEMICAL_BISMUTH, true, 2, false));
    assert(!alchemical_can_use(ALCHEMICAL_BISMUTH, true, 3, false));
    assert(!alchemical_can_use(ALCHEMICAL_ANTIMONY, true, 0, false));
    assert(alchemical_can_use(ALCHEMICAL_ANTIMONY, true, 0, true));
    assert(!alchemical_can_use(ALCHEMICAL_BRIMSTONE, true, 0, false));
    assert(alchemical_can_use(ALCHEMICAL_BRIMSTONE, true, 0, true));

    const enum AlchemicalId up_to_four[] = {
        ALCHEMICAL_MANGANESE,
        ALCHEMICAL_BORAX,
        ALCHEMICAL_GOLD,
        ALCHEMICAL_SILVER
    };
    for (size_t i = 0; i < sizeof(up_to_four) / sizeof(up_to_four[0]); i++)
    {
        assert(!alchemical_can_use(up_to_four[i], true, 0, false));
        assert(alchemical_can_use(up_to_four[i], true, 4, false));
        assert(!alchemical_can_use(up_to_four[i], true, 5, false));
    }
    assert(alchemical_can_use(ALCHEMICAL_GLASS, true, 1, false));
    assert(!alchemical_can_use(ALCHEMICAL_GLASS, true, 2, false));
    assert(alchemical_can_use(ALCHEMICAL_SOAP, true, 3, false));
    assert(!alchemical_can_use(ALCHEMICAL_SOAP, true, 4, false));
    assert(alchemical_can_use(ALCHEMICAL_COBALT, true, 5, false));
    assert(!alchemical_can_use(ALCHEMICAL_COBALT, true, 6, false));
    assert(alchemical_can_use(ALCHEMICAL_WAX, true, 1, false));
    assert(!alchemical_can_use(ALCHEMICAL_WAX, true, 2, false));
    assert(alchemical_can_use(ALCHEMICAL_MAGNET, true, 1, false));
    assert(alchemical_can_use(ALCHEMICAL_ACID, true, 1, false));
    assert(alchemical_can_use(ALCHEMICAL_URANIUM, true, 1, false));
}

static void test_wax_expands_full_hand(void)
{
    assert(alchemical_wax_hand_capacity(8, 8, 16) == 10);
    assert(alchemical_wax_hand_capacity(10, 8, 16) == 10);
    assert(alchemical_wax_hand_capacity(14, 14, 16) == 16);
    assert(alchemical_wax_hand_capacity(16, 14, 16) == 16);
    assert(alchemical_wax_hand_capacity(16, 15, 16) == -1);
    assert(alchemical_wax_hand_capacity(0, 0, 16) == -1);
    assert(alchemical_wax_hand_capacity(8, -1, 16) == -1);
}

static void test_exact_hand_growth_bounds(void)
{
    assert(alchemical_can_expand_hand(8, 2, 16));
    assert(alchemical_can_expand_hand(14, 2, 16));
    assert(!alchemical_can_expand_hand(15, 2, 16));
    assert(!alchemical_can_expand_hand(16, 2, 16));
    assert(!alchemical_can_expand_hand(8, 0, 16));
    assert(!alchemical_can_expand_hand(-1, 2, 16));

    assert(alchemical_can_add_bounded(97, 2, 99));
    assert(!alchemical_can_add_bounded(98, 2, 99));
    assert(!alchemical_can_add_bounded(99, 2, 99));
    assert(!alchemical_can_add_bounded(-1, 2, 99));
    assert(!alchemical_can_add_bounded(4, 0, 99));
}

static void test_acid_preserves_a_legal_followup(void)
{
    assert(alchemical_acid_can_leave_playable(1, 1));
    assert(!alchemical_acid_can_leave_playable(0, 1));
    assert(alchemical_acid_can_leave_playable(5, 5));
    assert(!alchemical_acid_can_leave_playable(4, 5));
    assert(!alchemical_acid_can_leave_playable(-1, 1));
    assert(!alchemical_acid_can_leave_playable(8, 0));
}

static void test_phosphorus_can_rescue_an_empty_deck(void)
{
    assert(alchemical_phosphorus_can_rescue_hand(0, 1, 1));
    assert(alchemical_phosphorus_can_rescue_hand(3, 2, 5));
    assert(!alchemical_phosphorus_can_rescue_hand(3, 1, 5));
    assert(!alchemical_phosphorus_can_rescue_hand(0, 0, 1));
    assert(!alchemical_phosphorus_can_rescue_hand(-1, 5, 1));
    assert(!alchemical_phosphorus_can_rescue_hand(0, 5, 0));
}

static void test_wax_can_rescue_psychic_hand(void)
{
    assert(alchemical_wax_can_rescue_hand(8, 3, 1, 52, 5, 16, 80));
    assert(alchemical_wax_can_rescue_hand(8, 1, 2, 52, 5, 16, 80));
    assert(!alchemical_wax_can_rescue_hand(8, 2, 1, 52, 5, 16, 80));
    assert(!alchemical_wax_can_rescue_hand(8, 0, 3, 52, 5, 16, 80));
    assert(!alchemical_wax_can_rescue_hand(16, 15, 1, 52, 16, 16, 80));
    assert(!alchemical_wax_can_rescue_hand(8, 3, 1, 79, 5, 16, 80));
    assert(alchemical_wax_can_rescue_hand(8, 5, 0, 52, 5, 16, 80));
}

static void test_inventory_limit_and_removal(void)
{
    AlchemicalInventory inventory;
    alchemical_inventory_reset(&inventory);
    assert(inventory.count == 0);
    assert(inventory.held[0] == ALCHEMICAL_INVALID_ID);
    assert(alchemical_inventory_add(&inventory, ALCHEMICAL_IGNIS));
    assert(alchemical_inventory_add(&inventory, ALCHEMICAL_AQUA));
    assert(alchemical_inventory_add(&inventory, ALCHEMICAL_TERRA));
    assert(!alchemical_inventory_add(&inventory, ALCHEMICAL_AERO));
    assert(alchemical_inventory_remove(&inventory, 0));
    assert(inventory.count == 2);
    assert(inventory.held[0] == ALCHEMICAL_AQUA);
    assert(inventory.used_count == 1);
    assert(!alchemical_inventory_remove(&inventory, 2));
    assert(!alchemical_inventory_add(&inventory, ALCHEMICAL_ID_COUNT));
    assert(!alchemical_inventory_add(NULL, ALCHEMICAL_IGNIS));
    assert(!alchemical_inventory_remove(NULL, 0));

    inventory.count = UINT8_MAX;
    assert(!alchemical_inventory_add(&inventory, ALCHEMICAL_IGNIS));
    assert(!alchemical_inventory_remove(&inventory, 0));

    alchemical_inventory_reset(&inventory);
    inventory.used_count = UINT8_MAX;
    assert(alchemical_inventory_add(&inventory, ALCHEMICAL_IGNIS));
    assert(alchemical_inventory_remove(&inventory, 0));
    assert(inventory.used_count == UINT8_MAX);

    alchemical_inventory_reset(&inventory);
    assert(alchemical_inventory_add(&inventory, ALCHEMICAL_AERO));
    assert(alchemical_inventory_sell(&inventory, 0));
    assert(inventory.count == 0);
    assert(inventory.used_count == 0);
    assert(!alchemical_inventory_sell(&inventory, 0));
}

static void test_scalar_effects(void)
{
    AlchemicalScalarState state = {
        .hands = 4,
        .discards = 3,
        .hand_size = 8,
        .money = 4,
        .blind_requirement = 101
    };

    assert(alchemical_apply_scalar(ALCHEMICAL_IGNIS, &state));
    assert(state.discards == 4);
    assert(alchemical_apply_scalar(ALCHEMICAL_AQUA, &state));
    assert(state.hands == 5);
    assert(alchemical_apply_scalar(ALCHEMICAL_TERRA, &state));
    assert(state.blind_requirement == 85);
    assert(alchemical_apply_scalar(ALCHEMICAL_AERO, &state));
    assert(state.cards_to_draw == 4);
    assert(state.hand_size == 8);
    assert(alchemical_apply_scalar(ALCHEMICAL_QUICKSILVER, &state));
    assert(state.hand_size == 10);
    assert(alchemical_apply_scalar(ALCHEMICAL_SALT, &state));
    assert(state.money == 8);
    assert(alchemical_apply_scalar(ALCHEMICAL_ARSENIC, &state));
    assert(state.hands == 4 && state.discards == 5);
    assert(alchemical_apply_scalar(ALCHEMICAL_BRIMSTONE, &state));
    assert(state.hands == 6 && state.discards == 7);
}

static void test_sulfur_and_bounds(void)
{
    AlchemicalScalarState state = {.hands = 4, .money = 0};
    assert(alchemical_apply_scalar(ALCHEMICAL_SULFUR, &state));
    assert(state.hands == 1);
    assert(state.money == 6);

    state = (AlchemicalScalarState){
        .hands = -10,
        .discards = -2,
        .hand_size = -1,
        .money = -20,
        .blind_requirement = 2
    };
    assert(alchemical_apply_scalar(ALCHEMICAL_TERRA, &state));
    assert(state.hands == 0 && state.discards == 0 && state.money == 0);
    assert(state.blind_requirement == 1);

    state.hands = INT_MAX;
    assert(!alchemical_apply_scalar(ALCHEMICAL_AQUA, &state));
    assert(state.hands == INT_MAX);

    state = (AlchemicalScalarState){.hands = INT_MAX, .money = INT_MAX};
    assert(alchemical_apply_scalar(ALCHEMICAL_SULFUR, &state));
    assert(state.hands == 1);
    assert(state.money == INT_MAX);

    state = (AlchemicalScalarState){.blind_requirement = UINT32_MAX};
    assert(alchemical_apply_scalar(ALCHEMICAL_TERRA, &state));
    assert(state.blind_requirement == 3650722200U);

    assert(!alchemical_apply_scalar(ALCHEMICAL_WAX, &state));
    assert(!alchemical_apply_scalar(ALCHEMICAL_IGNIS, NULL));

    state = (AlchemicalScalarState){.hand_size = 8};
    assert(alchemical_apply_scalar(ALCHEMICAL_AERO, &state));
    assert(state.hand_size == 8);
    assert(state.cards_to_draw == 4);

    state = (AlchemicalScalarState){
        .hands = 1,
        .discards = 1,
        .hand_size = INT_MAX,
        .money = INT_MAX,
        .blind_requirement = 1,
        .cards_to_draw = INT_MAX
    };
    assert(!alchemical_apply_scalar(ALCHEMICAL_IGNIS, &(AlchemicalScalarState){
        .discards = INT_MAX
    }));
    assert(!alchemical_apply_scalar(ALCHEMICAL_TERRA, &state));
    assert(!alchemical_apply_scalar(ALCHEMICAL_AERO, &state));
    assert(!alchemical_apply_scalar(ALCHEMICAL_QUICKSILVER, &state));
    assert(!alchemical_apply_scalar(ALCHEMICAL_SALT, &state));
    assert(!alchemical_apply_scalar(ALCHEMICAL_SULFUR, &state));
    assert(!alchemical_apply_scalar(ALCHEMICAL_ARSENIC, &state));
    state.hands = INT_MAX;
    state.discards = INT_MAX;
    assert(!alchemical_apply_scalar(ALCHEMICAL_BRIMSTONE, &state));
}

static void test_failed_scalar_effects_are_transactional(void)
{
    AlchemicalScalarState state = {
        .hands = -2,
        .discards = INT_MAX,
        .hand_size = -3,
        .money = -4,
        .cards_to_draw = -5,
        .blind_requirement = 100
    };
    AlchemicalScalarState before = state;
    assert(!alchemical_apply_scalar(ALCHEMICAL_IGNIS, &state));
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    assert(!alchemical_apply_scalar(ALCHEMICAL_WAX, &state));
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(!alchemical_apply_scalar((enum AlchemicalId)-1, &state));
    assert(memcmp(&state, &before, sizeof(state)) == 0);
}

int main(void)
{
    test_registry_has_all_24_cards();
    test_interaction_heavy_card_rules();
    test_shop_economy_has_no_instant_resale_arbitrage();
    test_use_conditions();
    test_wax_expands_full_hand();
    test_exact_hand_growth_bounds();
    test_acid_preserves_a_legal_followup();
    test_phosphorus_can_rescue_an_empty_deck();
    test_wax_can_rescue_psychic_hand();
    test_inventory_limit_and_removal();
    test_scalar_effects();
    test_sulfur_and_bounds();
    test_failed_scalar_effects_are_transactional();
    puts("alchemy tests passed");
    return 0;
}
