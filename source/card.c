#include "card.h"

#include "graphic_utils.h"
#include "game_variables.h"
#include "util.h"

#include <maxmod.h>
#include <stdlib.h>

// Audio
#include "pool.h"
#include "soundbank.h"

// Card Sprites and Palettes
#include "deck_big_gfx.h"
#include "deck_gfx.h"
#include "decks_face_down_gfx.h"
#include "high_contrast_deck_pal_gfx.h"

// Card sprites lookup table. First index is the suit, second index is the rank. The value is the
// tile index.
const static u16 _card_sprite_lut[NUM_SUITS][NUM_RANKS] = {
    {0,   16,  32,  48,  64,  80,  96,  112, 128, 144, 160, 176, 192},
    {208, 224, 240, 256, 272, 288, 304, 320, 336, 352, 368, 384, 400},
    {416, 432, 448, 464, 480, 496, 512, 528, 544, 560, 576, 592, 608},
    {624, 640, 656, 672, 688, 704, 720, 736, 752, 768, 784, 800, 816}
};
// Deck sprites lookup table. Index is the deck Id. The value is the tile index.
const static u16 _deck_sprite_lut[DECK_TYPE_MAX] = {0, 16, 32, 48, 64, 80};

bool high_contrast = DEFAULT_HIGH_CONTRAST;
bool more_readable = DEFAULT_MORE_READABLE;

void card_init()
{
    GRIT_CPY(&pal_obj_mem[DECK_SPRITES_PB * PAL_ROW_LEN], decks_face_down_gfxPal);
}

void set_cards_high_contrast(bool enable)
{
    high_contrast = enable;
    if (high_contrast)
    {
        GRIT_CPY(&pal_obj_mem[CARD_PB * PAL_ROW_LEN], high_contrast_deck_pal_gfxPal);
    }
    else
    {
        GRIT_CPY(&pal_obj_mem[CARD_PB * PAL_ROW_LEN], deck_gfxPal);
    }
}

void set_cards_more_readable(bool enable)
{
    more_readable = enable;
}

bool get_cards_high_contrast(void)
{
    return high_contrast;
}

bool get_cards_more_readable(void)
{
    return more_readable;
}

// Card methods
Card* card_new(u8 suit, u8 rank)
{
    if (suit >= NUM_SUITS || rank >= NUM_RANKS)
        return NULL;

    Card* card = POOL_GET(Card);
    if (card == NULL)
        return NULL;

    card->suit = suit;
    card->rank = rank;
    card->enhancement = CARD_ENHANCEMENT_NONE;
    card->edition = CARD_EDITION_NONE;
    card->seal = CARD_SEAL_NONE;
    card->alchemy_flags = 0;
    card->alchemy_original_suit = suit;
    card->boss_flags = 0;

    return card;
}

void card_destroy(Card** card)
{
    if (card == NULL)
        return;
    POOL_FREE(Card, *card);
    *card = NULL;
}

u8 card_get_value(Card* card)
{
    if (card == NULL || card->rank >= NUM_RANKS)
        return 0;

    if (card->enhancement == CARD_ENHANCEMENT_STONE)
        return 0;
    if (card->rank == JACK || card->rank == QUEEN || card->rank == KING)
    {
        return 10; // Face cards are worth 10
    }
    else if (card->rank == ACE)
    {
        return 11; // Ace is worth 11
    }
    else
    {
        return card->rank + RANK_OFFSET; // 2-10 are worth their rank + RANK_OFFSET
    }

    return 0; // Should never reach here, but just in case
}

bool card_matches_suit(const Card* card, u8 suit)
{
    if (card == NULL || suit >= NUM_SUITS ||
        card->enhancement == CARD_ENHANCEMENT_STONE)
        return false;
    return card->enhancement == CARD_ENHANCEMENT_WILD || card->suit == suit;
}

bool card_has_rank(const Card* card)
{
    return card != NULL && card->enhancement != CARD_ENHANCEMENT_STONE;
}

