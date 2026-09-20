#include "joker.h"

#include "card.h"
#include "game_variables.h"
#include "graphic_utils.h"
#include "pool.h"
#include "random.h"
#include "soundbank.h"
#include "util.h"

// Tiles and palettes
#include "card_rarity_pal_gfx.h"
#include "joker_gfx.h"

#include <maxmod.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <tonc.h>

#define JOKER_SCORE_TEXT_Y     48
#define HELD_CARD_SCORE_TEXT_Y 108
#define MAX_CARD_SCORE_STR_LEN 2
// what it was before (MAX_DEFINEABLE_JOKERS / JOKERS_PER_SPRITESHEET)
#define MAX_NUM_JOKERS_SPRITESHEETS 75

/*
 * The fixed pools must hold the largest stable scene: every owned Joker plus
 * every possible Overstock shop offer.  Make future capacity changes fail at
 * compile time instead of degrading into missing cards or a NULL allocation.
 */
_Static_assert(
    MAX_ACTIVE_JOKERS >= MAX_JOKERS_HELD_SIZE + MAX_SHOP_JOKERS,
    "Joker pool is smaller than owned + shop capacity"
);

static const unsigned int* joker_gfxTiles[] = {
#define DEF_JOKER_GFX(idx) joker_gfx##idx##Tiles,
#include "../include/def_joker_gfx_table.h"
#undef DEF_JOKER_GFX
};
static const unsigned short* joker_gfxPal[] = {
#define DEF_JOKER_GFX(idx) joker_gfx##idx##Pal,
#include "def_joker_gfx_table.h"
#undef DEF_JOKER_GFX
};

const static u8 edition_price_lut[MAX_EDITIONS] = {
    0, // BASE_EDITION
    2, // FOIL_EDITION
    3, // HOLO_EDITION
    5, // POLY_EDITION
    5, // NEGATIVE_EDITION
};

static u8 s_roll_joker_modifier(void)
{
    /*
     * Match Balatro's unmodified shop rates using tenths of a percent:
     * Negative 0.3%, Polychrome 0.3%, Holographic 1.4%, Foil 2.0%.
     */
    int roll = rng_get_u32() % 1000;
    if (roll < 3)
        return NEGATIVE_EDITION;
    if (roll < 6)
        return POLY_EDITION;
    if (roll < 20)
        return HOLO_EDITION;
    if (roll < 40)
        return FOIL_EDITION;
    return BASE_EDITION;
}

static void s_joker_tile_set_pixel(u8* tiles, int x, int y, u8 color)
{
    if (tiles == NULL || x < 0 || x >= 32 || y < 0 || y >= 32)
        return;
    int tile = (y / 8) * 4 + x / 8;
    int offset = tile * 32 + (y % 8) * 4 + (x % 8) / 2;
    if (x & 1)
        tiles[offset] = (tiles[offset] & 0x0F) | (color << 4);
    else
        tiles[offset] = (tiles[offset] & 0xF0) | color;
}

