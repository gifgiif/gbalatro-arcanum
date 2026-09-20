#include "voucher_object.h"

#include "alchemical_gfx.h"
#include "alchemical_object.h"
#include "card.h"
#include "joker.h"
#include "util.h"
#include "voucher_focus_gfx.h"
#include "voucher_gfx.h"

#include <string.h>
#include <tonc.h>

#define VOUCHER_PB             4
#define VOUCHER_TILE_COUNT     16
#define VOUCHER_TID            \
    (JOKER_TID + MAX_JOKER_OBJECTS * JOKER_SPRITE_OFFSET + \
     ALCHEMICAL_OBJECT_SLOTS * VOUCHER_TILE_COUNT)
/*
 * Keep the Voucher after every Alchemical OAM slot.  Layer 20 is the fifth
 * (rightmost) played card and reusing it could suppress that card's scoring
 * sprite/animation after leaving the shop.
 */
#define VOUCHER_STARTING_LAYER \
    (JOKER_STARTING_LAYER + MAX_JOKER_OBJECTS + ALCHEMICAL_OBJECT_SLOTS)
_Static_assert(
    VOUCHER_STARTING_LAYER < MAX_SPRITES,
    "Voucher OAM layer exceeds the sprite table"
);
#define VOUCHER_SHOP_SCALE     float2fx(5.0f / 4.0f)
#define VOUCHER_DESC_SCALE     int2fx(1)
_Static_assert(
    VOUCHER_TID + VOUCHER_TILE_COUNT <= 1024,
    "Voucher OBJ tiles exceed GBA character memory"
);

static VoucherObject s_object;
static bool s_object_in_use = false;
static bool s_palette_loaded = false;

static void voucher_object_apply_scale(VoucherObject* object, FIXED scale)
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

void voucher_object_init(void)
{
    /*
     * Voucher art is quantized against the Alchemical palette. Sharing palette
     * bank 4 saves one of the scarce OBJ palettes without changing either art
     * set on hardware.
     */
    memcpy16(&pal_obj_mem[VOUCHER_PB * PAL_ROW_LEN], alchemical_gfxPal, 16);
    s_palette_loaded = true;
}

static void voucher_object_copy_tiles(enum VoucherId id, bool focused)
{
    if ((unsigned int)id >= VOUCHER_COUNT)
        return;

    const unsigned int* tiles = focused ? voucher_focus_gfxTiles : voucher_gfxTiles;
    memcpy32(
        &tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][VOUCHER_TID],
        &tiles[id * VOUCHER_TILE_COUNT * TILE_SIZE],
        VOUCHER_TILE_COUNT * TILE_SIZE
    );
}

VoucherObject* voucher_object_new(enum VoucherId id)
{
    if ((unsigned int)id >= VOUCHER_COUNT || s_object_in_use)
        return NULL;
    if (!s_palette_loaded)
        voucher_object_init();

    s_object.id = id;
    s_object.focused = false;
    s_object.sprite_object = sprite_object_new();
    if (s_object.sprite_object == NULL)
        return NULL;

    voucher_object_copy_tiles(id, false);
    Sprite* sprite = sprite_new(
        ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF,
        ATTR1_SIZE_32,
        VOUCHER_TID,
        VOUCHER_PB,
        VOUCHER_STARTING_LAYER
    );
    if (sprite == NULL)
    {
        sprite_object_destroy(&s_object.sprite_object);
        return NULL;
    }

    sprite_object_set_sprite(s_object.sprite_object, sprite);
    voucher_object_apply_scale(&s_object, VOUCHER_SHOP_SCALE);
    s_object_in_use = true;
    return &s_object;
}

void voucher_object_destroy(VoucherObject** object)
{
    if (object == NULL || *object == NULL)
        return;
    sprite_object_destroy(&(*object)->sprite_object);
    s_object_in_use = false;
    *object = NULL;
}

void voucher_object_set_focus(VoucherObject* object, bool focus)
{
    if (object == NULL || object->sprite_object == NULL || object->focused == focus)
        return;
    object->focused = focus;
    sprite_object_set_focus(object->sprite_object, focus);
}

void voucher_object_set_description_scale(VoucherObject* object, bool description_scale)
{
    voucher_object_apply_scale(
        object,
        description_scale ? VOUCHER_DESC_SCALE : VOUCHER_SHOP_SCALE
    );
}
