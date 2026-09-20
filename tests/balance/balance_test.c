#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "alchemy.h"
#include "deck_rules.h"
#include "planet.h"
#include "voucher.h"

static uint32_t next_random(uint32_t* state)
{
    *state = *state * 1664525U + 1013904223U;
    return *state;
}

static enum AlchemicalId pick_enabled_card(uint32_t roll)
{
    int enabled = 0;
    for (int id = 0; id < ALCHEMICAL_ID_COUNT; id++)
        enabled += alchemical_is_shop_enabled((enum AlchemicalId)id);
    assert(enabled > 0);

    int wanted = (int)(roll % (uint32_t)enabled);
    for (int id = 0; id < ALCHEMICAL_ID_COUNT; id++)
        if (alchemical_is_shop_enabled((enum AlchemicalId)id) && wanted-- == 0)
            return (enum AlchemicalId)id;
    assert(false);
    return ALCHEMICAL_IGNIS;
}

static void test_every_valid_voucher_deck_economy_combination(void)
{
    for (int deck = 0; deck < DECK_TYPE_MAX; deck++)
    {
        for (uint32_t mask = 0; mask <= VOUCHER_OWNED_MASK; mask++)
        {
            VoucherState vouchers = {
                .owned_mask = (uint16_t)mask,
                .offer_id = VOUCHER_INVALID_ID,
                .offer_ante = VOUCHER_INVALID_ID
            };
            if (!voucher_state_is_valid(&vouchers))
                continue;

            int hands = voucher_get_hands_per_blind(
                &vouchers,
                deck_get_hands_per_blind((enum DeckType)deck, 4)
            );
            int discards = voucher_get_discards_per_blind(
                &vouchers,
                deck_get_discards_per_blind((enum DeckType)deck, 4)
            );
            int capacity = voucher_get_joker_capacity(
                &vouchers,
                deck_get_joker_capacity((enum DeckType)deck, 5)
            );
            assert(hands >= 1 && hands <= 7);
            assert(discards >= 4 && discards <= 7);
            assert(capacity >= 4 && capacity <= 7);
            assert(voucher_get_shop_joker_slots(&vouchers) >= 2);
            assert(voucher_get_shop_joker_slots(&vouchers) <= 4);

            for (int id = 0; id < ALCHEMICAL_ID_COUNT; id++)
            {
                const AlchemicalInfo* info = alchemical_get_info(id);
                assert(info != NULL);
                int price = voucher_discount_price(&vouchers, info->cost);
                assert(price >= 2 && price <= 6);
                assert(alchemical_get_sell_value(id) <= price);
            }
            for (int reroll = 1; reroll <= 1000; reroll++)
                assert(voucher_get_reroll_cost(&vouchers, reroll) >= 1);
        }
    }
}

static void test_hundred_thousand_blind_progression_soak(void)
{
    uint32_t rng = 0xC0DEC0DEU;
    uint8_t levels[ALCHEMICAL_HAND_TYPE_COUNT] = {0};
    int money = 4;

    for (int blind = 0; blind < 100000; blind++)
    {
        enum DeckType deck = (enum DeckType)(next_random(&rng) % DECK_TYPE_MAX);
        int hands = deck_get_hands_per_blind(deck, 4);
        int discards = deck_get_discards_per_blind(deck, 4);
        int hand_size = deck_get_hand_size(deck, 8);

        enum AlchemicalId id = pick_enabled_card(next_random(&rng));
        const AlchemicalInfo* info = alchemical_get_info(id);
        assert(info != NULL);
        if (money >= info->cost)
        {
            AlchemicalScalarState state = {
                .hands = hands,
                .discards = discards,
                .hand_size = hand_size,
                .money = money - info->cost,
                .blind_requirement = 1000000U
            };
            if (alchemical_apply_scalar(id, &state))
            {
                hands = state.hands;
                discards = state.discards;
                hand_size = state.hand_size;
                money = state.money;
            }
        }

        /* A completed blind funds the next shop through the real deck rules. */
        int cashout = deck_get_hand_cashout(deck, hands > 0 ? hands - 1 : 0, discards);
        if (cashout > 0 && money <= INT_MAX - cashout)
            money += cashout;
        assert(money >= 0);
        assert(hands >= 0);
        assert(discards >= 0);
        assert(hand_size >= 1);

        enum PlanetId planet = (enum PlanetId)(next_random(&rng) % PLANET_COUNT);
        if (money >= PLANET_BASE_COST && planet_can_apply(planet, levels))
        {
            assert(planet_apply(planet, levels));
            money -= PLANET_BASE_COST;
        }
        for (int type = 0; type < ALCHEMICAL_HAND_TYPE_COUNT; type++)
            assert(levels[type] <= PLANET_MAX_LEVEL);
    }
}

int main(void)
{
    test_every_valid_voucher_deck_economy_combination();
    test_hundred_thousand_blind_progression_soak();
    puts("balance soak passed");
    return 0;
}