static void s_joker_stamp_edition(u8* tiles, int palette_bank, u8 modifier)
{
    if (tiles == NULL || modifier == BASE_EDITION || modifier >= MAX_EDITIONS)
        return;

    u8 bright = 1;
    u8 dark = 1;
    u8 accent = 1;
    int bright_luma = -1;
    int dark_luma = INT_MAX;
    int accent_score = -1;
    for (int i = 1; i < 16; i++)
    {
        COLOR color = pal_obj_mem[palette_bank * 16 + i];
        int red = color & 31;
        int green = (color >> 5) & 31;
        int blue = (color >> 10) & 31;
        int luma = red + green + blue;
        int chroma = max(red, max(green, blue)) - min(red, min(green, blue));
        if (luma > bright_luma)
        {
            bright_luma = luma;
            bright = i;
        }
        if (luma < dark_luma)
        {
            dark_luma = luma;
            dark = i;
        }
        /*
         * Prefer a colourful mid/high-luma palette entry for the small
         * edition sparkle. Joker sheets already own their palette, so this
         * keeps the badge cheap while avoiding a universal black box.
         */
        int score = chroma * 3 + luma;
        if (luma > 20 && score > accent_score)
        {
            accent_score = score;
            accent = i;
        }
    }
    if (bright == dark)
        dark = bright == 1 ? 2 : 1;
    if (accent == dark)
        accent = bright;

    /*
     * A contained top-right mark with no filled background. The old solid
     * darkest-colour rectangle appeared as a broken black tile on many Joker
     * palettes and obscured their art.
     */
    if (modifier == FOIL_EDITION)
    {
        /* Dark one-pixel shadow, then a bright F. */
        for (int y = 3; y <= 7; y++)
            s_joker_tile_set_pixel(tiles, 27, y, dark);
        for (int x = 27; x <= 30; x++)
            s_joker_tile_set_pixel(tiles, x, 3, dark);
        for (int x = 27; x <= 29; x++)
            s_joker_tile_set_pixel(tiles, x, 5, dark);
        for (int y = 2; y <= 6; y++)
            s_joker_tile_set_pixel(tiles, 26, y, bright);
        for (int x = 26; x <= 29; x++)
            s_joker_tile_set_pixel(tiles, x, 2, bright);
        for (int x = 26; x <= 28; x++)
            s_joker_tile_set_pixel(tiles, x, 4, bright);
    }
    else if (modifier == HOLO_EDITION)
    {
        /* Two-tone holographic sparkle, outlined only where needed. */
        s_joker_tile_set_pixel(tiles, 28, 1, dark);
        s_joker_tile_set_pixel(tiles, 28, 7, dark);
        s_joker_tile_set_pixel(tiles, 24, 4, dark);
        s_joker_tile_set_pixel(tiles, 30, 4, dark);
        for (int y = 2; y <= 6; y++)
            s_joker_tile_set_pixel(tiles, 27, y, accent);
        for (int x = 25; x <= 29; x++)
            s_joker_tile_set_pixel(tiles, x, 4, accent);
        s_joker_tile_set_pixel(tiles, 27, 2, bright);
        s_joker_tile_set_pixel(tiles, 27, 3, bright);
        s_joker_tile_set_pixel(tiles, 25, 4, bright);
        s_joker_tile_set_pixel(tiles, 26, 4, bright);
        s_joker_tile_set_pixel(tiles, 27, 4, bright);
    }
    else if (modifier == POLY_EDITION)
    {
        s_joker_tile_set_pixel(tiles, 28, 1, dark);
        s_joker_tile_set_pixel(tiles, 26, 3, dark);
        s_joker_tile_set_pixel(tiles, 30, 3, dark);
        s_joker_tile_set_pixel(tiles, 28, 7, dark);
        s_joker_tile_set_pixel(tiles, 28, 2, bright);
        s_joker_tile_set_pixel(tiles, 27, 3, accent);
        s_joker_tile_set_pixel(tiles, 29, 3, bright);
        s_joker_tile_set_pixel(tiles, 26, 4, bright);
        s_joker_tile_set_pixel(tiles, 27, 4, accent);
        s_joker_tile_set_pixel(tiles, 28, 4, bright);
        s_joker_tile_set_pixel(tiles, 29, 4, accent);
        s_joker_tile_set_pixel(tiles, 30, 4, bright);
        s_joker_tile_set_pixel(tiles, 27, 5, bright);
        s_joker_tile_set_pixel(tiles, 29, 5, accent);
        s_joker_tile_set_pixel(tiles, 28, 6, bright);
    }
    else if (modifier == NEGATIVE_EDITION)
    {
        for (int y = 3; y <= 7; y++)
        {
            s_joker_tile_set_pixel(tiles, 27, y, dark);
            s_joker_tile_set_pixel(tiles, 31, y, dark);
        }
        for (int y = 2; y <= 6; y++)
        {
            s_joker_tile_set_pixel(tiles, 26, y, bright);
            s_joker_tile_set_pixel(tiles, 30, y, bright);
        }
        s_joker_tile_set_pixel(tiles, 27, 3, bright);
        s_joker_tile_set_pixel(tiles, 28, 4, bright);
        s_joker_tile_set_pixel(tiles, 29, 5, bright);
    }
}

