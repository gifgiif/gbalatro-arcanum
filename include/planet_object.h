#ifndef PLANET_OBJECT_H
#define PLANET_OBJECT_H

#include "planet.h"
#include "sprite.h"

typedef struct
{
    enum PlanetId id;
    bool focused;
    SpriteObject* sprite_object;
} PlanetObject;

void planet_object_init(void);
PlanetObject* planet_object_new(enum PlanetId id);
void planet_object_destroy(PlanetObject** object);
void planet_object_set_focus(PlanetObject* object, bool focus);
void planet_object_set_description_scale(PlanetObject* object, bool description_scale);

#endif // PLANET_OBJECT_H
