#include "planet_object.h"

#include "alchemical_gfx.h"
#include "alchemical_object.h"
#include "card.h"
#include "joker.h"
#include "planet_focus_gfx.h"
#include "planet_gfx.h"
#include "util.h"

#include <string.h>
#include <tonc.h>

#define PLANET_PB          4
#define PLANET_TILE_COUNT  16
#define PLANET_OFFER_SLOT  (ALCHEMICAL_OBJECT_SLOTS - 1)
#define PLANET_TID         \
    (JOKER_TID + MAX_JOKER_OBJECTS * JOKER_SPRITE_OFFSET + \
     PLANET_OFFER_SLOT * PLANET_TILE_COUNT)
#define PLANET_LAYER       \
    (JOKER_STARTING_LAYER + MAX_JOKER_OBJECTS + PLANET_OFFER_SLOT)
#define PLANET_SHOP_SCALE  float2fx(5.0f / 4.0f)
#define PLANET_DESC_SCALE  int2fx(1)

_Static_assert(PLANET_LAYER < MAX_SPRITES, "Planet OAM layer exceeds sprite table");
_Static_assert(
    PLANET_TID + PLANET_TILE_COUNT <= 1024,
    "Planet OBJ tiles exceed character memory"
);

static PlanetObject s_object;
static bool s_object_in_use = false;
static bool s_palette_loaded = false;

static void planet_object_apply_scale(PlanetObject* object, FIXED scale)
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

void planet_object_init(void)
{
    memcpy16(&pal_obj_mem[PLANET_PB * PAL_ROW_LEN], alchemical_gfxPal, 16);
    s_palette_loaded = true;
}

static void planet_object_copy_tiles(enum PlanetId id, bool focused)
{
    if ((unsigned int)id >= PLANET_COUNT)
        return;

    const unsigned int* tiles = focused ? planet_focus_gfxTiles : planet_gfxTiles;
    memcpy32(
        &tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][PLANET_TID],
        &tiles[id * PLANET_TILE_COUNT * TILE_SIZE],
        PLANET_TILE_COUNT * TILE_SIZE
    );
}

PlanetObject* planet_object_new(enum PlanetId id)
{
    if ((unsigned int)id >= PLANET_COUNT || s_object_in_use)
        return NULL;
    if (!s_palette_loaded)
        planet_object_init();

    s_object.id = id;
    s_object.focused = false;
    s_object.sprite_object = sprite_object_new();
    if (s_object.sprite_object == NULL)
        return NULL;

    planet_object_copy_tiles(id, false);
    Sprite* sprite = sprite_new(
        ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF,
        ATTR1_SIZE_32,
        PLANET_TID,
        PLANET_PB,
        PLANET_LAYER
    );
    if (sprite == NULL)
    {
        sprite_object_destroy(&s_object.sprite_object);
        return NULL;
    }

    sprite_object_set_sprite(s_object.sprite_object, sprite);
    planet_object_apply_scale(&s_object, PLANET_SHOP_SCALE);
    s_object_in_use = true;
    return &s_object;
}

void planet_object_destroy(PlanetObject** object)
{
    if (object == NULL || *object == NULL)
        return;
    sprite_object_destroy(&(*object)->sprite_object);
    s_object_in_use = false;
    *object = NULL;
}

void planet_object_set_focus(PlanetObject* object, bool focus)
{
    if (object == NULL || object->sprite_object == NULL || object->focused == focus)
        return;
    object->focused = focus;
    sprite_object_set_focus(object->sprite_object, focus);
}

void planet_object_set_description_scale(PlanetObject* object, bool description_scale)
{
    planet_object_apply_scale(
        object,
        description_scale ? PLANET_DESC_SCALE : PLANET_SHOP_SCALE
    );
}