static int s_joker_get_spritesheet_idx(u8 joker_id);
static int s_joker_get_sprite_idx_in_sheet(u8 joker_id, int spritesheet_idx);

static void s_joker_render_object_tiles(
    const Joker* joker,
    int layer,
    int palette_bank,
    bool focused
)
{
    if (joker == NULL || layer < 0 || layer >= MAX_JOKER_OBJECTS ||
        palette_bank < 0 || palette_bank >= NUM_PALETTES)
    {
        return;
    }

    int spritesheet_idx = s_joker_get_spritesheet_idx(joker->id);
    int joker_idx = s_joker_get_sprite_idx_in_sheet(joker->id, spritesheet_idx);
    int tile_index = JOKER_TID + layer * JOKER_SPRITE_OFFSET;
    u8* tiles = (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index];
    memcpy32(
        tiles,
        &joker_gfxTiles[spritesheet_idx][joker_idx * TILE_SIZE * JOKER_SPRITE_OFFSET],
        TILE_SIZE * JOKER_SPRITE_OFFSET
    );
    s_joker_stamp_edition(tiles, palette_bank, joker->modifier);
    if (focused)
    {
        obj_tiles_add_outline_4bpp(
            tiles,
            CARD_SPRITE_SIZE,
            CARD_SPRITE_SIZE,
            obj_palette_brightest_color_index(palette_bank)
        );
    }
}

/* So for the card objects, I needed them to be properly sorted
   which is why they let you specify the layer index when creating a new card object.
   Since the cards would overlap a lot in your hand, If they weren't sorted properly, it would look
   like a mess. The joker objects are functionally identical to card objects, so they use the same
   logic. But I'm going to use a simpler approach for the joker objects since I'm lazy and sorting
   them wouldn't look good enough to warrant the effort.
*/
static bool used_layers[MAX_JOKER_OBJECTS] = {false}; // Track used layers for joker sprites
// TODO: Refactor sorting into SpriteObject?

// Maps the spritesheet index to the palette bank index allocated to it.
// Spritesheets that were not allocated are
static int joker_spritesheet_pb_map[MAX_NUM_JOKERS_SPRITESHEETS];
static int joker_pb_num_sprite_users[JOKER_LAST_PB - JOKER_BASE_PB + 1] = {0};

// See linked issue for context of maps
// https://github.com/GBALATRO/balatro-gba/issues/274#issue-3685075538

// clang-format off
// Map of Joker ID -> Spritesheet idx
static int joker_id_to_sprite_map[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1,
    2, 2,
    3, 3, 3, 3, 3,
    4, 4, 4, 4, 4,
    5, 5, 5, 5,
    6, 6, 6, 6,
    7, 7,
    8, 8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
};

// Map of Spritesheet idx -> first Joker ID in sheet
// This is used to determine the sprite index of a Joker's sprite
// within its spritesheet by substracting its ID to this starting ID.
// Notice how spritesheets with only one Joker have sequential starting IDs.
static int spritesheet_idx_to_starting_joker_id[] = {
     0, 18, 20, 22, 27, 32, 36, 40, 42, 44,
    45, 46, 47, 48, 49, 50, 51, 52
};

// Lookup table of Joker Rarity strings. Used to display at the bottom of the description screen.
static const char* joker_rarity_strings_lut[MAX_RARITIES] = {
    "Common", "Uncommon", "Rare", "Legendary"
};
// clang-format on

static int s_get_num_spritesheets(void);
static int s_joker_get_spritesheet_idx(u8 joker_id);
static int s_joker_get_sprite_idx_in_sheet(u8 joker_id, int spritesheet_idx);
static void s_joker_pb_add_sprite_user(int pb);
static void s_joker_pb_remove_sprite_user(int pb);
static int s_joker_pb_get_num_sprite_users(int joker_pb);
static int s_get_unused_joker_pb(void);
static int s_allocate_pb_if_needed(u8 joker_id);

