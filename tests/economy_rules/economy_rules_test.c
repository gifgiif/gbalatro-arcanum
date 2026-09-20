#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "economy_rules.h"

int main(void)
{
    assert(economy_get_skip_reward(3) == 4);
    assert(economy_get_skip_reward(4) == 5);
    assert(economy_get_skip_reward(0) == 0);
    assert(economy_get_skip_reward(-1) == 0);
    assert(economy_get_skip_reward(INT_MAX) == INT_MAX);

    assert(economy_credit_money(4, 4) == 8);
    assert(economy_credit_money(8, 5) == 13);
    assert(economy_credit_money(-5, 4) == 4);
    assert(economy_credit_money(7, 0) == 7);
    assert(economy_credit_money(INT_MAX - 1, 5) == INT_MAX);

    assert(economy_get_interest_reward(0, true, 1, 5) == 0);
    assert(economy_get_interest_reward(4, true, 1, 5) == 0);
    assert(economy_get_interest_reward(5, true, 1, 5) == 1);
    assert(economy_get_interest_reward(24, true, 1, 5) == 4);
    assert(economy_get_interest_reward(25, true, 1, 5) == 5);
    assert(economy_get_interest_reward(100, true, 1, 5) == 5);
    assert(economy_get_interest_reward(INT_MAX, true, 1, 5) == 5);
    assert(economy_get_interest_reward(25, false, 1, 5) == 0);
    assert(economy_get_interest_reward(25, true, 0, 5) == 0);
    assert(economy_get_interest_reward(25, true, 1, 0) == 0);

    puts("economy rules tests passed");
    return 0;
}