const char* card_get_enhancement_name(u8 enhancement)
{
    static const char* names[CARD_ENHANCEMENT_COUNT] = {
        "Base", "Bonus", "Mult", "Wild", "Glass", "Steel", "Stone", "Gold", "Lucky"
    };
    return enhancement < CARD_ENHANCEMENT_COUNT ? names[enhancement] : "Invalid";
}

const char* card_get_edition_name(u8 edition)
{
    static const char* names[CARD_EDITION_COUNT] = {
        "Base", "Foil", "Holographic", "Polychrome"
    };
    return edition < CARD_EDITION_COUNT ? names[edition] : "Invalid";
}

const char* card_get_seal_name(u8 seal)
{
    static const char* names[CARD_SEAL_COUNT] = {
        "None", "Gold Seal", "Red Seal", "Blue Seal", "Purple Seal"
    };
    return seal < CARD_SEAL_COUNT ? names[seal] : "Invalid";
}

// CardObject methods
CardObject* card_object_new(Card* card)
{
    if (card == NULL)
        return NULL;

    CardObject* card_object = POOL_GET(CardObject);
    if (card_object == NULL)
        return NULL;

    card_object->card = card;
    card_object->sprite_object = sprite_object_new();
    if (card_object->sprite_object == NULL)
    {
        POOL_FREE(CardObject, card_object);
        return NULL;
    }
    card_object->selected = false;
    card_object->focused = false;

    return card_object;
}

void card_object_destroy(CardObject** card_object)
{
    if (card_object == NULL || *card_object == NULL)
        return;
    sprite_object_destroy(&((*card_object)->sprite_object));
    POOL_FREE(CardObject, *card_object);
    *card_object = NULL;
}