void joker_init()
{
    // This should init once only so no need to free
    int num_spritesheets = s_get_num_spritesheets();

    for (int i = 0; i < num_spritesheets; i++)
    {
        joker_spritesheet_pb_map[i] = UNDEFINED;
    }
}

Joker* joker_new(u8 id)
{
    return joker_new_with_modifier(id, s_roll_joker_modifier());
}

Joker* joker_new_with_modifier(u8 id, u8 modifier)
{
    if (id >= get_joker_registry_size() || modifier >= MAX_EDITIONS)
        return NULL;

    Joker* joker = POOL_GET(Joker);
    if (joker == NULL)
        return NULL;
    const JokerInfo* jinfo = get_joker_registry_entry(id);
    if (jinfo == NULL || jinfo->joker_effect_func == NULL)
    {
        POOL_FREE(Joker, joker);
        return NULL;
    }

    joker->id = id;
    joker->modifier = modifier;
    joker->value = jinfo->base_value + edition_price_lut[joker->modifier];
    joker->rarity = jinfo->rarity;
    joker->scoring_state = 0;
    joker->persistent_state = 0;

    // initialize persistent Joker data if needed
    JokerEffect* joker_effect = NULL;
    jinfo->joker_effect_func(joker, NULL, JOKER_EVENT_ON_JOKER_CREATED, &joker_effect);

    return joker;
}

const char* joker_get_edition_name(u8 modifier)
{
    static const char* names[MAX_EDITIONS] = {
        "Base", "Foil", "Holographic", "Polychrome", "Negative"
    };
    return modifier < MAX_EDITIONS ? names[modifier] : "Invalid";
}

const char* joker_get_edition_effect_short(u8 modifier)
{
    static const char* effects[MAX_EDITIONS] = {
        "",
        "Foil: +50 chips",
        "Holo: +10 mult",
        "Poly: x1.5 mult",
        "Neg: +1 slot"
    };
    return modifier < MAX_EDITIONS ? effects[modifier] : "Invalid edition";
}

void joker_destroy(Joker** joker)
{
    if (joker == NULL)
        return;
    POOL_FREE(Joker, *joker);
    *joker = NULL;
}

u32 joker_get_score_effect(
    Joker* joker,
    Card* scored_card,
    enum JokerEvent joker_event,
    JokerEffect** joker_effect
)
{
    if (joker == NULL || joker_effect == NULL ||
        joker_event < JOKER_EVENT_ON_JOKER_CREATED ||
        joker_event > JOKER_EVENT_ON_BLIND_SELECTED)
        return JOKER_EFFECT_FLAG_NONE;

    const JokerInfo* jinfo = get_joker_registry_entry(joker->id);
    if (jinfo == NULL || jinfo->joker_effect_func == NULL)
        return JOKER_EFFECT_FLAG_NONE;

    return jinfo->joker_effect_func(joker, scored_card, joker_event, joker_effect);
}

const char* joker_get_rarity_string(u8 rarity)
{
    if (rarity >= MAX_RARITIES)
        return NULL;

    return joker_rarity_strings_lut[rarity];
}

u16 joker_get_rarity_color(u8 rarity, bool main_color)
{
    if (rarity >= MAX_RARITIES)
        return 0x0;

    // +1 to account for the transparency
    // odd indices are the main colors, even ones are the shadows
    return card_rarity_pal_gfxPal[1 + 2 * rarity + (main_color ? 0 : 1)];
}

int joker_get_sell_value(const Joker* joker)
{
    if (joker == NULL)
    {
        return UNDEFINED;
    }

    return joker->value / 2;
}

