#ifndef PLANET_H
#define PLANET_H

#include "alchemy.h"

#include <stdbool.h>
#include <stdint.h>

#define PLANET_BASE_COST 6
#define PLANET_MAX_LEVEL 20

enum PlanetId
{
    PLANET_PLUTO,
    PLANET_MERCURY,
    PLANET_URANUS,
    PLANET_VENUS,
    PLANET_SATURN,
    PLANET_JUPITER,
    PLANET_EARTH,
    PLANET_MARS,
    PLANET_NEPTUNE,
    PLANET_X,
    PLANET_CERES,
    PLANET_ERIS,
    PLANET_COUNT
};

typedef struct
{
    const char* name;
    const char* hand_name;
    const char* description;
    uint8_t hand_type;
    bool secret;
} PlanetInfo;

const PlanetInfo* planet_get_info(enum PlanetId id);
bool planet_is_unlocked(enum PlanetId id, const uint16_t* hand_play_counts);
bool planet_can_apply(enum PlanetId id, const uint8_t* hand_levels);
bool planet_apply(enum PlanetId id, uint8_t* hand_levels);
int planet_get_display_level(enum PlanetId id, const uint8_t* hand_levels);

#endif // PLANET_H
