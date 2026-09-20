#include "economy_rules.h"

#include <limits.h>
#include <stdint.h>

int economy_get_skip_reward(int base_blind_reward)
{
    if (base_blind_reward <= 0)
        return 0;
    return base_blind_reward == INT_MAX ? INT_MAX : base_blind_reward + 1;
}

int economy_credit_money(int money, int reward)
{
    if (money < 0)
        money = 0;
    if (reward <= 0)
        return money;
    return money > INT_MAX - reward ? INT_MAX : money + reward;
}

int economy_get_interest_reward(
    int money,
    bool allows_interest,
    int interest_per_5,
    int max_interest
)
{
    if (!allows_interest || money < 5 || interest_per_5 <= 0 || max_interest <= 0)
        return 0;

    int64_t reward = (int64_t)(money / 5) * interest_per_5;
    if (reward > max_interest)
        reward = max_interest;
    return (int)reward;
}