// JokerObject methods
JokerObject* joker_object_new(Joker* joker)
{
    if (joker == NULL)
        return NULL;

    JokerObject* joker_object = POOL_GET(JokerObject);
    if (joker_object == NULL)
        return NULL;

    int layer = UNDEFINED;
    for (int i = 0; i < MAX_JOKER_OBJECTS; i++)
    {
        if (!used_layers[i])
        {
            layer = i;
            used_layers[i] = true; // Mark this layer as used
            break;
        }
    }
    if (layer == UNDEFINED)
    {
        POOL_FREE(JokerObject, joker_object);
        return NULL;
    }

    joker_object->joker = joker;
    joker_object->sprite_object = sprite_object_new();
    if (joker_object->sprite_object == NULL)
    {
        used_layers[layer] = false;
        POOL_FREE(JokerObject, joker_object);
        return NULL;
    }

    int tile_index = JOKER_TID + layer * JOKER_SPRITE_OFFSET;

    int joker_pb = s_allocate_pb_if_needed(joker->id);
    /*
     * OBJ palette banks are a hard GBA resource.  Reusing an unrelated
     * palette when all banks are occupied made a newly-created Joker appear
     * as a black/corrupt square and also left the spritesheet map inconsistent
     * when that object was destroyed.  Fail this allocation transactionally;
     * callers can omit the offer without corrupting the active run.
     */
    if (joker_pb == UNDEFINED)
    {
        used_layers[layer] = false;
        sprite_object_destroy(&joker_object->sprite_object);
        POOL_FREE(JokerObject, joker_object);
        return NULL;
    }
    s_joker_pb_add_sprite_user(joker_pb);

    s_joker_render_object_tiles(joker, layer, joker_pb, false);

    Sprite* sprite = sprite_new(
        ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF,
        ATTR1_SIZE_32,
        tile_index,
        joker_pb,
        JOKER_STARTING_LAYER + layer
    );
    if (sprite == NULL)
    {
        s_joker_pb_remove_sprite_user(joker_pb);
        used_layers[layer] = false;
        sprite_object_destroy(&joker_object->sprite_object);
        POOL_FREE(JokerObject, joker_object);
        return NULL;
    }
    sprite_object_set_sprite(joker_object->sprite_object, sprite);

    return joker_object;
}

void joker_object_set_focus(JokerObject* joker_object, bool focus)
{
    if (joker_object == NULL || joker_object->joker == NULL ||
        joker_object->sprite_object == NULL || joker_object->sprite_object->sprite == NULL ||
        sprite_object_is_focused(joker_object->sprite_object) == focus)
    {
        return;
    }

    /* Focus is a lightweight OAM raise. Rebuilding and outlining 32x32 tiles
     * for both the old and new Joker on every cursor step stalled mmFrame(). */
    sprite_object_set_focus(joker_object->sprite_object, focus);
}

void joker_object_destroy(JokerObject** joker_object)
{
    if (joker_object == NULL || *joker_object == NULL)
        return;

    Joker* joker = (*joker_object)->joker;
    Sprite* sprite = joker_object_get_sprite(*joker_object);
    int layer = sprite_get_layer(sprite) - JOKER_STARTING_LAYER;
    int palette_bank = sprite_get_pb(sprite);
    if (layer >= 0 && layer < MAX_JOKER_OBJECTS)
        used_layers[layer] = false;
    if (palette_bank != UNDEFINED)
        s_joker_pb_remove_sprite_user(palette_bank);
    if (palette_bank != UNDEFINED &&
        s_joker_pb_get_num_sprite_users(palette_bank) == 0 && joker != NULL)
    {
        joker_spritesheet_pb_map[s_joker_get_spritesheet_idx(joker->id)] =
            UNDEFINED;
    }

    sprite_object_destroy(&(*joker_object)->sprite_object); // Destroy the sprite
    joker_destroy(&(*joker_object)->joker);                 // Destroy the joker
    POOL_FREE(JokerObject, *joker_object);
    *joker_object = NULL;
}

void joker_object_shake(JokerObject* joker_object, mm_word sound_id)
{
    if (joker_object == NULL || joker_object->sprite_object == NULL)
        return;
    sprite_object_shake(joker_object->sprite_object, sound_id);
}

