#include "voucher.h"

#include <limits.h>
#include <stddef.h>

static const VoucherInfo voucher_registry[VOUCHER_COUNT] = {
    [VOUCHER_OVERSTOCK] = {
        "Overstock", "+1 Joker offer in every shop", VOUCHER_BASE_COST, VOUCHER_INVALID_ID
    },
    [VOUCHER_OVERSTOCK_PLUS] = {
        "Overstock Plus", "+1 more Joker offer in every shop", VOUCHER_BASE_COST,
        VOUCHER_OVERSTOCK
    },
    [VOUCHER_CLEARANCE_SALE] = {
        "Clearance Sale", "Jokers and consumables cost 25% less", VOUCHER_BASE_COST,
        VOUCHER_INVALID_ID
    },
    [VOUCHER_LIQUIDATION] = {
        "Liquidation", "Jokers and consumables cost 50% less", VOUCHER_BASE_COST,
        VOUCHER_CLEARANCE_SALE
    },
    [VOUCHER_REROLL_SURPLUS] = {
        "Reroll Surplus", "Rerolls cost $2 less", VOUCHER_BASE_COST, VOUCHER_INVALID_ID
    },
    [VOUCHER_REROLL_GLUT] = {
        "Reroll Glut", "Rerolls cost another $2 less", VOUCHER_BASE_COST,
        VOUCHER_REROLL_SURPLUS
    },
    [VOUCHER_GRABBER] = {
        "Grabber", "+1 hand each blind", VOUCHER_BASE_COST, VOUCHER_INVALID_ID
    },
    [VOUCHER_NACHO_TONG] = {
        "Nacho Tong", "+1 more hand each blind", VOUCHER_BASE_COST, VOUCHER_GRABBER
    },
    [VOUCHER_WASTEFUL] = {
        "Wasteful", "+1 discard each blind", VOUCHER_BASE_COST, VOUCHER_INVALID_ID
    },
    [VOUCHER_RECYCLOMANCY] = {
        "Recyclomancy", "+1 more discard each blind", VOUCHER_BASE_COST, VOUCHER_WASTEFUL
    },
    [VOUCHER_BLANK] = {
        "Blank", "Does nothing and unlocks Antimatter", VOUCHER_BASE_COST, VOUCHER_INVALID_ID
    },
    [VOUCHER_ANTIMATTER] = {
        "Antimatter", "+1 Joker slot", VOUCHER_BASE_COST, VOUCHER_BLANK
    },
};

static bool voucher_id_valid(enum VoucherId id)
{
    return (unsigned int)id < VOUCHER_COUNT;
}

static int saturating_add_int(int value, int amount)
{
    if (amount > 0 && value > INT_MAX - amount)
        return INT_MAX;
    if (amount < 0 && value < INT_MIN - amount)
        return INT_MIN;
    return value + amount;
}

const VoucherInfo* voucher_get_info(enum VoucherId id)
{
    return voucher_id_valid(id) ? &voucher_registry[id] : NULL;
}

void voucher_state_reset(VoucherState* state)
{
    if (state == NULL)
        return;
    state->owned_mask = 0;
    state->offer_id = VOUCHER_INVALID_ID;
    state->offer_ante = VOUCHER_INVALID_ID;
}

bool voucher_is_owned(const VoucherState* state, enum VoucherId id)
{
    return state != NULL && voucher_id_valid(id) &&
           (state->owned_mask & (uint16_t)(1U << id)) != 0;
}

bool voucher_can_offer(const VoucherState* state, enum VoucherId id)
{
    const VoucherInfo* info = voucher_get_info(id);
    if (state == NULL || info == NULL || voucher_is_owned(state, id))
        return false;
    return info->prerequisite == VOUCHER_INVALID_ID ||
           voucher_is_owned(state, (enum VoucherId)info->prerequisite);
}

bool voucher_prepare_offer(VoucherState* state, int ante, uint32_t roll)
{
    if (state == NULL || ante < 0)
        return false;
    if (state->offer_ante == ante)
        return voucher_can_offer(state, (enum VoucherId)state->offer_id);

    state->offer_ante = ante > INT8_MAX ? INT8_MAX : (int8_t)ante;
    state->offer_id = VOUCHER_INVALID_ID;

    enum VoucherId eligible[VOUCHER_COUNT];
    int eligible_count = 0;
    for (int id = 0; id < VOUCHER_COUNT; id++)
    {
        if (voucher_can_offer(state, (enum VoucherId)id))
            eligible[eligible_count++] = (enum VoucherId)id;
    }
    if (eligible_count == 0)
        return false;

    state->offer_id = (int8_t)eligible[roll % (uint32_t)eligible_count];
    return true;
}

