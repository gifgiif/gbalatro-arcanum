#include "alchemical_object.h"

#include "alchemical_focus_gfx.h"
#include "alchemical_gfx.h"
#include "card.h"
#include "joker.h"
#include "util.h"

#include <string.h>
#include <tonc.h>

#define ALCHEMICAL_PB             4
#define ALCHEMICAL_TILE_COUNT     16
#define ALCHEMICAL_TILE_STRIDE    ALCHEMICAL_TILE_COUNT
#define ALCHEMICAL_TID            (JOKER_TID + MAX_JOKER_OBJECTS * JOKER_SPRITE_OFFSET)
/*
 * OAM layout:
 *   0..15  cards held in hand
 *   16..20 cards currently being scored
 *   21..25 blind/menu sprites
 *   26..57 Jokers
 *
 * Alchemical objects coexist with played cards during a blind, so they must
 * live after the complete Joker reservation.  The previous 16..19 range
 * stole played-card sprites and made scoring animations disappear.
 */
#define ALCHEMICAL_STARTING_LAYER (JOKER_STARTING_LAYER + MAX_JOKER_OBJECTS)
_Static_assert(
    ALCHEMICAL_STARTING_LAYER + ALCHEMICAL_OBJECT_SLOTS <= MAX_SPRITES,
    "Alchemical OAM layers exceed the sprite table"
);
_Static_assert(
    ALCHEMICAL_TID + ALCHEMICAL_OBJECT_SLOTS * ALCHEMICAL_TILE_COUNT <= 1024,
    "Alchemical OBJ tiles exceed GBA character memory"
);
#define ALCHEMICAL_HELD_SCALE     float2fx(7.0f / 4.0f)
#define ALCHEMICAL_SHOP_SCALE     float2fx(5.0f / 4.0f)
#define ALCHEMICAL_DESC_SCALE     int2fx(1)

static AlchemicalObject objects[ALCHEMICAL_OBJECT_SLOTS];
static bool used_slots[ALCHEMICAL_OBJECT_SLOTS];
static bool palette_loaded = false;

static FIXED alchemical_object_default_scale(const AlchemicalObject* object)
{
    return object != NULL && object->slot >= ALCHEMICAL_HELD_LIMIT
             ? ALCHEMICAL_SHOP_SCALE
             : ALCHEMICAL_HELD_SCALE;
}

static void alchemical_object_apply_scale(AlchemicalObject* object, FIXED scale)
{
    if (object == NULL || object->sprite_object == NULL ||
        object->sprite_object->sprite == NULL)
    {
        return;
    }
    object->sprite_object->scale = scale;
    object->sprite_object->tscale = scale;
    object->sprite_object->vscale = 0;
    obj_aff_rotscale(object->sprite_object->sprite->aff, scale, scale, 0);
}

void alchemical_object_init(void)
{
    memcpy16(&pal_obj_mem[ALCHEMICAL_PB * PAL_ROW_LEN], alchemical_gfxPal, 16);
    palette_loaded = true;
}

static void copy_tiles(enum AlchemicalId id, int slot, bool focused)
{
    if ((unsigned int)id >= ALCHEMICAL_ID_COUNT ||
        slot < 0 || slot >= ALCHEMICAL_OBJECT_SLOTS)
        return;

    const unsigned int* tiles =
        focused ? alchemical_focus_gfxTiles : alchemical_gfxTiles;
    memcpy32(
        &tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][ALCHEMICAL_TID + slot * ALCHEMICAL_TILE_STRIDE],
        &tiles[id * ALCHEMICAL_TILE_COUNT * TILE_SIZE],
        ALCHEMICAL_TILE_COUNT * TILE_SIZE
    );
}

AlchemicalObject* alchemical_object_new(enum AlchemicalId id, int slot)
{
    if ((unsigned int)id >= ALCHEMICAL_ID_COUNT ||
        slot < 0 || slot >= ALCHEMICAL_OBJECT_SLOTS || used_slots[slot])
    {
        return NULL;
    }

    if (!palette_loaded)
        alchemical_object_init();

    AlchemicalObject* object = &objects[slot];
    object->id = id;
    object->slot = slot;
    object->focused = false;
    object->sprite_object = sprite_object_new();
    if (object->sprite_object == NULL)
        return NULL;

    copy_tiles(id, slot, false);
    Sprite* sprite = sprite_new(
        ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF,
        ATTR1_SIZE_32,
        ALCHEMICAL_TID + slot * ALCHEMICAL_TILE_STRIDE,
        ALCHEMICAL_PB,
        ALCHEMICAL_STARTING_LAYER + slot
    );
    if (sprite == NULL)
    {
        sprite_object_destroy(&object->sprite_object);
        return NULL;
    }

    sprite_object_set_sprite(object->sprite_object, sprite);
    /*
     * Shop offers use the same 32px footprint as Jokers. Held cards remain
     * compact so three fit in the upper-right consumable panel.
     */
    alchemical_object_apply_scale(object, alchemical_object_default_scale(object));
    used_slots[slot] = true;
    return object;
}

void alchemical_object_destroy(AlchemicalObject** object)
{
    if (object == NULL || *object == NULL)
        return;

    int slot = (*object)->slot;
    sprite_object_destroy(&(*object)->sprite_object);
    if (slot >= 0 && slot < ALCHEMICAL_OBJECT_SLOTS)
        used_slots[slot] = false;
    *object = NULL;
}

void alchemical_object_set_id(AlchemicalObject* object, enum AlchemicalId id)
{
    if (object == NULL || (unsigned int)id >= ALCHEMICAL_ID_COUNT)
        return;
    object->id = id;
    copy_tiles(id, object->slot, false);
}

void alchemical_object_set_focus(AlchemicalObject* object, bool focus)
{
    if (object == NULL || object->sprite_object == NULL || object->focused == focus)
        return;

    object->focused = focus;
    /* Keep navigation OAM-only; copying a full 32x32 focused sprite here
     * caused two VRAM transfers for every cursor movement. */
    sprite_object_set_focus(object->sprite_object, focus);
}

void alchemical_object_set_description_scale(
    AlchemicalObject* object,
    bool description_scale
)
{
    alchemical_object_apply_scale(
        object,
        description_scale ? ALCHEMICAL_DESC_SCALE
                          : alchemical_object_default_scale(object)
    );
}
