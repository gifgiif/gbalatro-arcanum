#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hand_types.h"
#include "planet.h"

static void test_registry(void)
{
    assert(PLANET_COUNT == 12);
    assert(PLANET_BASE_COST == 6);
    for (int id = 0; id < PLANET_COUNT; id++)
    {
        const PlanetInfo* info = planet_get_info(id);
        assert(info != NULL);
        assert(info->name != NULL && strlen(info->name) > 0);
        assert(info->hand_name != NULL && strlen(info->hand_name) > 0);
        assert(info->description != NULL && strlen(info->description) > 0);
        /* ';' is a control glyph in the compact GBA font, not punctuation. */
        assert(strchr(info->description, ';') == NULL);
        assert(info->hand_type > NONE && info->hand_type < ALCHEMICAL_HAND_TYPE_COUNT);
    }
    assert(planet_get_info(PLANET_COUNT) == NULL);
    assert(planet_get_info((enum PlanetId)-1) == NULL);
}

static void test_unlocks(void)
{
    uint16_t counts[ALCHEMICAL_HAND_TYPE_COUNT] = {0};
    assert(planet_is_unlocked(PLANET_PLUTO, counts));
    assert(!planet_is_unlocked(PLANET_X, counts));
    assert(!planet_is_unlocked(PLANET_CERES, counts));
    assert(!planet_is_unlocked(PLANET_ERIS, counts));

    counts[FIVE_OF_A_KIND] = 1;
    counts[FLUSH_HOUSE] = 1;
    counts[FLUSH_FIVE] = 1;
    assert(planet_is_unlocked(PLANET_X, counts));
    assert(planet_is_unlocked(PLANET_CERES, counts));
    assert(planet_is_unlocked(PLANET_ERIS, counts));
    assert(!planet_is_unlocked(PLANET_X, NULL));
}

static void test_apply_and_cap(void)
{
    uint8_t levels[ALCHEMICAL_HAND_TYPE_COUNT] = {0};
    assert(planet_get_display_level(PLANET_JUPITER, levels) == 1);
    assert(planet_can_apply(PLANET_JUPITER, levels));
    assert(planet_apply(PLANET_JUPITER, levels));
    assert(levels[FLUSH] == 1);
    assert(planet_get_display_level(PLANET_JUPITER, levels) == 2);

    levels[FLUSH] = PLANET_MAX_LEVEL;
    assert(!planet_can_apply(PLANET_JUPITER, levels));
    assert(!planet_apply(PLANET_JUPITER, levels));
    assert(levels[FLUSH] == PLANET_MAX_LEVEL);

    assert(planet_apply(PLANET_NEPTUNE, levels));
    assert(levels[STRAIGHT_FLUSH] == 1);
    assert(levels[ROYAL_FLUSH] == 1);
    levels[STRAIGHT_FLUSH] = PLANET_MAX_LEVEL;
    assert(planet_apply(PLANET_NEPTUNE, levels));
    assert(levels[STRAIGHT_FLUSH] == PLANET_MAX_LEVEL);
    assert(levels[ROYAL_FLUSH] == 2);
    levels[ROYAL_FLUSH] = PLANET_MAX_LEVEL;
    assert(!planet_apply(PLANET_NEPTUNE, levels));

    assert(!planet_apply(PLANET_COUNT, levels));
    assert(!planet_apply(PLANET_PLUTO, NULL));
}

int main(void)
{
    test_registry();
    test_unlocks();
    test_apply_and_cap();
    puts("planet tests passed");
    return 0;
}
