#include "planet.h"

#include "hand_types.h"

#include <stddef.h>

static const PlanetInfo s_planets[PLANET_COUNT] = {
    [PLANET_PLUTO] = {
        .name = "Pluto",
        .hand_name = "High Card",
        .description = "Permanently level up High Card",
        .hand_type = HIGH_CARD,
        .secret = false
    },
    [PLANET_MERCURY] = {
        .name = "Mercury",
        .hand_name = "Pair",
        .description = "Permanently level up Pair",
        .hand_type = PAIR,
        .secret = false
    },
    [PLANET_URANUS] = {
        .name = "Uranus",
        .hand_name = "Two Pair",
        .description = "Permanently level up Two Pair",
        .hand_type = TWO_PAIR,
        .secret = false
    },
    [PLANET_VENUS] = {
        .name = "Venus",
        .hand_name = "Three of a Kind",
        .description = "Permanently level up Three of a Kind",
        .hand_type = THREE_OF_A_KIND,
        .secret = false
    },
    [PLANET_SATURN] = {
        .name = "Saturn",
        .hand_name = "Straight",
        .description = "Permanently level up Straight",
        .hand_type = STRAIGHT,
        .secret = false
    },
    [PLANET_JUPITER] = {
        .name = "Jupiter",
        .hand_name = "Flush",
        .description = "Permanently level up Flush",
        .hand_type = FLUSH,
        .secret = false
    },
    [PLANET_EARTH] = {
        .name = "Earth",
        .hand_name = "Full House",
        .description = "Permanently level up Full House",
        .hand_type = FULL_HOUSE,
        .secret = false
    },
    [PLANET_MARS] = {
        .name = "Mars",
        .hand_name = "Four of a Kind",
        .description = "Permanently level up Four of a Kind",
        .hand_type = FOUR_OF_A_KIND,
        .secret = false
    },
    [PLANET_NEPTUNE] = {
        .name = "Neptune",
        .hand_name = "Straight Flush",
        .description = "Level up Straight Flush and Royal Flush",
        .hand_type = STRAIGHT_FLUSH,
        .secret = false
    },
    [PLANET_X] = {
        .name = "Planet X",
        .hand_name = "Five of a Kind",
        .description = "Permanently level up Five of a Kind",
        .hand_type = FIVE_OF_A_KIND,
        .secret = true
    },
    [PLANET_CERES] = {
        .name = "Ceres",
        .hand_name = "Flush House",
        .description = "Permanently level up Flush House",
        .hand_type = FLUSH_HOUSE,
        .secret = true
    },
    [PLANET_ERIS] = {
        .name = "Eris",
        .hand_name = "Flush Five",
        .description = "Permanently level up Flush Five",
        .hand_type = FLUSH_FIVE,
        .secret = true
    }
};

static bool planet_valid(enum PlanetId id)
{
    return (unsigned int)id < PLANET_COUNT;
}

const PlanetInfo* planet_get_info(enum PlanetId id)
{
    return planet_valid(id) ? &s_planets[id] : NULL;
}

bool planet_is_unlocked(enum PlanetId id, const uint16_t* hand_play_counts)
{
    const PlanetInfo* info = planet_get_info(id);
    if (info == NULL)
        return false;
    if (!info->secret)
        return true;
    return hand_play_counts != NULL &&
           hand_play_counts[info->hand_type] > 0;
}

bool planet_can_apply(enum PlanetId id, const uint8_t* hand_levels)
{
    const PlanetInfo* info = planet_get_info(id);
    if (info == NULL || hand_levels == NULL)
        return false;

    if (id == PLANET_NEPTUNE)
        return hand_levels[STRAIGHT_FLUSH] < PLANET_MAX_LEVEL ||
               hand_levels[ROYAL_FLUSH] < PLANET_MAX_LEVEL;
    return hand_levels[info->hand_type] < PLANET_MAX_LEVEL;
}

bool planet_apply(enum PlanetId id, uint8_t* hand_levels)
{
    const PlanetInfo* info = planet_get_info(id);
    if (info == NULL || !planet_can_apply(id, hand_levels))
        return false;

    if (id == PLANET_NEPTUNE)
    {
        if (hand_levels[STRAIGHT_FLUSH] < PLANET_MAX_LEVEL)
            hand_levels[STRAIGHT_FLUSH]++;
        if (hand_levels[ROYAL_FLUSH] < PLANET_MAX_LEVEL)
            hand_levels[ROYAL_FLUSH]++;
        return true;
    }

    hand_levels[info->hand_type]++;
    return true;
}

int planet_get_display_level(enum PlanetId id, const uint8_t* hand_levels)
{
    const PlanetInfo* info = planet_get_info(id);
    if (info == NULL || hand_levels == NULL)
        return 1;
    return hand_levels[info->hand_type] + 1;
}
