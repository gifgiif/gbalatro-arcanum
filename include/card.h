#ifndef CARD_H
#define CARD_H

#include "deck_types.h"
#include "sprite.h"

#include <maxmod.h>
#include <tonc.h>

#define MAX_CARDS           64
#define MAX_CARDS_ON_SCREEN 16

#define CARD_TID            0
#define CARD_SPRITE_OFFSET  16
#define CARD_PB             0
#define CARD_STARTING_LAYER 0

// Card suits
#define DIAMONDS  0
#define CLUBS     1
#define HEARTS    2
#define SPADES    3
#define NUM_SUITS 4

// Card ranks
#define TWO         0
#define THREE       1
#define FOUR        2
#define FIVE        3
#define SIX         4
#define SEVEN       5
#define EIGHT       6
#define NINE        7
#define TEN         8
#define JACK        9
#define QUEEN       10
#define KING        11
#define ACE         12
#define NUM_RANKS   13
#define RANK_OFFSET 2 // Because the first rank is 2 and ranks start at 0

#define IMPOSSIBLY_HIGH_CARD_VALUE 100

// Card sprites
#define DEFAULT_HIGH_CONTRAST false
#define DEFAULT_MORE_READABLE false

// Card types
typedef struct Card
{
    u8 suit;
    u8 rank;
    u8 enhancement;
    u8 edition;
    u8 seal;
    u8 alchemy_flags;
    u8 alchemy_original_suit;
    u8 boss_flags;
} Card;

enum CardEnhancement
{
    CARD_ENHANCEMENT_NONE,
    CARD_ENHANCEMENT_BONUS,
    CARD_ENHANCEMENT_MULT,
    CARD_ENHANCEMENT_WILD,
    CARD_ENHANCEMENT_GLASS,
    CARD_ENHANCEMENT_STEEL,
    CARD_ENHANCEMENT_STONE,
    CARD_ENHANCEMENT_GOLD,
    CARD_ENHANCEMENT_LUCKY,
    CARD_ENHANCEMENT_COUNT
};

enum PlayingCardEdition
{
    CARD_EDITION_NONE,
    CARD_EDITION_FOIL,
    CARD_EDITION_HOLOGRAPHIC,
    CARD_EDITION_POLYCHROME,
    CARD_EDITION_COUNT
};

enum CardSeal
{
    CARD_SEAL_NONE,
    CARD_SEAL_GOLD,
    CARD_SEAL_RED,
    CARD_SEAL_BLUE,
    CARD_SEAL_PURPLE,
    CARD_SEAL_COUNT
};

#define CARD_ALCHEMY_POLY      (1 << 0)
#define CARD_ALCHEMY_STEEL     (1 << 1)
#define CARD_ALCHEMY_GLASS     (1 << 2)
#define CARD_ALCHEMY_GOLD      (1 << 3)
#define CARD_ALCHEMY_LUCKY     (1 << 4)
#define CARD_ALCHEMY_OILED     (1 << 5)
#define CARD_ALCHEMY_BORAX     (1 << 6)
#define CARD_ALCHEMY_TEMP_COPY (1 << 7)

/* These four effects are temporary alternatives to one enhancement slot.
 * They must not stack with one another: doing so is stronger than the source
 * mechanics and can also make a consumable look applied without a new result. */
#define CARD_ALCHEMY_ENHANCEMENT_MASK                                      \
    (CARD_ALCHEMY_STEEL | CARD_ALCHEMY_GLASS | CARD_ALCHEMY_GOLD |        \
     CARD_ALCHEMY_LUCKY)

#define CARD_BOSS_PLAYED_ANTE (1 << 0)
#define CARD_BOSS_FACE_DOWN   (1 << 1)
#define CARD_BOSS_DEBUFFED    (1 << 2)
#define CARD_BOSS_FORCED      (1 << 3)
#define CARD_RUNTIME_DESTROY  (1 << 4)

typedef struct CardObject
{
    Card* card;
    SpriteObject* sprite_object;
    bool selected;
    /** True only for the card currently under the D-pad cursor. */
    bool focused;
} CardObject;

void card_init(void);

// Card sprites accessibility functions
void set_cards_high_contrast(bool enable);
void set_cards_more_readable(bool enable);
bool get_cards_high_contrast(void);
bool get_cards_more_readable(void);

// Card methods
Card* card_new(u8 suit, u8 rank);
void card_destroy(Card** card);
u8 card_get_value(Card* card);
bool card_matches_suit(const Card* card, u8 suit);
bool card_has_rank(const Card* card);
const char* card_get_enhancement_name(u8 enhancement);
const char* card_get_edition_name(u8 edition);
const char* card_get_seal_name(u8 seal);

// CardObject methods
CardObject* card_object_new(Card* card);
void card_object_destroy(CardObject** card_object);
void card_object_set_sprite(CardObject* card_object, int layer);
void card_object_set_sprite_face_down(CardObject* card_object, enum DeckType deck, int layer);
void card_object_shake(CardObject* card_object, mm_word sound_id);

void card_object_set_selected(CardObject* card_object, bool selected);
bool card_object_is_selected(CardObject* card_object);
void card_object_set_focus(CardObject* card_object, bool focused);
Sprite* card_object_get_sprite(CardObject* card_object);

#endif // CARD_H