void set_and_shift_text(char* str, int* cursor_pos_x, int* cursor_pos_y, int color_pb)
{
    tte_set_pos(*cursor_pos_x, *cursor_pos_y);
    tte_set_special(color_pb * TTE_SPECIAL_PB_MULT_OFFSET);
    tte_write(str);

    // + 1 For space
    const int joker_score_display_offset_px = (MAX_CARD_SCORE_STR_LEN + 1) * TTE_CHAR_SIZE;
    *cursor_pos_x += joker_score_display_offset_px;
}

bool joker_object_score(
    JokerObject* joker_object,
    CardObject* card_object,
    enum JokerEvent joker_event
)
{
    if (joker_object == NULL || joker_object->joker == NULL)
    {
        return false;
    }

    bool event_requires_card =
        joker_event == JOKER_EVENT_ON_CARD_SCORED ||
        joker_event == JOKER_EVENT_ON_CARD_SCORED_END ||
        joker_event == JOKER_EVENT_ON_CARD_HELD;
    if (event_requires_card &&
        (card_object == NULL || card_object->card == NULL ||
         card_object->sprite_object == NULL))
    {
        return false;
    }

    JokerEffect* joker_effect = NULL;
    Card* scored_card = card_object != NULL ? card_object->card : NULL;
    u32 effect_flags_ret =
        joker_get_score_effect(joker_object->joker, scored_card, joker_event, &joker_effect);

    bool edition_scores =
        joker_event == JOKER_EVENT_INDEPENDENT &&
        joker_object->joker->modifier > BASE_EDITION &&
        joker_object->joker->modifier < NEGATIVE_EDITION;
    if (effect_flags_ret == JOKER_EFFECT_FLAG_NONE && !edition_scores)
    {
        return false;
    }
    /*
     * Every non-empty effect flag set must have a payload.  Keeping this
     * invariant at the common dispatch point prevents a malformed/future
     * Joker effect from turning an otherwise harmless description or score
     * event into an invalid pointer jump.
     */
    if (effect_flags_ret != JOKER_EFFECT_FLAG_NONE && joker_effect == NULL)
    {
        return false;
    }
    if ((effect_flags_ret & JOKER_EFFECT_FLAG_MESSAGE) &&
        joker_effect->message == NULL)
    {
        effect_flags_ret &= ~JOKER_EFFECT_FLAG_MESSAGE;
    }

    u32 chips = get_chips();
    u32 mult = get_mult();
    int money = g_game_vars.money;

    if (effect_flags_ret & JOKER_EFFECT_FLAG_RETRIGGER)
    {
        set_retrigger(joker_effect->retrigger);
    }

    // joker_effect.message will have been set if the Joker had anything custom to say

    int cursorPosX = TILE_SIZE; // Offset of one tile to better center the text on the card
    int cursorPosY = 0;
    if (joker_event == JOKER_EVENT_ON_CARD_HELD)
    {
        // display the text on top of the card instead of below the Joker for Held Cards effects
        if (card_object == NULL || card_object->sprite_object == NULL ||
            scored_card == NULL)
            return false;
        cursorPosX += fx2int(card_object->sprite_object->x);
        cursorPosY = HELD_CARD_SCORE_TEXT_Y;
    }
    else
    {
        /*
         * A temporarily missing sprite must not suppress an owned Joker's
         * gameplay effect.  Use the left edge as a safe text fallback.
         */
        if (joker_object->sprite_object != NULL)
            cursorPosX += fx2int(joker_object->sprite_object->x);
        cursorPosY = JOKER_SCORE_TEXT_Y;
    }

    mm_word sfx_id = SFX_CARD_SELECT;
    if (effect_flags_ret & JOKER_EFFECT_FLAG_CHIPS)
    {
        chips = u32_protected_add(chips, joker_effect->chips);
        char score_buffer[INT_MAX_DIGITS + 2]; // For '+' and null terminator
        snprintf(score_buffer, sizeof(score_buffer), "+%lu", joker_effect->chips);
        set_and_shift_text(score_buffer, &cursorPosX, &cursorPosY, TTE_BLUE_PB);
        sfx_id = SFX_CHIPS_GENERIC; // The joker chips effect is "generic"
    }
    if (effect_flags_ret & JOKER_EFFECT_FLAG_MULT)
    {
        mult = u32_protected_add(mult, joker_effect->mult);
        char score_buffer[INT_MAX_DIGITS + 2];
        snprintf(score_buffer, sizeof(score_buffer), "+%lu", joker_effect->mult);
        set_and_shift_text(score_buffer, &cursorPosX, &cursorPosY, TTE_RED_PB);
        sfx_id = SFX_MULT;
    }
    // if xmult is zero, DO NOT multiply by it
    if (effect_flags_ret & JOKER_EFFECT_FLAG_XMULT && joker_effect->xmult > 0)
    {
        mult = u32_protected_mult(mult, joker_effect->xmult);
        char score_buffer[INT_MAX_DIGITS + 2];
        snprintf(score_buffer, sizeof(score_buffer), "X%lu", joker_effect->xmult);
        set_and_shift_text(score_buffer, &cursorPosX, &cursorPosY, TTE_RED_PB);
        sfx_id = SFX_XMULT;
    }
    if (effect_flags_ret & JOKER_EFFECT_FLAG_MONEY)
    {
        if (joker_effect->money > 0 && money > INT_MAX - joker_effect->money)
            money = INT_MAX;
        else if (joker_effect->money < 0 && money < INT_MIN - joker_effect->money)
            money = INT_MIN;
        else
            money += joker_effect->money;
        char score_buffer[INT_MAX_DIGITS + 2];
        snprintf(score_buffer, sizeof(score_buffer), "%d$", joker_effect->money);
        set_and_shift_text(score_buffer, &cursorPosX, &cursorPosY, TTE_YELLOW_PB);
        // TODO: Money sound effect
    }
    // custom message for Jokers (including retriggers where Jokers will say "Again!")
    // joker_effect->message will have been set if the Joker had anything custom to say
    if (effect_flags_ret & JOKER_EFFECT_FLAG_MESSAGE)
    {
        set_and_shift_text(joker_effect->message, &cursorPosX, &cursorPosY, TTE_WHITE_PB);
    }

    if (edition_scores)
    {
        switch (joker_object->joker->modifier)
        {
            case FOIL_EDITION:
                chips = u32_protected_add(chips, 50);
                set_and_shift_text("+50", &cursorPosX, &cursorPosY, TTE_BLUE_PB);
                sfx_id = SFX_CHIPS_GENERIC;
                break;
            case HOLO_EDITION:
                mult = u32_protected_add(mult, 10);
                set_and_shift_text("+10", &cursorPosX, &cursorPosY, TTE_RED_PB);
                sfx_id = SFX_MULT;
                break;
            case POLY_EDITION:
            {
                u64 scaled = ((u64)mult * 3U) / 2U;
                mult = scaled > UINT_MAX ? UINT_MAX : (u32)scaled;
                set_and_shift_text("X1.5", &cursorPosX, &cursorPosY, TTE_RED_PB);
                sfx_id = SFX_XMULT;
                break;
            }
            default:
                break;
        }
    }
    // this will start the Joker expire animation
    if (effect_flags_ret & JOKER_EFFECT_FLAG_EXPIRE && joker_effect->expire)
    {
        joker_object_shake(joker_object, UNDEFINED);
        bool already_expiring = false;
        ListItr itr = list_itr_create(get_expired_jokers_list());
        JokerObject* expiring = NULL;
        while ((expiring = list_itr_next(&itr)))
            if (expiring == joker_object)
            {
                already_expiring = true;
                break;
            }
        if (!already_expiring)
            list_push_back(get_expired_jokers_list(), joker_object);
    }

    // Update values
    set_chips(chips);
    set_mult(mult);
    g_game_vars.money = max(0, money);

    // Update displays
    display_chips();
    display_mult();
    display_money();

    joker_object_shake(joker_object, sfx_id);

    return true;
}

