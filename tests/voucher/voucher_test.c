#include "voucher.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void test_registry(void)
{
    assert(VOUCHER_COUNT == 12);
    for (int id = 0; id < VOUCHER_COUNT; id++)
    {
        const VoucherInfo* info = voucher_get_info((enum VoucherId)id);
        assert(info != NULL);
        assert(info->name != NULL && strlen(info->name) > 0);
        assert(info->description != NULL && strlen(info->description) > 0);
        /* ';' is a control glyph in the compact GBA font, not punctuation. */
        assert(strchr(info->description, ';') == NULL);
        assert(info->cost == VOUCHER_BASE_COST);
    }
    assert(voucher_get_info(VOUCHER_COUNT) == NULL);
    assert(voucher_get_info((enum VoucherId)-1) == NULL);
}

static void test_offer_lifecycle_and_prerequisites(void)
{
    VoucherState state;
    voucher_state_reset(&state);
    assert(state.offer_id == VOUCHER_INVALID_ID);
    assert(!voucher_can_offer(&state, VOUCHER_OVERSTOCK_PLUS));
    assert(voucher_can_offer(&state, VOUCHER_OVERSTOCK));

    assert(voucher_prepare_offer(&state, 1, 0));
    assert(state.offer_id == VOUCHER_OVERSTOCK);
    assert(voucher_buy_offer(&state));
    assert(voucher_is_owned(&state, VOUCHER_OVERSTOCK));
    assert(state.offer_id == VOUCHER_INVALID_ID);

    /* A bought voucher cannot be replaced by rerolling the same Ante. */
    assert(!voucher_prepare_offer(&state, 1, 7));
    assert(voucher_prepare_offer(&state, 2, 0));
    assert(state.offer_id == VOUCHER_OVERSTOCK_PLUS);
    assert(!voucher_grant(&state, VOUCHER_OVERSTOCK));
    assert(!voucher_grant(&state, VOUCHER_COUNT));
}

static void test_effects_and_limits(void)
{
    VoucherState state;
    voucher_state_reset(&state);
    assert(voucher_get_shop_joker_slots(&state) == 2);
    assert(voucher_discount_price(&state, 5) == 5);
    assert(voucher_get_reroll_cost(&state, 1) == 1);
    assert(voucher_get_hands_per_blind(&state, 4) == 4);
    assert(voucher_get_discards_per_blind(&state, 4) == 4);
    assert(voucher_get_joker_capacity(&state, 5) == 5);

    assert(voucher_grant(&state, VOUCHER_OVERSTOCK));
    assert(voucher_grant(&state, VOUCHER_OVERSTOCK_PLUS));
    assert(voucher_get_shop_joker_slots(&state) == 4);

    assert(voucher_grant(&state, VOUCHER_CLEARANCE_SALE));
    assert(voucher_discount_price(&state, 5) == 4);
    assert(voucher_grant(&state, VOUCHER_LIQUIDATION));
    assert(voucher_discount_price(&state, 5) == 3);
    assert(voucher_discount_price(&state, INT_MAX) == 1073741824);

    assert(voucher_grant(&state, VOUCHER_REROLL_SURPLUS));
    assert(voucher_grant(&state, VOUCHER_REROLL_GLUT));
    assert(voucher_get_reroll_cost(&state, 5) == 1);
    assert(voucher_get_reroll_cost(&state, 2) == 1);
    assert(voucher_get_reroll_cost(&state, INT_MIN) == 1);
    assert(voucher_get_reroll_cost(&state, INT_MAX) == INT_MAX - 4);

    assert(voucher_grant(&state, VOUCHER_GRABBER));
    assert(voucher_grant(&state, VOUCHER_NACHO_TONG));
    assert(voucher_get_hands_per_blind(&state, 4) == 6);
    assert(voucher_grant(&state, VOUCHER_WASTEFUL));
    assert(voucher_grant(&state, VOUCHER_RECYCLOMANCY));
    assert(voucher_get_discards_per_blind(&state, 4) == 6);
    assert(!voucher_grant(&state, VOUCHER_ANTIMATTER));
    assert(voucher_grant(&state, VOUCHER_BLANK));
    assert(voucher_grant(&state, VOUCHER_ANTIMATTER));
    assert(voucher_get_joker_capacity(&state, 5) == 6);
    assert(voucher_get_hands_per_blind(&state, INT_MAX) == INT_MAX);
    assert(voucher_get_discards_per_blind(&state, INT_MAX) == INT_MAX);
    assert(voucher_get_joker_capacity(&state, INT_MAX) == INT_MAX);
}