bool voucher_grant(VoucherState* state, enum VoucherId id)
{
    if (state == NULL || !voucher_id_valid(id) || voucher_is_owned(state, id))
        return false;
    const VoucherInfo* info = voucher_get_info(id);
    if (info->prerequisite != VOUCHER_INVALID_ID &&
        !voucher_is_owned(state, (enum VoucherId)info->prerequisite))
        return false;
    state->owned_mask |= (uint16_t)(1U << id);
    state->owned_mask &= VOUCHER_OWNED_MASK;
    return true;
}

bool voucher_state_is_valid(const VoucherState* state)
{
    if (state == NULL ||
        (state->owned_mask & (uint16_t)~VOUCHER_OWNED_MASK) != 0 ||
        state->offer_id < VOUCHER_INVALID_ID ||
        state->offer_id >= VOUCHER_COUNT ||
        state->offer_ante < VOUCHER_INVALID_ID ||
        (state->offer_id != VOUCHER_INVALID_ID &&
         state->offer_ante == VOUCHER_INVALID_ID))
        return false;

    for (int id = 0; id < VOUCHER_COUNT; id++)
    {
        const VoucherInfo* info = &voucher_registry[id];
        if (voucher_is_owned(state, (enum VoucherId)id) &&
            info->prerequisite != VOUCHER_INVALID_ID &&
            !voucher_is_owned(state, (enum VoucherId)info->prerequisite))
            return false;
    }

    if (state->offer_id != VOUCHER_INVALID_ID)
    {
        enum VoucherId offer = (enum VoucherId)state->offer_id;
        if (!voucher_can_offer(state, offer))
            return false;
    }
    return true;
}

bool voucher_buy_offer(VoucherState* state)
{
    if (state == NULL || !voucher_id_valid((enum VoucherId)state->offer_id))
        return false;
    enum VoucherId id = (enum VoucherId)state->offer_id;
    if (!voucher_grant(state, id))
        return false;
    state->offer_id = VOUCHER_INVALID_ID;
    return true;
}

int voucher_get_shop_joker_slots(const VoucherState* state)
{
    return 2 + voucher_is_owned(state, VOUCHER_OVERSTOCK) +
           voucher_is_owned(state, VOUCHER_OVERSTOCK_PLUS);
}

int voucher_get_discount_percent(const VoucherState* state)
{
    if (voucher_is_owned(state, VOUCHER_LIQUIDATION))
        return 50;
    if (voucher_is_owned(state, VOUCHER_CLEARANCE_SALE))
        return 25;
    return 0;
}

int voucher_discount_price(const VoucherState* state, int base_price)
{
    if (base_price <= 0)
        return 0;
    int percent_to_pay = 100 - voucher_get_discount_percent(state);
    return (int)(((int64_t)base_price * percent_to_pay + 99) / 100);
}

int voucher_get_reroll_cost(const VoucherState* state, int base_cost)
{
    int discount = 2 * voucher_is_owned(state, VOUCHER_REROLL_SURPLUS) +
                   2 * voucher_is_owned(state, VOUCHER_REROLL_GLUT);
    if (base_cost <= discount || base_cost <= 1)
        return 1;
    return base_cost - discount;
}

int voucher_get_hands_per_blind(const VoucherState* state, int base_hands)
{
    int bonus = voucher_is_owned(state, VOUCHER_GRABBER) +
                voucher_is_owned(state, VOUCHER_NACHO_TONG);
    return saturating_add_int(base_hands, bonus);
}

int voucher_get_discards_per_blind(const VoucherState* state, int base_discards)
{
    int bonus = voucher_is_owned(state, VOUCHER_WASTEFUL) +
                voucher_is_owned(state, VOUCHER_RECYCLOMANCY);
    return saturating_add_int(base_discards, bonus);
}

int voucher_get_joker_capacity(const VoucherState* state, int base_capacity)
{
    return saturating_add_int(
        base_capacity,
        voucher_is_owned(state, VOUCHER_ANTIMATTER)
    );
}