Sprite* joker_object_get_sprite(JokerObject* joker_object)
{
    if (joker_object == NULL)
        return NULL;
    return sprite_object_get_sprite(joker_object->sprite_object);
}

int joker_get_random_rarity()
{
    int joker_rarity = 0;
    int rarity_roll = rng_get_u32() % 100;
    if (rarity_roll < COMMON_JOKER_CHANCE)
    {
        joker_rarity = COMMON_JOKER;
    }
    else if (rarity_roll < COMMON_JOKER_CHANCE + UNCOMMON_JOKER_CHANCE)
    {
        joker_rarity = UNCOMMON_JOKER;
    }
    else if (rarity_roll < COMMON_JOKER_CHANCE + UNCOMMON_JOKER_CHANCE + RARE_JOKER_CHANCE)
    {
        joker_rarity = RARE_JOKER;
    }
    else if (rarity_roll < COMMON_JOKER_CHANCE + UNCOMMON_JOKER_CHANCE + RARE_JOKER_CHANCE +
                               LEGENDARY_JOKER_CHANCE)
    {
        joker_rarity = LEGENDARY_JOKER;
    }

    return joker_rarity;
}

static int s_get_num_spritesheets()
{
    return MAX_NUM_JOKERS_SPRITESHEETS;
}

static int s_joker_get_spritesheet_idx(u8 joker_id)
{
    return joker_id_to_sprite_map[joker_id];
}

