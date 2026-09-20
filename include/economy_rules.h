#ifndef ECONOMY_RULES_H
#define ECONOMY_RULES_H

#include <stdbool.h>

/*
 * The GBA build has no Skip Tags.  A skipped Small/Big Blind therefore pays
 * one dollar more than its printed base reward, while still forfeiting the
 * remaining-hand payout, interest, and the skipped Shop.
 */
int economy_get_skip_reward(int base_blind_reward);

/* Credits a non-negative reward without overflowing the signed money field. */
int economy_credit_money(int money, int reward);

/*
 * Returns the end-of-round Interest for the money held before cash-out.
 * The parameters keep the pure rule independent from the GBA UI constants.
 */
int economy_get_interest_reward(
    int money,
    bool allows_interest,
    int interest_per_5,
    int max_interest
);

#endif // ECONOMY_RULES_H
