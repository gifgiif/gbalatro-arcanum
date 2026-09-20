#ifndef ALCHEMICAL_OBJECT_H
#define ALCHEMICAL_OBJECT_H

#include "alchemy.h"
#include "sprite.h"

/*
 * Three held cards plus the single lower-right shop offer can coexist.
 */
#define ALCHEMICAL_OBJECT_SLOTS 4
#define ALCHEMICAL_OBJECT_WIDTH  32
#define ALCHEMICAL_OBJECT_HEIGHT 32

typedef struct
{
    enum AlchemicalId id;
    int slot;
    bool focused;
    SpriteObject* sprite_object;
} AlchemicalObject;

void alchemical_object_init(void);
AlchemicalObject* alchemical_object_new(enum AlchemicalId id, int slot);
void alchemical_object_destroy(AlchemicalObject** object);
void alchemical_object_set_id(AlchemicalObject* object, enum AlchemicalId id);
void alchemical_object_set_focus(AlchemicalObject* object, bool focus);
void alchemical_object_set_description_scale(
    AlchemicalObject* object,
    bool description_scale
);

#endif // ALCHEMICAL_OBJECT_H