void card_object_set_sprite(CardObject* card_object, int layer)
{
    if (card_object == NULL || card_object->card == NULL ||
        card_object->sprite_object == NULL || layer < 0 ||
        layer >= MAX_SPRITES - CARD_STARTING_LAYER ||
        card_object->card->suit >= NUM_SUITS ||
        card_object->card->rank >= NUM_RANKS)
        return;

    int tile_index = CARD_TID + (layer * CARD_SPRITE_OFFSET);
    if (card_object->card->boss_flags & CARD_BOSS_FACE_DOWN)
    {
        card_object_set_sprite_face_down(
            card_object,
            (enum DeckType)clamp(g_game_vars.deck, 0, DECK_TYPE_MAX - 1),
            layer
        );
        return;
    }
    const unsigned int* card_tiles = more_readable ? deck_big_gfxTiles : deck_gfxTiles;
    memcpy32(
        &tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index],
        &card_tiles[_card_sprite_lut[card_object->card->suit][card_object->card->rank] * TILE_SIZE],
        TILE_SIZE * CARD_SPRITE_OFFSET
    );

    /*
     * Every visible card already owns a private 32x32 tile block. Stamp tiny
     * coloured pips into that block instead of allocating extra OAM sprites.
     * This keeps modifiers readable on GBA/R36S and supports stacked effects.
     */
    u8 flags = card_object->card->alchemy_flags;
    int marker = 0;
    for (int bit = 0; bit < 7; bit++)
    {
        if (!(flags & (1U << bit)))
            continue;
        int x0 = 2 + marker * 4;
        int color = 3 + (bit % 5);
        for (int y = 29; y <= 30; y++)
        {
            for (int x = x0; x < x0 + 3; x++)
            {
                int tile = (y / 8) * 4 + x / 8;
                int byte = (y % 8) * 4 + (x % 8) / 2;
                u8* dst = (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX]
                                      [tile_index + tile];
                if (x & 1)
                    dst[byte] = (dst[byte] & 0x0F) | (color << 4);
                else
                    dst[byte] = (dst[byte] & 0xF0) | color;
            }
        }
        marker++;
    }
    if (flags & CARD_ALCHEMY_TEMP_COPY)
    {
        for (int y = 2; y <= 3; y++)
        {
            for (int x = 27; x <= 29; x++)
            {
                int tile = (y / 8) * 4 + x / 8;
                int byte = (y % 8) * 4 + (x % 8) / 2;
                u8* dst = (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX]
                                      [tile_index + tile];
                if (x & 1)
                    dst[byte] = (dst[byte] & 0x0F) | (8 << 4);
                else
                    dst[byte] = (dst[byte] & 0xF0) | 8;
            }
        }
    }
    if (card_object->card->enhancement > CARD_ENHANCEMENT_NONE &&
        card_object->card->enhancement < CARD_ENHANCEMENT_COUNT)
    {
        /*
         * Keep every marker inside the 4bpp palette range.  The former
         * arithmetic mapping produced colour index 16 for Lucky cards; that
         * spills into the neighbouring nibble and can corrupt the card art.
         */
        static const u8 enhancement_colors[CARD_ENHANCEMENT_COUNT] = {
            0, 9, 10, 11, 12, 13, 14, 15, 8
        };
        int color = enhancement_colors[card_object->card->enhancement];
        for (int y = 2; y <= 6; y++)
            for (int x = 2; x <= 5; x++)
                if (x == 2 || y == 2 || y == 4)
                {
                    int tile = (y / 8) * 4 + x / 8;
                    int byte = (y % 8) * 4 + (x % 8) / 2;
                    u8* dst = (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX]
                                          [tile_index + tile];
                    if (x & 1)
                        dst[byte] = (dst[byte] & 0x0F) | (color << 4);
                    else
                        dst[byte] = (dst[byte] & 0xF0) | color;
                }
    }
    if (card_object->card->edition > CARD_EDITION_NONE &&
        card_object->card->edition < CARD_EDITION_COUNT)
    {
        int color = 10 + card_object->card->edition;
        for (int x = 8; x <= 23; x += 3)
        {
            int y = 2;
            int tile = (y / 8) * 4 + x / 8;
            int byte = (y % 8) * 4 + (x % 8) / 2;
            u8* dst =
                (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index + tile];
            if (x & 1)
                dst[byte] = (dst[byte] & 0x0F) | (color << 4);
            else
                dst[byte] = (dst[byte] & 0xF0) | color;
        }
    }
    if (card_object->card->seal > CARD_SEAL_NONE &&
        card_object->card->seal < CARD_SEAL_COUNT)
    {
        static const u8 seal_colors[CARD_SEAL_COUNT] = {0, 14, 3, 6, 13};
        int color = seal_colors[card_object->card->seal];
        for (int y = 3; y <= 6; y++)
            for (int x = 27; x <= 30; x++)
            {
                int tile = (y / 8) * 4 + x / 8;
                int byte = (y % 8) * 4 + (x % 8) / 2;
                u8* dst = (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX]
                                      [tile_index + tile];
                if (x & 1)
                    dst[byte] = (dst[byte] & 0x0F) | (color << 4);
                else
                    dst[byte] = (dst[byte] & 0xF0) | color;
            }
    }
    if (card_object->card->boss_flags & CARD_BOSS_DEBUFFED)
    {
        for (int p = 7; p <= 24; p++)
        {
            int xs[2] = {p, 31 - p};
            for (int n = 0; n < 2; n++)
            {
                int x = xs[n];
                int y = p;
                int tile = (y / 8) * 4 + x / 8;
                int byte = (y % 8) * 4 + (x % 8) / 2;
                u8* dst = (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX]
                                      [tile_index + tile];
                if (x & 1)
                    dst[byte] = (dst[byte] & 0x0F) | (3 << 4);
                else
                    dst[byte] = (dst[byte] & 0xF0) | 3;
            }
        }
    }

    if (card_object->focused)
    {
        obj_tiles_add_outline_4bpp(
            (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index],
            CARD_SPRITE_SIZE,
            CARD_SPRITE_SIZE,
            obj_palette_brightest_color_index(CARD_PB)
        );
        obj_tiles_add_card_cursor_frame_4bpp(
            (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index],
            obj_palette_brightest_color_index(CARD_PB),
            obj_palette_darkest_color_index(CARD_PB)
        );
    }

    /* A focus refresh keeps the same OAM object and only replaces its tiles. */
    Sprite* existing = card_object_get_sprite(card_object);
    if (existing != NULL &&
        sprite_get_layer(existing) == layer + CARD_STARTING_LAYER)
    {
        return;
    }
    Sprite* sprite = sprite_new(
        ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF,
        ATTR1_SIZE_32,
        tile_index,
        CARD_PB,
        layer + CARD_STARTING_LAYER
    );
    sprite_object_set_sprite(card_object->sprite_object, sprite);
}