static int s_joker_get_sprite_idx_in_sheet(u8 joker_id, int spritesheet_idx)
{
    return joker_id - spritesheet_idx_to_starting_joker_id[joker_id_to_sprite_map[joker_id]];
}

static void s_joker_pb_add_sprite_user(int pb)
{
    if (pb < JOKER_BASE_PB || pb > JOKER_LAST_PB)
        return;
    joker_pb_num_sprite_users[pb - JOKER_BASE_PB]++;
}

static void s_joker_pb_remove_sprite_user(int pb)
{
    if (pb < JOKER_BASE_PB || pb > JOKER_LAST_PB)
        return;
    int num_sprite_users = joker_pb_num_sprite_users[pb - JOKER_BASE_PB];
    joker_pb_num_sprite_users[pb - JOKER_BASE_PB] = max(0, num_sprite_users - 1);
}

static int s_joker_pb_get_num_sprite_users(int joker_pb)
{
    if (joker_pb < JOKER_BASE_PB || joker_pb > JOKER_LAST_PB)
        return 0;
    return joker_pb_num_sprite_users[joker_pb - JOKER_BASE_PB];
}

static int s_get_unused_joker_pb()
{
    for (int i = 0; i < NUM_ELEM_IN_ARR(joker_pb_num_sprite_users); i++)
    {
        if (joker_pb_num_sprite_users[i] == 0)
        {
            return (i + JOKER_BASE_PB);
        }
    }

    return UNDEFINED;
}

static int s_allocate_pb_if_needed(u8 joker_id)
{
    int joker_spritesheet_idx = s_joker_get_spritesheet_idx(joker_id);
    int joker_pb = joker_spritesheet_pb_map[joker_spritesheet_idx];
    if (joker_pb != UNDEFINED)
    {
        // Already allocated
        return joker_pb;
    }

    // Allocate a new palette
    joker_pb = s_get_unused_joker_pb();

    if (joker_pb == UNDEFINED)
        return UNDEFINED;

    joker_spritesheet_pb_map[joker_spritesheet_idx] = joker_pb;
    memcpy16(
        &pal_obj_mem[PAL_ROW_LEN * joker_pb],
        joker_gfxPal[joker_spritesheet_idx],
        NUM_ELEM_IN_ARR(joker_gfx0Pal)
    );

    return joker_pb;
}
