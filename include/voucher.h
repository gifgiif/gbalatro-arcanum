#ifndef VOUCHER_H
#define VOUCHER_H

#include <stdbool.h>
#include <stdint.h>

#define VOUCHER_COUNT        12
#define VOUCHER_INVALID_ID   (-1)
#define VOUCHER_BASE_COST    10
#define VOUCHER_OWNED_MASK   ((1U << VOUCHER_COUNT) - 1U)

enum VoucherId
{
    VOUCHER_OVERSTOCK,
    VOUCHER_OVERSTOCK_PLUS,
    VOUCHER_CLEARANCE_SALE,
    VOUCHER_LIQUIDATION,
    VOUCHER_REROLL_SURPLUS,
    VOUCHER_REROLL_GLUT,
    VOUCHER_GRABBER,
    VOUCHER_NACHO_TONG,
    VOUCHER_WASTEFUL,
    VOUCHER_RECYCLOMANCY,
    VOUCHER_BLANK,
    VOUCHER_ANTIMATTER,
};

typedef struct
{
    const char* name;
    const char* description;
    uint8_t cost;
    int8_t prerequisite;
} VoucherInfo;

typedef struct
{
    uint16_t owned_mask;
    int8_t offer_id;
    int8_t offer_ante;
} VoucherState;

const VoucherInfo* voucher_get_info(enum VoucherId id);
void voucher_state_reset(VoucherState* state);
bool voucher_is_owned(const VoucherState* state, enum VoucherId id);
bool voucher_can_offer(const VoucherState* state, enum VoucherId id);
bool voucher_prepare_offer(VoucherState* state, int ante, uint32_t roll);
bool voucher_buy_offer(VoucherState* state);
bool voucher_grant(VoucherState* state, enum VoucherId id);
bool voucher_state_is_valid(const VoucherState* state);

int voucher_get_shop_joker_slots(const VoucherState* state);
int voucher_get_discount_percent(const VoucherState* state);
int voucher_discount_price(const VoucherState* state, int base_price);
int voucher_get_reroll_cost(const VoucherState* state, int base_cost);
int voucher_get_hands_per_blind(const VoucherState* state, int base_hands);
int voucher_get_discards_per_blind(const VoucherState* state, int base_discards);
int voucher_get_joker_capacity(const VoucherState* state, int base_capacity);

#endif // VOUCHER_H