void card_object_set_sprite_face_down(CardObject* card_object, enum DeckType deck, int layer)
{
    if (card_object == NULL || card_object->sprite_object == NULL ||
        (unsigned int)deck >= DECK_TYPE_MAX || layer < 0 ||
        layer >= MAX_SPRITES - CARD_STARTING_LAYER)
        return;

    int tile_index = CARD_TID + (layer * CARD_SPRITE_OFFSET);
    memcpy32(
        &tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index],
        &decks_face_down_gfxTiles[_deck_sprite_lut[deck] * TILE_SIZE],
        TILE_SIZE * CARD_SPRITE_OFFSET
    );
    if (card_object->focused)
    {
        obj_tiles_add_outline_4bpp(
            (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index],
            CARD_SPRITE_SIZE,
            CARD_SPRITE_SIZE,
            obj_palette_brightest_color_index(DECK_SPRITES_PB)
        );
        obj_tiles_add_card_cursor_frame_4bpp(
            (u8*)&tile_mem[TILE_MEM_OBJ_CHARBLOCK0_IDX][tile_index],
            obj_palette_brightest_color_index(DECK_SPRITES_PB),
            obj_palette_darkest_color_index(DECK_SPRITES_PB)
        );
    }

    Sprite* existing = card_object_get_sprite(card_object);
    if (existing != NULL &&
        sprite_get_layer(existing) == layer + CARD_STARTING_LAYER)
    {
        return;
    }
    Sprite* sprite = sprite_new(
        ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF,
        ATTR1_SIZE_32,
        tile_index,
        DECK_SPRITES_PB,
        layer + CARD_STARTING_LAYER
    );
    sprite_object_set_sprite(card_object->sprite_object, sprite);
}

void card_object_shake(CardObject* card_object, mm_word sound_id)
{
    if (card_object == NULL || card_object->sprite_object == NULL)
        return;
    sprite_object_shake(card_object->sprite_object, sound_id);
}

void card_object_set_selected(CardObject* card_object, bool selected)
{
    if (card_object == NULL)
        return;
    card_object->selected = selected;
}

bool card_object_is_selected(CardObject* card_object)
{
    if (card_object == NULL)
        return false;
    return card_object->selected;
}

void card_object_set_focus(CardObject* card_object, bool focused)
{
    if (card_object == NULL || card_object->sprite_object == NULL ||
        card_object->focused == focused)
    {
        return;
    }

    Sprite* sprite = card_object_get_sprite(card_object);
    if (sprite == NULL)
    {
        card_object->focused = focused;
        return;
    }

    int layer = sprite_get_layer(sprite) - CARD_STARTING_LAYER;
    if (layer < 0 || layer >= MAX_CARDS_ON_SCREEN)
        return;

    card_object->focused = focused;
    card_object_set_sprite(card_object, layer);
}

Sprite* card_object_get_sprite(CardObject* card_object)
{
    if (card_object == NULL)
        return NULL;
    return sprite_object_get_sprite(card_object->sprite_object);
}