static void test_state_validation(void)
{
    VoucherState state;
    voucher_state_reset(&state);
    assert(voucher_state_is_valid(&state));

    state.owned_mask = (uint16_t)(1U << VOUCHER_ANTIMATTER);
    assert(!voucher_state_is_valid(&state));

    voucher_state_reset(&state);
    state.offer_id = VOUCHER_OVERSTOCK_PLUS;
    assert(!voucher_state_is_valid(&state));

    voucher_state_reset(&state);
    state.offer_id = VOUCHER_OVERSTOCK;
    /* A real offer must belong to a concrete Ante. */
    assert(!voucher_state_is_valid(&state));

    state.offer_ante = 1;
    assert(voucher_state_is_valid(&state));

    /* Reusing an Ante must reject stale or newly-owned offers. */
    assert(voucher_grant(&state, VOUCHER_OVERSTOCK));
    assert(!voucher_prepare_offer(&state, 1, 0));

    voucher_state_reset(&state);
    assert(voucher_grant(&state, VOUCHER_OVERSTOCK));
    state.offer_ante = 1;
    state.offer_id = VOUCHER_OVERSTOCK_PLUS;
    assert(voucher_state_is_valid(&state));

    state.offer_id = VOUCHER_OVERSTOCK;
    assert(!voucher_state_is_valid(&state));

    voucher_state_reset(&state);
    state.owned_mask = (uint16_t)(VOUCHER_OWNED_MASK | (1U << VOUCHER_COUNT));
    assert(!voucher_state_is_valid(&state));
}

static void test_economy_bounds_for_every_price(void)
{
    VoucherState none;
    VoucherState clearance;
    VoucherState liquidation;
    VoucherState rerolls;
    voucher_state_reset(&none);
    voucher_state_reset(&clearance);
    voucher_state_reset(&liquidation);
    voucher_state_reset(&rerolls);
    assert(voucher_grant(&clearance, VOUCHER_CLEARANCE_SALE));
    assert(voucher_grant(&liquidation, VOUCHER_CLEARANCE_SALE));
    assert(voucher_grant(&liquidation, VOUCHER_LIQUIDATION));
    assert(voucher_grant(&rerolls, VOUCHER_REROLL_SURPLUS));
    assert(voucher_grant(&rerolls, VOUCHER_REROLL_GLUT));

    /*
     * Exhaustively cover the complete practical Shop range. Discounts must be
     * monotonic, never free, and never increase the original price.
     */
    for (int base = 1; base <= 255; base++)
    {
        int full = voucher_discount_price(&none, base);
        int sale = voucher_discount_price(&clearance, base);
        int liquidated = voucher_discount_price(&liquidation, base);
        assert(full == base);
        assert(liquidated >= 1);
        assert(liquidated <= sale);
        assert(sale <= full);
    }

    for (int base = 0; base <= 255; base++)
    {
        int cost = voucher_get_reroll_cost(&rerolls, base);
        assert(cost >= 1);
        assert(cost <= (base > 0 ? base : 1));
    }
}

int main(void)
{
    test_registry();
    test_offer_lifecycle_and_prerequisites();
    test_effects_and_limits();
    test_state_validation();
    test_economy_bounds_for_every_price();
    puts("voucher tests passed");
    return 0;
}
