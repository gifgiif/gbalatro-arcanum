#include "game.h"

#include "affine_background.h"
#include "affine_background_gfx.h"
#include "alchemical_object.h"
#include "alchemy.h"
#include "audio_utils.h"
#include "background_gfx.h"
#include "background_main_menu_gfx.h"
#include "button.h"
#include "boss_rules.h"
#include "card.h"
#include "debug.h"
#include "game/blind_select.h"
#include "game/common_ui.h"
#include "game/game_over.h"
#include "game/joker_row.h"
#include "game/main_menu.h"
#include "game/options_menu.h"
#include "game/round_end.h"
#include "game/run_setup.h"
#include "game/shop.h"
#include "game_variables.h"
#include "graphic_utils.h"
#include "hand.h"
#include "joker.h"
#include "layout.h"
#include "list.h"
#include "random.h"
#include "save.h"
#include "selection_grid.h"
#include "soundbank.h"
#include "splash_screen.h"
#include "sprite.h"
#include "state_machine.h"
#include "timer.h"
#include "tonc_memdef.h"
#include "util.h"

#include <maxmod.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STRAIGHT_AND_FLUSH_SIZE_FOUR_FINGERS 4
#define STRAIGHT_AND_FLUSH_SIZE_DEFAULT      5

// Pixel sizes
#define SCORED_CARD_TEXT_Y 48

// SE sizes

#define PITCH_STEP_DISCARD_SFX   (-64)
#define PITCH_STEP_DRAW_SFX      24
#define PITCH_STEP_UNDISCARD_SFX 2 * PITCH_STEP_DRAW_SFX

#define STARTING_ROUND 0
#define STARTING_ANTE  1
#define STARTING_MONEY 4
#define STARTING_SCORE 0

#define CARD_FOCUSED_UNSEL_Y 10
#define CARD_UNFOCUSED_SEL_Y 15
#define CARD_FOCUSED_SEL_Y   20

// TODO: Rename "PID" to "PAL_IDX"
// Palette IDs

#define BLIND_BG_SHADOW_PAL_IDX     5
#define BLIND_BG_SECONDARY_PAL_IDX  18
#define BLIND_BG_PRIMARY_PAL_IDX    19
#define REWARD_PANEL_BORDER_PAL_IDX 19

#define PLAY_HAND_BTN_PAL_IDX           6
#define PLAY_HAND_BTN_BORDER_PAL_IDX    7
#define DISCARD_BTN_PAL_IDX             13
#define DISCARD_BTN_BORDER_PAL_IDX      8
#define SORT_BTNS_PAL_IDX               9
#define SORT_BY_RANK_BTN_BORDER_PAL_IDX 22
#define SORT_BY_SUIT_BTN_BORDER_PAL_IDX 23

// Naming the stage where cards return from the discard pile to the deck "undiscard"

/* This needs to stay a power of 2 and small enough
 * for the lerping to be done before the next hand is drawn.
 */
#define NUM_SCORE_LERP_STEPS   16
#define TM_SCORE_LERP_INTERVAL 2

#define GAME_PLAYING_HAND_SEL_Y      1
#define GAME_PLAYING_BUTTONS_SEL_Y   2
#define GAME_PLAYING_NUM_BOTTOM_BTNS 2

#define EXPIRE_ANIMATION_FRAME_COUNT 3
#define ALCHEMY_FEEDBACK_FRAMES      90

// These functions need to be forward declared
// so they're visible to the state_info array,
// and the sub-state function tables.
// This could be done, and maybe should be done,
// with an X macro, but I'll leave that to the
// reviewer(s).
static void game_round_on_init(void);
static void game_playing_on_update(void);

static void display_temp_score(u32 value);
static void check_flaming_score(void);
static int deck_get_size(void);
static int deck_get_max_size(void);
static bool check_and_score_joker_for_event(
    ListItr* starting_joker_itr,
    CardObject* card_object,
    enum JokerEvent joker_event
);

static void game_playing_discard_on_pressed(void);
static void game_playing_execute_discard(void);
static void game_playing_play_hand_on_pressed(void);
static void game_playing_execute_play_hand(void);
static void game_playing_sort_by_rank_on_pressed(void);
static void game_playing_sort_by_suit_on_pressed(void);

static int game_playing_button_row_get_size(void);
static bool game_playing_button_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);
static void game_playing_button_row_on_key_hit(SelectionGrid* selection_grid, Selection* selection);

static void game_playing_hand_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
);

static bool game_playing_hand_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);

static int game_playing_hand_row_get_size(void);

static int hand_sel_idx_to_card_idx(int selection_index);
static bool can_discard_hand(void);
static bool can_play_hand(void);
static void game_alchemy_sync_round_objects(void);
static void game_alchemy_end_blind(void);
static bool game_alchemy_use_selected(void);
static int game_playing_top_row_get_size(void);
static bool game_playing_top_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);
static void game_playing_top_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
);

// Consts

// clang-format off
// Rects                                       left     top     right   bottom

// The rect for popping menu animations (round end, shop, blinds) 
// - extends beyond the visible screen to the end of the screenblock
// It includes both the target and source position rects. 
// This is because when popping, the target position is blank so we just animate 
// the whole rect so we don't have to track its position

static const Rect HAND_BG_RECT_SELECTING    = {9,       11,     24,     17 };

/* Contains the shop icon/current blind etc. 
 * The difference between TOP_LEFT_PANEL_ANIM_RECT and TOP_LEFT_PANEL_RECT 
 * is due to an overlap between the bottom of the top left panel
 * and the top of the score panel in the tiles connecting them.
 * TOP_LEFT_PANEL_ANIM_RECT should be used for animations, 
 * TOP_LEFT_PANEL_RECT for copies etc. but mind the overlap
 */
static const BG_POINT TOP_LEFT_BLIND_TITLE_POINT = {0,  21, };
static const Rect BIG_BLIND_TITLE_SRC_RECT  = {0,       26,     8,      26 };
static const Rect BOSS_BLIND_TITLE_SRC_RECT = {0,       27,     8,      27 };

// Flaming score animation frames
#define SCORE_FLAMES_ANIM_FREQ  5 // animation will run at 12FPS
#define NUM_SCORE_FLAMES_FRAMES 8 // Chips and Mult flame frames are next to one another
#define SCORE_FLAME_FRAME_WIDTH 3 // so we only need to offset to get the next ones
static const Rect SCORE_FLAME_RESET         = {26,      20,      28,     20};
static const Rect SCORE_FLAME_FRAMES_START  = {26,      21,      28,     21};
static const BG_POINT SCORE_FLAME_CHIPS_POS = {1,       9};
static const BG_POINT SCORE_FLAME_MULT_POS  = {5,       9};

// Rects for TTE (in pixels)
static const Rect HAND_SIZE_RECT_SELECT     = {120,     128,    160,    136 };
static const Rect HAND_SIZE_RECT_PLAYING    = {120,     152,    160,    160 };
// Score displayed in the same place as the hand type
static const Rect TEMP_SCORE_RECT           = {8,       64,     64,     72  }; 
static const Rect SCORE_RECT                = {24,      48,     64,     56  };

static const Rect PLAYED_CARDS_SCORES_RECT  = {72,      48,     240,    56  };
static const Rect HELD_CARDS_SCORES_RECT    = {72,      108,    240,    116 };
static const Rect MONEY_TEXT_RECT           = {8,       120,    64,     128 };
static const Rect CHIPS_TEXT_RECT           = {8,       80,     32,     88  };
static const Rect MULT_TEXT_RECT            = {40,      80,     64,     88  };

static const Rect HANDS_TEXT_RECT           = {16,      104,    32,        112       };
static const Rect DISCARDS_TEXT_RECT        = {48,      104,    64,        112       };
static const Rect DECK_SIZE_RECT            = {200,     152,    240,       160       };
static const Rect ROUND_TEXT_RECT           = {48,      144,    64,        152       };
static const Rect ANTE_TEXT_RECT            = {8,       144,    40,        152       };
/*
 * TTE erase rectangles use an exclusive bottom edge.  Include the complete
 * final 8-pixel text row so old description/feedback glyphs cannot survive.
 */
static const Rect ALCHEMY_DETAIL_RECT       = {72,      48,     240,       72        };
static const Rect ALCHEMY_FEEDBACK_RECT     = {72,      72,     240,       80        };

static const BG_POINT CARD_DRAW_POS         = {208,     118};
static const BG_POINT CARD_DISCARD_PNT      = {240,     70};
static const BG_POINT HAND_START_POS        = {120,     90};
static const BG_POINT HAND_PLAY_POS         = {120,     70};
// clang-format on

// NOTE: This is going to be removed in favor of the background
// variable and handling in common.c once the related refactor is finished
static enum BackgroundId background_legacy = BG_NONE;

static StateInfo state_info[] = {
#define DEF_STATE_INFO(stateEnum, init_fn, update_fn, exit_fn) \
    {.on_init = init_fn, .on_update = update_fn, .on_exit = exit_fn},
#include "../include/def_state_info_table.h"
#undef DEF_STATE_INFO
};

static StateMachine game_sm = STATE_MACHINE_DEFINE(state_info, GAME_STATE_MAX);

// clang-format off
static SelectionGridRow game_playing_selection_rows[] = {
    {
        0,
        game_playing_top_row_get_size,
        game_playing_top_row_on_selection_changed,
        game_playing_top_row_on_key_transit,
        {.wrap = false}
    },
    {
        1,
        game_playing_hand_row_get_size,
        game_playing_hand_row_on_selection_changed,
        game_playing_hand_row_on_key_transit,
        {.wrap = true}
    },
    {
        2,
        game_playing_button_row_get_size,
        game_playing_button_row_on_selection_changed,
        game_playing_button_row_on_key_hit,
        {.wrap = false}
    }
};
// clang-format on

static const Selection GAME_PLAYING_INIT_SEL = {0, 1};

static SelectionGrid game_playing_selection_grid = {
    game_playing_selection_rows,
    NUM_ELEM_IN_ARR(game_playing_selection_rows),
    GAME_PLAYING_INIT_SEL
};

// Array of buttons by horizontal selection index (x)
static Button game_playing_buttons[] = {
    {PLAY_HAND_BTN_BORDER_PAL_IDX,    PLAY_HAND_BTN_PAL_IDX, game_playing_play_hand_on_pressed,    can_play_hand   },
    {SORT_BY_RANK_BTN_BORDER_PAL_IDX, SORT_BTNS_PAL_IDX,     game_playing_sort_by_rank_on_pressed, NULL            },
    {SORT_BY_SUIT_BTN_BORDER_PAL_IDX, SORT_BTNS_PAL_IDX,     game_playing_sort_by_suit_on_pressed, NULL            },
    {DISCARD_BTN_BORDER_PAL_IDX,      DISCARD_BTN_PAL_IDX,   game_playing_discard_on_pressed,      can_discard_hand},
};

// This is a stupid way to do this but I don't care
static const int HAND_SPACING_LUT[MAX_HAND_SIZE] =
    {28, 28, 28, 28, 27, 21, 18, 15, 13, 12, 10, 9, 9, 8, 8, 7};

static enum PlayState play_state = PLAY_STARTING;

// Initialization of the global vars
// clang-format off
GameVariables g_game_vars = {
    .timer = 0, .rng_info = {0, 0},

    .round = 0, .ante = 0, .money = 0, .hand_size = DEFAULT_HAND_SIZE,
    .deck = DECK_TYPE_RED,

    .current_blind = BLIND_TYPE_SMALL,
    .next_boss_blind = BLIND_TYPE_BOSS,
    .blinds_states =
    {
        BLIND_STATE_CURRENT,
        BLIND_STATE_UPCOMING,
        BLIND_STATE_UPCOMING
    },

    .hands = 0,
    .discards = 0,
    .score = 0,

    .playing_blind_token = NULL,
    .round_end_blind_token = NULL,
    .alchemy = {
        .held = {
            ALCHEMICAL_INVALID_ID,
            ALCHEMICAL_INVALID_ID,
            ALCHEMICAL_INVALID_ID
        },
        .count = 0
    },

    .game_speed = DEFAULT_GAME_SPEED,
    .music_volume = DEFAULT_MUSIC_VOLUME,
    .sound_volume = DEFAULT_SOUND_VOLUME,
};
// clang-format on

static u32 temp_score = 0; // This is the score that shows in the same spot as the hand type.
static bool score_flames_active = false;
/*
 * Score values can use the complete u32 range. Keeping the animation in TONC
 * 24.8 fixed point overflowed once either value exceeded 0x7fffff, which made
 * the score count backwards, disappear, or never finish on high-scoring runs.
 * Store only a bounded interpolation progress and calculate displays in u64.
 */
static u32 score_lerp_progress = 0;

static u32 chips = 0;
static u32 mult = 0;
static bool retrigger = false;

static int cards_drawn = 0;

// Keeping track of cards scored
static int scored_card_index = 0;

// discarded cards specific
static bool sound_played = false;
static bool discarded_card = false;

// Playing-card drag/select state. Kept with the other per-blind transients so
// game_init() can reset it before a new run starts.
static bool moving_card = false;
static bool card_moved_too_fast = false;
static bool card_selected_instead_of_moved = false;
static const int card_swap_time_threshold = 6;
static int selection_hit_timer = UNDEFINED;

// Keeping track of what Jokers are scored at each step
static ListItr _joker_scored_itr;
static ListItr _joker_card_scored_end_itr;
static ListItr _joker_round_end_itr;

static List _owned_jokers_list;
static List _discarded_jokers_list;
static List _expired_jokers_list;

// Stacks
static CardObject* played[MAX_SELECTION_SIZE] = {NULL};
static int played_top = -1;

EWRAM_DATA static Card* deck[MAX_DECK_SIZE] = {NULL};
static int deck_top = -1;

EWRAM_DATA static Card* discard_pile[MAX_DECK_SIZE] = {NULL};
static int discard_top = -1;

static AlchemicalObject* round_alchemicals[ALCHEMICAL_HELD_LIMIT] = {NULL};
static int alchemy_selected_slot = 0;
/* Applying an item can destroy its focused sprite.  Queue it until the
 * SelectionGrid callback has returned so that callback-owned pointers remain
 * valid for the whole navigation transaction. */
static int alchemy_pending_use_slot = UNDEFINED;
static int alchemy_feedback_timer = 0;
static bool alchemy_focus = false;
static char alchemy_feedback[24] = {0};
static int alchemy_temporary_hand_size = 0;
static u32 alchemy_blind_requirement = 0;
static JokerObject* alchemy_antimony_jokers[ALCHEMICAL_HELD_LIMIT] = {NULL};
static int alchemy_antimony_retriggers = 0;
static JokerObject* alchemy_debuffed_jokers[ALCHEMICAL_HELD_LIMIT] = {NULL};
static int alchemy_debuffed_joker_count = 0;
#define ALCHEMY_ACID_CAPACITY MAX_CARDS
/*
 * Acid can temporarily own any live Card, including Wax copies. Size this from
 * the actual Card pool rather than from one particular deck container.
 */
EWRAM_DATA static Card* alchemy_acid_cards[ALCHEMY_ACID_CAPACITY] = {NULL};
static int alchemy_acid_count = 0;
typedef struct AlchemyUraniumBackup
{
    Card* card;
    u8 enhancement;
    u8 edition;
    u8 seal;
} AlchemyUraniumBackup;
EWRAM_DATA static AlchemyUraniumBackup alchemy_uranium_backups[MAX_DECK_SIZE] = {0};
static int alchemy_uranium_backup_count = 0;
static bool alchemy_steel_scored[MAX_HAND_SIZE] = {false};
static bool red_seal_retriggered[MAX_SELECTION_SIZE] = {false};
static bool red_seal_held_retriggered[MAX_HAND_SIZE] = {false};
static int round_end_gold_cards = 0;
static int round_end_blue_seals = 0;
static int round_end_alchemy_gold_cards = 0;
static enum HandType last_played_hand_type = NONE;
static char boss_card_reward[32] = {0};

/* Per-blind Boss state. Persistent hand frequencies live in GameVariables. */
/* The Eye stores one bit per playable hand type in this mask. */
_Static_assert(FLUSH_FIVE < 16, "Boss hand-history mask is too small");
static u16 boss_hand_types_played = 0;
static enum HandType boss_locked_hand_type = NONE;
static enum HandType boss_ox_hand_type = NONE;
static int boss_draw_count = UNDEFINED;
static int boss_hand_size_penalty = 0;
static bool boss_verdant_active = false;
static bool boss_house_initial_draw = false;
/* The Fish affects only the refill caused by a played hand, not later discards. */
static bool boss_fish_next_draw_face_down = false;
static bool boss_acorn_hidden = false;
static JokerObject* boss_disabled_joker = NULL;

static void game_alchemy_set_feedback(const char* message);

static bool game_alchemy_backup_uranium_card(Card* card)
{
    if (card == NULL)
        return false;
    for (int i = 0; i < alchemy_uranium_backup_count; i++)
        if (alchemy_uranium_backups[i].card == card)
            return true;
    if (alchemy_uranium_backup_count >= MAX_DECK_SIZE)
        return false;
    alchemy_uranium_backups[alchemy_uranium_backup_count++] =
        (AlchemyUraniumBackup){
            .card = card,
            .enhancement = card->enhancement,
            .edition = card->edition,
            .seal = card->seal
        };
    return true;
}

static void game_alchemy_remove_uranium_backup(Card* card, bool restore)
{
    if (card == NULL)
        return;
    for (int i = 0; i < alchemy_uranium_backup_count; i++)
    {
        if (alchemy_uranium_backups[i].card != card)
            continue;
        if (restore)
        {
            card->enhancement = alchemy_uranium_backups[i].enhancement;
            card->edition = alchemy_uranium_backups[i].edition;
            card->seal = alchemy_uranium_backups[i].seal;
        }
        for (int j = i; j < alchemy_uranium_backup_count - 1; j++)
            alchemy_uranium_backups[j] = alchemy_uranium_backups[j + 1];
        alchemy_uranium_backups[--alchemy_uranium_backup_count] =
            (AlchemyUraniumBackup){0};
        return;
    }
}

const char* game_get_boss_card_reward(void)
{
    return boss_card_reward;
}

static void game_card_modifier_on_discard(Card* card)
{
    if (card == NULL || card->seal != CARD_SEAL_PURPLE ||
        (card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY) != 0 ||
        (card->boss_flags & CARD_BOSS_DEBUFFED) != 0 ||
        g_game_vars.alchemy.count >= ALCHEMICAL_HELD_LIMIT)
        return;
    int start = rng_get_u32() % ALCHEMICAL_ID_COUNT;
    for (int offset = 0; offset < ALCHEMICAL_ID_COUNT; offset++)
    {
        enum AlchemicalId id = (start + offset) % ALCHEMICAL_ID_COUNT;
        if (!alchemical_is_shop_enabled(id))
            continue;
        if (alchemical_inventory_add(&g_game_vars.alchemy, id))
        {
            game_alchemy_sync_round_objects();
            game_alchemy_set_feedback("Purple: Alchemy gained");
        }
        return;
    }
}

static void game_capture_round_end_card_modifiers(void)
{
    round_end_gold_cards = 0;
    round_end_blue_seals = 0;
    round_end_alchemy_gold_cards = 0;
    if (g_game_vars.score < alchemy_blind_requirement)
        return;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
    {
        if (hand[i] == NULL || hand[i]->card == NULL)
            continue;
        Card* card = hand[i]->card;
        if ((card->boss_flags & CARD_BOSS_DEBUFFED) != 0 ||
            (card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY) != 0)
            continue;
        int held_triggers = card->seal == CARD_SEAL_RED ? 2 : 1;
        if (card->enhancement == CARD_ENHANCEMENT_GOLD)
            round_end_gold_cards += held_triggers;
        if (card->seal == CARD_SEAL_BLUE)
            round_end_blue_seals += held_triggers;
        if (card->alchemy_flags & CARD_ALCHEMY_GOLD)
            round_end_alchemy_gold_cards += held_triggers;
    }
}

static bool boss_blind_active(enum BlindType type)
{
    return g_game_vars.current_blind == type;
}

static bool boss_card_is_debuffed(const Card* card)
{
    if (card == NULL)
        return false;
    if (card->alchemy_flags & CARD_ALCHEMY_OILED)
        return false;
    switch (g_game_vars.current_blind)
    {
        case BLIND_TYPE_CLUB:
            return card_matches_suit(card, CLUBS);
        case BLIND_TYPE_GOAD:
            return card_matches_suit(card, SPADES);
        case BLIND_TYPE_WINDOW:
            return card_matches_suit(card, DIAMONDS);
        case BLIND_TYPE_HEAD:
            return card_matches_suit(card, HEARTS);
        case BLIND_TYPE_PLANT:
            return card_is_face(card);
        case BLIND_TYPE_PILLAR:
            return (card->boss_flags & CARD_BOSS_PLAYED_ANTE) != 0;
        case BLIND_TYPE_LEAF:
            return boss_verdant_active;
        default:
            return false;
    }
}

static void boss_refresh_card(Card* card)
{
    if (card == NULL)
        return;
    if (boss_card_is_debuffed(card))
        card->boss_flags |= CARD_BOSS_DEBUFFED;
    else
        card->boss_flags &= (u8)~CARD_BOSS_DEBUFFED;
}

static void boss_refresh_hand(void)
{
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL)
            boss_refresh_card(hand[i]->card);
    if (get_hand_top() >= 0)
        reorder_card_sprites_layers();
}

static bool game_owned_jokers_contains(const JokerObject* target)
{
    if (target == NULL)
        return false;
    ListItr itr = list_itr_create(&_owned_jokers_list);
    JokerObject* joker = NULL;
    while ((joker = list_itr_next(&itr)))
        if (joker == target)
            return true;
    return false;
}

bool game_is_joker_disabled(const JokerObject* joker)
{
    if (joker == NULL)
        return true;
    if (joker == boss_disabled_joker)
        return true;
    for (int i = 0; i < alchemy_debuffed_joker_count; i++)
        if (alchemy_debuffed_jokers[i] == joker)
            return true;
    return false;
}

static bool game_has_active_joker(int joker_id)
{
    ListItr itr = list_itr_create(&_owned_jokers_list);
    JokerObject* joker = NULL;
    while ((joker = list_itr_next(&itr)))
        if (joker->joker != NULL && joker->joker->id == joker_id &&
            !game_is_joker_disabled(joker))
            return true;
    return false;
}

static void game_notify_joker_removed(bool counts_as_sale)
{
    /*
     * Joker objects leave the owned list before their discard animation and
     * are then returned to the fixed pool. Never retain a temporary-effect
     * pointer that a later Joker allocation could reuse.
     */
    int kept_antimony = 0;
    for (int i = 0; i < alchemy_antimony_retriggers; i++)
        if (game_owned_jokers_contains(alchemy_antimony_jokers[i]))
            alchemy_antimony_jokers[kept_antimony++] = alchemy_antimony_jokers[i];
    while (kept_antimony < alchemy_antimony_retriggers)
        alchemy_antimony_jokers[kept_antimony++] = NULL;
    alchemy_antimony_retriggers = 0;
    while (alchemy_antimony_retriggers < ALCHEMICAL_HELD_LIMIT &&
           alchemy_antimony_jokers[alchemy_antimony_retriggers] != NULL)
        alchemy_antimony_retriggers++;
    if (!game_owned_jokers_contains(boss_disabled_joker))
        boss_disabled_joker = NULL;

    int kept = 0;
    for (int i = 0; i < alchemy_debuffed_joker_count; i++)
        if (game_owned_jokers_contains(alchemy_debuffed_jokers[i]))
            alchemy_debuffed_jokers[kept++] = alchemy_debuffed_jokers[i];
    while (kept < alchemy_debuffed_joker_count)
        alchemy_debuffed_jokers[kept++] = NULL;
    alchemy_debuffed_joker_count = 0;
    while (alchemy_debuffed_joker_count < ALCHEMICAL_HELD_LIMIT &&
           alchemy_debuffed_jokers[alchemy_debuffed_joker_count] != NULL)
        alchemy_debuffed_joker_count++;

    if (counts_as_sale && boss_blind_active(BLIND_TYPE_LEAF) &&
        boss_verdant_active)
    {
        boss_verdant_active = false;
        boss_refresh_hand();
    }
    else if (boss_blind_active(BLIND_TYPE_PLANT))
    {
        /*
         * Pareidolia changes which cards The Plant debuffs. Selling it must
         * immediately recalculate every visible card instead of leaving stale
         * debuff flags until the next draw.
         */
        boss_refresh_hand();
    }
}

void game_notify_joker_sold(void)
{
    game_notify_joker_removed(true);
}

static void boss_choose_disabled_joker(void)
{
    boss_disabled_joker = NULL;
    if (!boss_blind_active(BLIND_TYPE_HEART))
        return;

    /*
     * Crimson Heart must disable a Joker that is currently active.  Choosing
     * an Antimony/Brimstone-debuffed Joker made the boss hand silently do
     * nothing and left the player with no visible new penalty.
     */
    JokerObject* eligible[MAX_JOKERS_HELD_SIZE];
    int eligible_count = 0;
    ListItr itr = list_itr_create(&_owned_jokers_list);
    JokerObject* joker = NULL;
    while ((joker = list_itr_next(&itr)) &&
           eligible_count < MAX_JOKERS_HELD_SIZE)
        if (joker->joker != NULL && !game_is_joker_disabled(joker))
            eligible[eligible_count++] = joker;
    if (eligible_count > 0)
        boss_disabled_joker = eligible[rng_get_u32() % eligible_count];
}

static void boss_force_random_card(void)
{
    if (!boss_blind_active(BLIND_TYPE_BELL) || get_hand_top() < 0)
        return;
    CardObject** hand = get_hand_array();
    int eligible_indices[MAX_HAND_SIZE];
    int eligible_count = 0;
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL)
        {
            hand[i]->card->boss_flags &= (u8)~CARD_BOSS_FORCED;
            if (eligible_count < MAX_HAND_SIZE)
                eligible_indices[eligible_count++] = i;
        }
    if (eligible_count == 0)
        return;
    int index = eligible_indices[rng_get_u32() % eligible_count];
    if (hand[index] != NULL && hand[index]->card != NULL)
    {
        hand[index]->card->boss_flags |= CARD_BOSS_FORCED;
        if (!card_object_is_selected(hand[index]))
        {
            card_object_set_selected(hand[index], true);
            hand_set_nb_selected_cards(hand_get_nb_selected_cards() + 1);
        }
    }
}

static void boss_ensure_forced_card(void)
{
    if (!boss_blind_active(BLIND_TYPE_BELL) || get_hand_top() < 0)
        return;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL &&
            (hand[i]->card->boss_flags & CARD_BOSS_FORCED))
            return;
    boss_force_random_card();
}

static void boss_enforce_forced_card(void)
{
    if (!boss_blind_active(BLIND_TYPE_BELL))
        return;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL &&
            (hand[i]->card->boss_flags & CARD_BOSS_FORCED) &&
            !card_object_is_selected(hand[i]))
        {
            card_object_set_selected(hand[i], true);
            hand_set_nb_selected_cards(hand_get_nb_selected_cards() + 1);
            compute_hand_value_info();
        }
}

static enum HandState boss_state_after_play(void)
{
    boss_draw_count = boss_blind_active(BLIND_TYPE_SERPENT) ? 3 : UNDEFINED;
    boss_fish_next_draw_face_down = boss_blind_active(BLIND_TYPE_FISH);
    boss_choose_disabled_joker();
    if (!boss_blind_active(BLIND_TYPE_HOOK) || get_hand_top() < 0)
        return HAND_DRAW;

    hand_deselect_all_cards();
    CardObject** hand = get_hand_array();
    int eligible_indices[MAX_HAND_SIZE];
    int available = 0;
    for (int i = 0; i <= get_hand_top() && available < MAX_HAND_SIZE; i++)
        if (hand[i] != NULL && hand[i]->card != NULL)
            eligible_indices[available++] = i;

    int discard_count = min(2, available);
    for (int picked = 0; picked < discard_count; picked++)
    {
        int remaining = available - picked;
        int chosen = picked + (int)(rng_get_u32() % (u32)remaining);
        int index = eligible_indices[chosen];
        eligible_indices[chosen] = eligible_indices[picked];
        eligible_indices[picked] = index;
        card_object_set_selected(hand[index], true);
    }
    hand_set_nb_selected_cards(discard_count);
    return discard_count > 0 ? HAND_DISCARD : HAND_DRAW;
}

// Joker Special Variables
static int shortcut_joker_count = 0;

static int four_fingers_joker_count = 0;

static inline void played_push(CardObject* card_object)
{
    if (played_top >= MAX_SELECTION_SIZE - 1)
        return;
    played[++played_top] = card_object;
}

static inline CardObject* played_pop()
{
    if (played_top < 0)
        return NULL;
    return played[played_top--];
}

static inline bool deck_push(Card* card)
{
    if (card == NULL || deck_top >= MAX_DECK_SIZE - 1)
        return false;
    deck[++deck_top] = card;
    return true;
}

static inline Card* deck_pop()
{
    if (deck_top < 0)
        return NULL;
    return deck[deck_top--];
}

static inline bool discard_push(Card* card)
{
    if (card == NULL || discard_top >= MAX_DECK_SIZE - 1)
        return false;
    discard_pile[++discard_top] = card;
    return true;
}

static inline Card* discard_pop()
{
    if (discard_top < 0)
        return NULL;
    return discard_pile[discard_top--];
}

static inline void display_ante(void)
{
    tte_erase_rect_wrapper(ANTE_TEXT_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%ld#{cx:0x%X000}/%d",
        ANTE_TEXT_RECT.left,
        ANTE_TEXT_RECT.top,
        TTE_YELLOW_PB,
        g_game_vars.ante,
        TTE_WHITE_PB,
        MAX_ANTE
    );
}

void game_init()
{
    state_machine_remove(&game_sm);
    state_machine_register(&game_sm);
    // Initialize all jokers list once
    _owned_jokers_list = list_init();
    _discarded_jokers_list = list_init();
    _expired_jokers_list = list_init();
    // Reset every iterator together with the lists they reference.
    _joker_scored_itr = list_itr_create(&_owned_jokers_list);
    _joker_card_scored_end_itr = list_itr_create(&_owned_jokers_list);
    _joker_round_end_itr = list_itr_create(&_owned_jokers_list);

    game_shop_reset();
    debug_reset();
    alchemical_inventory_reset(&g_game_vars.alchemy);
    voucher_state_reset(&g_game_vars.vouchers);
    memset(g_game_vars.hand_play_counts, 0, sizeof(g_game_vars.hand_play_counts));
    for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
        alchemical_object_destroy(&round_alchemicals[i]);
    alchemy_selected_slot = 0;
    alchemy_pending_use_slot = UNDEFINED;
    alchemy_feedback_timer = 0;
    alchemy_focus = false;
    alchemy_feedback[0] = '\0';
    alchemy_temporary_hand_size = 0;
    alchemy_blind_requirement = 0;
    memset(alchemy_antimony_jokers, 0, sizeof(alchemy_antimony_jokers));
    alchemy_antimony_retriggers = 0;
    memset(alchemy_debuffed_jokers, 0, sizeof(alchemy_debuffed_jokers));
    alchemy_debuffed_joker_count = 0;
    memset(alchemy_acid_cards, 0, sizeof(alchemy_acid_cards));
    alchemy_acid_count = 0;
    memset(alchemy_uranium_backups, 0, sizeof(alchemy_uranium_backups));
    alchemy_uranium_backup_count = 0;
    memset(alchemy_steel_scored, 0, sizeof(alchemy_steel_scored));
    memset(red_seal_retriggered, 0, sizeof(red_seal_retriggered));
    memset(red_seal_held_retriggered, 0, sizeof(red_seal_held_retriggered));
    round_end_gold_cards = 0;
    round_end_blue_seals = 0;
    round_end_alchemy_gold_cards = 0;
    last_played_hand_type = NONE;
    boss_card_reward[0] = '\0';

    /*
     * These values are deliberately reset even though the normal flow also
     * overwrites most of them. A restarted run may arrive here from a scoring,
     * drag, Boss, or description state; carrying any part of that state into
     * the next run creates delayed animation and input bugs.
     */
    play_state = PLAY_STARTING;
    temp_score = 0;
    score_flames_active = false;
    score_lerp_progress = 0;
    chips = 0;
    mult = 0;
    retrigger = false;
    cards_drawn = 0;
    scored_card_index = 0;
    sound_played = false;
    discarded_card = false;
    moving_card = false;
    card_moved_too_fast = false;
    card_selected_instead_of_moved = false;
    selection_hit_timer = UNDEFINED;
    played_top = -1;
    memset(played, 0, sizeof(played));
    shortcut_joker_count = 0;
    four_fingers_joker_count = 0;
    boss_hand_types_played = 0;
    boss_locked_hand_type = NONE;
    boss_ox_hand_type = NONE;
    boss_draw_count = UNDEFINED;
    boss_hand_size_penalty = 0;
    boss_verdant_active = false;
    boss_house_initial_draw = false;
    boss_fish_next_draw_face_down = false;
    boss_acorn_hidden = false;
    boss_disabled_joker = NULL;
    game_playing_selection_grid.selection = GAME_PLAYING_INIT_SEL;

    g_game_vars.hands = MAX_HANDS;
    g_game_vars.discards = MAX_DISCARDS;
    g_game_vars.timer = TM_ZERO;
    g_game_vars.current_blind = BLIND_TYPE_SMALL;
    g_game_vars.next_boss_blind = BLIND_TYPE_BOSS;
    g_game_vars.blinds_states[0] = BLIND_STATE_CURRENT;
    g_game_vars.blinds_states[1] = BLIND_STATE_UPCOMING;
    g_game_vars.blinds_states[2] = BLIND_STATE_UPCOMING;
    g_game_vars.ante = STARTING_ANTE;
    g_game_vars.money = STARTING_MONEY;
    g_game_vars.hand_size = DEFAULT_HAND_SIZE;
    g_game_vars.deck = DECK_TYPE_RED;
    g_game_vars.score = STARTING_SCORE;
    g_game_vars.round = 0;

    // Initialize/reset unbeaten Boss/Showdown Blinds so they are all available
    init_unbeaten_blinds_list(false);
    init_unbeaten_blinds_list(true);
}

void game_reset()
{
    while (list_get_len(&_owned_jokers_list) > 0)
    {
        JokerObject* joker_object = list_get_at_idx(&_owned_jokers_list, 0);
        remove_owned_joker(0);
        joker_object_destroy(&joker_object);
    }
    /*
     * Sold Jokers are no longer in the owned list while their exit animation
     * runs. Destroy them explicitly before clearing the animation list or the
     * fixed Joker/Sprite pools slowly leak across runs.
     */
    while (!list_is_empty(&_discarded_jokers_list))
    {
        JokerObject* joker_object = list_get_at_idx(&_discarded_jokers_list, 0);
        list_remove_at_idx(&_discarded_jokers_list, 0);
        joker_object_destroy(&joker_object);
    }
    while (deck_top >= 0)
    {
        Card* card = deck_pop();
        card_destroy(&card);
    }
    while (discard_top >= 0)
    {
        Card* card = discard_pop();
        card_destroy(&card);
    }
    CardObject** hand = get_hand_array();
    for (int i = get_hand_top(); i >= 0; i--)
    {
        if (hand[i] == NULL)
            continue;
        Card* card = hand[i]->card;
        card_object_destroy(&hand[i]);
        card_destroy(&card);
    }
    set_hand_top(-1);
    for (int i = played_top; i >= 0; i--)
    {
        if (played[i] == NULL)
            continue;
        Card* card = played[i]->card;
        card_object_destroy(&played[i]);
        card_destroy(&card);
    }
    played_top = -1;
    for (int i = 0; i < alchemy_acid_count; i++)
        card_destroy(&alchemy_acid_cards[i]);
    alchemy_acid_count = 0;

    tte_erase_screen();

    // For some reason that I haven't figured out yet,
    // if I don't destroy the blind tokens they won't
    // show up on the next run.
    sprite_destroy(&g_game_vars.playing_blind_token);
    sprite_destroy(&g_game_vars.round_end_blind_token);

    list_clear(&_owned_jokers_list);
    list_clear(&_discarded_jokers_list);
    list_clear(&_expired_jokers_list);

    game_init();

    display_round();
    display_score(g_game_vars.score);
    display_chips();
    display_mult();
    display_hands();
    display_discards();
    display_money();
    // Ante
    display_ante();

    affine_background_load_palette(affine_background_gfxPal);
}

static inline void discarded_jokers_update_loop(void)
{
    if (list_is_empty(&_discarded_jokers_list))
    {
        return;
    }

    ListItr itr = list_itr_create(&_discarded_jokers_list);
    JokerObject* joker_object;

    while ((joker_object = list_itr_next(&itr)))
    {
        if (joker_object == NULL || joker_object->sprite_object == NULL)
        {
            list_itr_remove_current_node(&itr);
            joker_object_destroy(&joker_object);
            continue;
        }
        if (joker_object->sprite_object->x == joker_object->sprite_object->tx &&
            joker_object->sprite_object->y == joker_object->sprite_object->ty)
        {
            list_itr_remove_current_node(&itr);
            joker_object_destroy(&joker_object);
        }
    }
}

static inline void held_jokers_update_loop(void)
{
    FIXED hand_x = int2fx(HELD_JOKERS_POS.x);

    ListItr itr = list_itr_create(&_owned_jokers_list);
    JokerObject* joker;
    JokerObject* description_card = game_shop_get_description_card();
    int joker_count = list_get_len(&_owned_jokers_list);
    int spacing = joker_count <= 4 ? 26 : joker_count <= 6 ? 20 : joker_count == 7 ? 17 : 16;
    int i = 0;
    while ((joker = list_itr_next(&itr)))
    {
        if (joker == NULL || joker->sprite_object == NULL)
            continue;
        // Let the Shop handle the position of this Joker
        if (joker != description_card)
        {
            int centered_offset = ((joker_count - 1) * spacing) / 2 - i * spacing;
            FIXED target_x = hand_x - int2fx(centered_offset);
            if (joker->sprite_object->tx != target_x)
                joker->sprite_object->tx = target_x;
        }
        bool disabled = game_is_joker_disabled(joker);
        bool hidden = boss_acorn_hidden;
        /*
         * A disabled Joker must remain visible so the player can understand
         * what stopped working.  Shrinking it to 80% is a cheap GBA-safe
         * status cue that needs no extra OAM object or graphics allocation.
         */
        FIXED target_scale = disabled ? float2fx(1.25f) : FIX_ONE;
        if (joker->sprite_object->tscale != target_scale)
            joker->sprite_object->tscale = target_scale;
        if (joker->sprite_object != NULL && joker->sprite_object->sprite != NULL)
        {
            OBJ_ATTR* obj = joker->sprite_object->sprite->obj;
            bool currently_hidden =
                obj != NULL && (obj->attr0 & ATTR0_MODE_MASK) == ATTR0_HIDE;
            if (hidden && !currently_hidden)
                obj_hide(joker->sprite_object->sprite->obj);
            else if (!hidden && currently_hidden)
                obj_unhide(joker->sprite_object->sprite->obj, ATTR0_AFF);
        }
        i++;
    }
}

static inline void expired_jokers_update_loop(void)
{
    if (list_is_empty(&_expired_jokers_list))
    {
        return;
    }

    ListItr itr = list_itr_create(&_expired_jokers_list);
    JokerObject* joker_object;

    while ((joker_object = list_itr_next(&itr)))
    {
        // let just enough frames pass that we see it rotating and shrinking
        if (g_game_vars.timer % FRAMES(EXPIRE_ANIMATION_FRAME_COUNT) == 0)
        {
            // get joker idx
            int expired_joker_idx = 0;
            ListItr joker_itr = list_itr_create(&_owned_jokers_list);
            JokerObject* expired_joker;
            while ((expired_joker = list_itr_next(&joker_itr)) && expired_joker != joker_object)
            {
                expired_joker_idx++;
            }

            if (expired_joker != joker_object)
            {
                /*
                 * A stale or duplicate expiry node must be discarded without
                 * dereferencing its possibly freed data pointer.
                 */
                list_itr_remove_current_node(&itr);
                continue;
            }

            // Removing expired Jokers here, instead of immediately like ones we
            // sell or discard allow us to have a small shrink animation without
            // the other owned Jokers rearranging themselves to fill the newly
            // freed space, therefore obscuring the animation
            remove_owned_joker(expired_joker_idx);
            /*
             * Expiration is still a removal. Clear Antimony, Brimstone and
             * boss pointers before the JokerObject returns to its fixed pool;
             * otherwise a later Joker allocated at the same address can
             * inherit the old temporary disable/retrigger state.
             */
            game_notify_joker_removed(false);
            list_itr_remove_current_node(&itr);
            joker_object_destroy(&joker_object);
        }
    }
}

static inline void jokers_update_loop(void)
{
    held_jokers_update_loop();
    discarded_jokers_update_loop();
    expired_jokers_update_loop();
}

void game_update()
{
    rng_update();

    g_game_vars.timer++;

    if (debug_update())
    {
        sprite_object_update_all();
        return;
    }

    jokers_update_loop();
    state_machine_update();

    sprite_object_update_all();
}

void game_change_state(enum GameState new_game_state)
{
    g_game_vars.timer = TM_ZERO; // Reset the timer

    state_machine_change_state(&game_sm, new_game_state);
}

enum GameState game_get_state(void)
{
    return game_sm.state;
}

CardObject** get_played_array(void)
{
    return played;
}

int get_played_top(void)
{
    return played_top;
}

int get_scored_card_index(void)
{
    return scored_card_index;
}

bool is_joker_owned(int joker_id)
{
    ListItr itr = list_itr_create(&_owned_jokers_list);
    JokerObject* joker;

    while ((joker = list_itr_next(&itr)))
    {
        if (joker != NULL && joker->joker != NULL &&
            joker->joker->id == joker_id)
        {
            return true;
        }
    }
    return false;
}

List* get_jokers_list(void)
{
    return &_owned_jokers_list;
}

int game_get_joker_capacity(void)
{
    int capacity =
        deck_get_joker_capacity(
            (enum DeckType)g_game_vars.deck,
            voucher_get_joker_capacity(&g_game_vars.vouchers, BASE_JOKERS_HELD_SIZE)
        );
    ListItr itr = list_itr_create(&_owned_jokers_list);
    JokerObject* joker_object;
    while ((joker_object = list_itr_next(&itr)))
        if (joker_object->joker != NULL &&
            joker_object->joker->modifier == NEGATIVE_EDITION)
            capacity++;
    return min(MAX_JOKERS_HELD_SIZE, capacity);
}

bool game_can_add_joker(const Joker* joker)
{
    if (joker == NULL)
        return false;
    int capacity = game_get_joker_capacity();
    if (joker->modifier == NEGATIVE_EDITION)
        capacity = min(MAX_JOKERS_HELD_SIZE, capacity + 1);
    return list_get_len(&_owned_jokers_list) < capacity;
}

int game_export_deck(Card* cards, int capacity)
{
    if (cards == NULL || capacity <= 0 || deck_top < 0 ||
        deck_top >= MAX_DECK_SIZE || deck_top + 1 > capacity ||
        get_hand_top() >= 0 || played_top >= 0 || discard_top >= 0 ||
        alchemy_acid_count > 0)
        return 0;

    int count = deck_top + 1;
    for (int i = 0; i < count; i++)
    {
        if (deck[i] == NULL || deck[i]->suit >= NUM_SUITS ||
            deck[i]->rank >= NUM_RANKS)
        {
            return 0;
        }
        cards[i] = *deck[i];
    }
    return count;
}

bool game_restore_deck(const Card* cards, int count)
{
    if (cards == NULL || count <= 0 || count > MAX_DECK_SIZE)
        return false;
    for (int i = 0; i < count; i++)
        if (cards[i].suit >= NUM_SUITS || cards[i].rank >= NUM_RANKS ||
            cards[i].enhancement >= CARD_ENHANCEMENT_COUNT ||
            cards[i].edition >= CARD_EDITION_COUNT ||
            cards[i].seal >= CARD_SEAL_COUNT)
            return false;

    while (deck_top >= 0)
    {
        Card* card = deck_pop();
        card_destroy(&card);
    }
    for (int i = 0; i < count; i++)
    {
        Card* card = card_new(cards[i].suit, cards[i].rank);
        if (card == NULL)
            goto restore_failed;
        card->enhancement = cards[i].enhancement;
        card->edition = cards[i].edition;
        card->seal = cards[i].seal;
        /*
         * Saves are written between blinds, so temporary Alchemical and Boss
         * flags must never leak into the resumed blind.
         */
        card->alchemy_flags = 0;
        card->alchemy_original_suit = card->suit;
        card->boss_flags = cards[i].boss_flags & CARD_BOSS_PLAYED_ANTE;
        if (!deck_push(card))
        {
            card_destroy(&card);
            goto restore_failed;
        }
    }
    return true;

restore_failed:
    /*
     * Loading is transactional: a fixed-pool failure must not leave a
     * half-restored deck that later code mistakes for a valid run.
     */
    while (deck_top >= 0)
    {
        Card* card = deck_pop();
        card_destroy(&card);
    }
    return false;
}

void game_clear_deck(void)
{
    while (deck_top >= 0)
    {
        Card* card = deck_pop();
        card_destroy(&card);
    }
}

List* get_expired_jokers_list(void)
{
    return &_expired_jokers_list;
}

List* get_discarded_jokers_list(void)
{
    return &_discarded_jokers_list;
}

bool is_shortcut_joker_active(void)
{
    return shortcut_joker_count > 0 && game_has_active_joker(SHORTCUT_JOKER_ID);
}

int get_straight_and_flush_size(void)
{
    return four_fingers_joker_count > 0 && game_has_active_joker(FOUR_FINGERS_JOKER_ID)
               ? STRAIGHT_AND_FLUSH_SIZE_FOUR_FINGERS
               : STRAIGHT_AND_FLUSH_SIZE_DEFAULT;
}

bool add_joker(JokerObject* joker_object)
{
    if (joker_object == NULL || joker_object->joker == NULL ||
        !list_push_back(&_owned_jokers_list, joker_object))
    {
        return false;
    }

    // TODO: Extract to on_joker_added() callback
    // In case the player gets multiple Four Fingers Jokers,
    // only change size when the first one is added
    if (joker_object->joker->id == FOUR_FINGERS_JOKER_ID)
    {
        four_fingers_joker_count++;
    }

    if (joker_object->joker->id == SHORTCUT_JOKER_ID)
    {
        shortcut_joker_count++;
    }
    return true;
}

void remove_owned_joker(int owned_joker_idx)
{
    // TODO: Extract to on_joker_removed() callback
    JokerObject* joker_object = list_get_at_idx(&_owned_jokers_list, owned_joker_idx);
    if (joker_object == NULL || joker_object->joker == NULL)
        return;
    // In case the player gets multiple Four Fingers Jokers,
    // and only reset the size when all of them have been removed
    if (joker_object->joker->id == FOUR_FINGERS_JOKER_ID)
    {
        four_fingers_joker_count = max(0, four_fingers_joker_count - 1);
    }

    if (joker_object->joker->id == SHORTCUT_JOKER_ID)
    {
        shortcut_joker_count = max(0, shortcut_joker_count - 1);
    }

    game_shop_set_joker_avail(joker_object->joker->id, true);
    list_remove_at_idx(&_owned_jokers_list, owned_joker_idx);
}

int get_deck_top(void)
{
    return deck_top;
}

int get_num_discards_remaining(void)
{
    return g_game_vars.discards;
}

int get_num_hands_remaining(void)
{
    return g_game_vars.hands;
}

u32 get_chips(void)
{
    return chips;
}

void set_chips(u32 new_chips)
{
    chips = new_chips;
}

u32 get_mult(void)
{
    return mult;
}

void set_mult(u32 new_mult)
{
    mult = new_mult;
}

void set_retrigger(bool new_retrigger)
{
    retrigger = new_retrigger;
}

void display_money()
{
    Rect money_text_rect = MONEY_TEXT_RECT;
    tte_erase_rect_wrapper(MONEY_TEXT_RECT);

    /*
     * The status field is seven glyphs wide.  Large but valid economy values
     * used to keep writing past it, leaving isolated letters/numbers in the
     * ante and round area after a background transition.  Format the numeric
     * part with the same bounded suffix logic used by score/chips.
     */
    char money_amount_buff[UINT_MAX_DIGITS + 1];
    int max_amount_chars =
        max(1, rect_width(&money_text_rect) / TTE_CHAR_SIZE - 1);
    truncate_uint_to_suffixed_str(
        (u32)max(0, g_game_vars.money),
        max_amount_chars,
        money_amount_buff
    );
    char money_str_buff[UINT_MAX_DIGITS + 2];
    snprintf(money_str_buff, sizeof(money_str_buff), "$%s", money_amount_buff);

    // Bias left so the number is centered and the "$" sign is on the left
    update_text_rect_to_center_str(&money_text_rect, money_str_buff, SCREEN_LEFT);

    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        money_text_rect.left,
        money_text_rect.top,
        TTE_YELLOW_PB,
        money_str_buff
    );
}

void display_chips(void)
{
    Rect chips_text_rect = CHIPS_TEXT_RECT;

    // In case of overflow, the rect overflow left by 1 char
    Rect chips_text_overflow_rect = chips_text_rect;
    chips_text_overflow_rect.left -= TTE_CHAR_SIZE;
    tte_erase_rect_wrapper(chips_text_overflow_rect);

    char chips_str_buff[UINT_MAX_DIGITS + 1];
    truncate_uint_to_suffixed_str(
        chips,
        rect_width(&chips_text_rect) / TTE_CHAR_SIZE,
        chips_str_buff
    );

    update_text_rect_to_right_align_str(&chips_text_rect, chips_str_buff, OVERFLOW_LEFT);

    tte_printf(
        "#{P:%d,%d; cx:0x%X000;}%s",
        chips_text_rect.left,
        chips_text_rect.top,
        TTE_WHITE_PB,
        chips_str_buff
    );
    check_flaming_score();
}

void display_mult(void)
{
    Rect mult_text_overflow_rect = MULT_TEXT_RECT;
    // In case of overflow the rect will overflow right by 1 char
    mult_text_overflow_rect.right += TTE_CHAR_SIZE;
    tte_erase_rect_wrapper(mult_text_overflow_rect);

    char mult_str_buff[UINT_MAX_DIGITS + 1];
    truncate_uint_to_suffixed_str(mult, rect_width(&MULT_TEXT_RECT) / TTE_CHAR_SIZE, mult_str_buff);

    tte_printf(
        "#{P:%d,%d; cx:0x%X000;}%s",
        MULT_TEXT_RECT.left,
        MULT_TEXT_RECT.top,
        TTE_WHITE_PB,
        mult_str_buff
    );

    check_flaming_score();
}

void display_deck_size_max(void)
{
    // TODO: the text will overflow if deck max size exceeds 99,
    // we will need a fix at some point for this
    tte_erase_rect_wrapper(DECK_SIZE_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%d/%d",
        DECK_SIZE_RECT.left,
        DECK_SIZE_RECT.top,
        TTE_WHITE_PB,
        deck_get_size(),
        deck_get_max_size()
    );
}

// Returns true if the card is *considered* a face card
bool card_is_face(const Card* card)
{
    // Card is a face card, or Pareidolia is present
    return (
        card_has_rank(card) &&
        (card->rank == JACK || card->rank == QUEEN || card->rank == KING ||
         game_has_active_joker(PAREIDOLIA_JOKER_ID))
    );
}

/* Copies the appropriate item into the top left panel (blind/shop icon)
 * from where it was put outside the screenview
 */
static void bg_copy_current_item_to_top_left_panel(void)
{
    main_bg_se_copy_rect(TOP_LEFT_ITEM_SRC_RECT, TOP_LEFT_PANEL_POINT);
}

void change_background_legacy(enum BackgroundId id)
{
    if (background_legacy == id)
    {
        return;
    }
    else if (id == BG_CARD_SELECTING)
    {
        tte_erase_rect_wrapper(HAND_SIZE_RECT_PLAYING);
        REG_WIN0V = (REG_WIN0V << 8) | 0x80; // Set window 0 top to 128

        if (background_legacy == BG_CARD_PLAYING)
        {
            int offset = 11;
            memcpy16(
                &se_mem[MAIN_BG_SBB][SE_ROW_LEN * offset],
                &background_gfxMap[SE_ROW_LEN * offset],
                SE_ROW_LEN * 8
            );
        }
        else
        {
            toggle_windows(true, true); // Enable window 0 for the hand shadow

            // Load the tiles and palette
            // Background
            GRIT_CPY(pal_bg_mem, background_gfxPal);
            dma3_cpy(
                &tile8_mem[MAIN_BG_CBB],
                background_gfxTiles,
                background_gfxTilesLen
            );
            dma3_cpy(
                &se_mem[MAIN_BG_SBB],
                background_gfxMap,
                background_gfxMapLen
            );

            if (g_game_vars.current_blind ==
                BLIND_TYPE_BIG) // Change text and palette depending on blind type
            {
                main_bg_se_copy_rect(BIG_BLIND_TITLE_SRC_RECT, TOP_LEFT_BLIND_TITLE_POINT);
            }
            else if (g_game_vars.current_blind >= BLIND_TYPE_BOSS)
            {
                main_bg_se_copy_rect(BOSS_BLIND_TITLE_SRC_RECT, TOP_LEFT_BLIND_TITLE_POINT);
            }

            bg_copy_current_item_to_top_left_panel();

            // This would change the palette of the background to match the blind, but the backgroun
            // doesn't use the blind token's exact colors so a different approach is required
            memset16(
                &pal_bg_mem[BLIND_BG_PRIMARY_PAL_IDX],
                blind_get_color(g_game_vars.current_blind, BLIND_BACKGROUND_MAIN_COLOR_INDEX),
                1
            );
            memset16(
                &pal_bg_mem[BLIND_BG_SECONDARY_PAL_IDX],
                blind_get_color(g_game_vars.current_blind, BLIND_BACKGROUND_SECONDARY_COLOR_INDEX),
                1
            );
            memset16(
                &pal_bg_mem[BLIND_BG_SHADOW_PAL_IDX],
                blind_get_color(g_game_vars.current_blind, BLIND_BACKGROUND_SHADOW_COLOR_INDEX),
                1
            );

            for (int i = 0; i < NUM_ELEM_IN_ARR(game_playing_buttons); i++)
            {
                button_set_highlight(&game_playing_buttons[i], false);
            }
        }
    }
    else if (id == BG_CARD_PLAYING)
    {
        if (background_legacy != BG_CARD_SELECTING)
        {
            change_background(BG_CARD_SELECTING, false);
            background_legacy = BG_CARD_PLAYING;
        }

        REG_WIN0V = (REG_WIN0V << 8) | 0xA0; // Set window 0 bottom to 160
        toggle_windows(true, true);

        for (int i = 0; i <= 2; i++)
        {
            main_bg_se_move_rect_1_tile_vert(HAND_BG_RECT_SELECTING, SCREEN_DOWN);
        }

        tte_erase_rect_wrapper(HAND_SIZE_RECT_SELECT);
    }
    else if (id == BG_MAIN_MENU || id == BG_BLIND_SELECT || id == BG_SHOP || id == BG_ROUND_END)
    {
        // do nothing, just don't return early!
    }
    else
    {
        return; // Invalid background ID
    }

    background_legacy = id;
}

void reset_background(void)
{
    background_legacy = BG_NONE;
}

static void display_temp_score(u32 value)
{
    char temp_score_str_buff[UINT_MAX_DIGITS + 1];
    Rect temp_score_rect = TEMP_SCORE_RECT;
    truncate_uint_to_suffixed_str(
        value,
        rect_width(&temp_score_rect) / TTE_CHAR_SIZE,
        temp_score_str_buff
    );
    update_text_rect_to_center_str(&temp_score_rect, temp_score_str_buff, SCREEN_RIGHT);

    tte_erase_rect_wrapper(TEMP_SCORE_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        temp_score_rect.left,
        temp_score_rect.top,
        TTE_WHITE_PB,
        temp_score_str_buff
    );
}

void display_score(u32 value)
{
    Rect score_rect = SCORE_RECT;
    // Clear the existing text before redrawing
    tte_erase_rect_wrapper(SCORE_RECT);

    char score_str_buff[UINT_MAX_DIGITS + 1];

    truncate_uint_to_suffixed_str(value, rect_width(&score_rect) / TTE_CHAR_SIZE, score_str_buff);
    update_text_rect_to_center_str(&score_rect, score_str_buff, SCREEN_RIGHT);

    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        score_rect.left,
        score_rect.top,
        TTE_WHITE_PB,
        score_str_buff
    );
}

static void display_alchemy_blind_requirement(void)
{
    Rect blind_req_text_rect = BLIND_REQ_TEXT_RECT;
    tte_erase_rect_wrapper(BLIND_REQ_TEXT_RECT);
    char blind_req_str_buff[UINT_MAX_DIGITS + 1];
    truncate_uint_to_suffixed_str(
        alchemy_blind_requirement,
        rect_width(&BLIND_REQ_TEXT_RECT) / TTE_CHAR_SIZE,
        blind_req_str_buff
    );
    update_text_rect_to_right_align_str(&blind_req_text_rect, blind_req_str_buff, OVERFLOW_RIGHT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        blind_req_text_rect.left,
        blind_req_text_rect.top,
        TTE_RED_PB,
        blind_req_str_buff
    );
}

// Show/Hide flaming score effect if we will score
// more than the required amount or not
static void check_flaming_score(void)
{
    u32 curr_score = u32_protected_mult(chips, mult);
    u32 required_score = alchemy_blind_requirement > 0
                           ? alchemy_blind_requirement
                           : blind_get_requirement(g_game_vars.current_blind, g_game_vars.ante);
    if (curr_score >= required_score && !score_flames_active)
    {
        // start flaming score
        score_flames_active = true;
        return;
    }
    if (curr_score < required_score && score_flames_active)
    {
        // stop flaming score and clear rect
        score_flames_active = false;

        Rect reset_rect = SCORE_FLAME_RESET;
        main_bg_se_copy_rect(reset_rect, SCORE_FLAME_CHIPS_POS);
        reset_rect.left += SCORE_FLAME_FRAME_WIDTH;
        reset_rect.right += SCORE_FLAME_FRAME_WIDTH;
        main_bg_se_copy_rect(reset_rect, SCORE_FLAME_MULT_POS);
    }
}

void display_round(void)
{
    tte_erase_rect_wrapper(ROUND_TEXT_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%ld",
        ROUND_TEXT_RECT.left,
        ROUND_TEXT_RECT.top,
        TTE_YELLOW_PB,
        g_game_vars.round
    );
}

void display_hands(void)
{
    tte_erase_rect_wrapper(HANDS_TEXT_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%ld",
        HANDS_TEXT_RECT.left,
        HANDS_TEXT_RECT.top,
        TTE_BLUE_PB,
        g_game_vars.hands
    );
}

void display_discards(void)
{
    tte_erase_rect_wrapper(DISCARDS_TEXT_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%ld",
        DISCARDS_TEXT_RECT.left,
        DISCARDS_TEXT_RECT.top,
        TTE_RED_PB,
        g_game_vars.discards
    );
}

void display_status_panel(void)
{
    /*
     * State transitions replace the panel artwork but TTE text lives on a
     * separate background. Redraw every dynamic field after clearing that
     * layer so glyphs from card/status messages cannot leak into the shop.
     */
    display_score(g_game_vars.score);
    display_chips();
    display_mult();
    display_hands();
    display_discards();
    display_money();
    display_ante();
    display_round();
    display_deck_size_max();
}

static int deck_get_size(void)
{
    return deck_top + 1;
}

static int deck_get_max_size(void)
{
    // This is the max amount of cards that the player currently has in their possession
    return get_hand_top() + played_top + deck_top + discard_top + alchemy_acid_count + 4;
}

static inline void deck_shuffle(void)
{
    for (int i = deck_top; i > 0; i--)
    {
        int j = rng_get_u32() % (i + 1);
        Card* temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

static void game_round_on_init(void)
{
    /*
     * A blind is a hard boundary for animation and input state.  Reset this
     * before creating any new objects so an interrupted previous scoring
     * sequence cannot skip, stall, or animate cards in the next blind.
     */
    play_state = PLAY_STARTING;
    temp_score = 0;
    score_flames_active = false;
    score_lerp_progress = 0;
    chips = 0;
    mult = 0;
    retrigger = false;
    cards_drawn = 0;
    scored_card_index = 0;
    sound_played = false;
    discarded_card = false;
    moving_card = false;
    card_moved_too_fast = false;
    card_selected_instead_of_moved = false;
    selection_hit_timer = UNDEFINED;
    game_playing_selection_grid.selection = GAME_PLAYING_INIT_SEL;
    _joker_scored_itr = list_itr_create(&_owned_jokers_list);
    _joker_card_scored_end_itr = list_itr_create(&_owned_jokers_list);
    _joker_round_end_itr = list_itr_create(&_owned_jokers_list);

    boss_hand_types_played = 0;
    boss_locked_hand_type = NONE;
    boss_ox_hand_type =
        boss_blind_active(BLIND_TYPE_OX)
            ? boss_ox_choose_hand(
                  g_game_vars.hand_play_counts,
                  ALCHEMICAL_HAND_TYPE_COUNT
              )
            : NONE;
    boss_draw_count = UNDEFINED;
    boss_hand_size_penalty = 0;
    boss_verdant_active = boss_blind_active(BLIND_TYPE_LEAF);
    boss_house_initial_draw = boss_blind_active(BLIND_TYPE_HOUSE);
    boss_fish_next_draw_face_down = false;
    boss_acorn_hidden = boss_blind_active(BLIND_TYPE_ACORN);
    boss_disabled_joker = NULL;
    memset(red_seal_retriggered, 0, sizeof(red_seal_retriggered));
    memset(red_seal_held_retriggered, 0, sizeof(red_seal_held_retriggered));
    round_end_gold_cards = 0;
    round_end_blue_seals = 0;
    round_end_alchemy_gold_cards = 0;
    last_played_hand_type = NONE;
    g_game_vars.hands = deck_get_hands_per_blind(
        (enum DeckType)g_game_vars.deck,
        voucher_get_hands_per_blind(&g_game_vars.vouchers, MAX_HANDS)
    );
    g_game_vars.discards = deck_get_discards_per_blind(
        (enum DeckType)g_game_vars.deck,
        voucher_get_discards_per_blind(&g_game_vars.vouchers, MAX_DISCARDS)
    );
    if (boss_blind_active(BLIND_TYPE_WATER))
        g_game_vars.discards = 0;
    if (boss_blind_active(BLIND_TYPE_NEEDLE))
        g_game_vars.hands = 1;
    if (boss_blind_active(BLIND_TYPE_MANACLE) && g_game_vars.hand_size > 1)
    {
        g_game_vars.hand_size--;
        boss_hand_size_penalty = 1;
    }
    if (boss_blind_active(BLIND_TYPE_ACORN))
    {
        int count = list_get_len(&_owned_jokers_list);
        for (int i = count - 1; i > 0; i--)
            list_swap(&_owned_jokers_list, i, rng_get_u32() % (i + 1));
    }
    boss_choose_disabled_joker();
    display_hands();
    display_discards();

    set_hand_state(HAND_DRAW);
    hand_set_nb_selected_cards(0);
    cards_drawn = 0;

    sprite_destroy(&g_game_vars.playing_blind_token);
    g_game_vars.playing_blind_token = blind_token_new(
        g_game_vars.current_blind,
        CUR_BLIND_TOKEN_POS.x,
        CUR_BLIND_TOKEN_POS.y,
        PLAYING_BLIND_TOKEN_LAYER
    ); // Create the blind token sprite at the top left corner
    // TODO: Hide blind token and display it after sliding blind rect animation
    // if (g_game_vars.playing_blind_token != NULL)
    //{
    //    obj_hide(g_game_vars.playing_blind_token->obj); // Hide the blind token sprite for now
    //}
    sprite_destroy(&g_game_vars.round_end_blind_token);
    g_game_vars.round_end_blind_token = blind_token_new(
        g_game_vars.current_blind,
        81,
        86,
        ROUND_END_BLIND_TOKEN_LAYER
    ); // Create the blind token sprite for round end

    if (g_game_vars.round_end_blind_token != NULL)
    {
        obj_hide(g_game_vars.round_end_blind_token->obj); // Hide the blind token sprite for now
    }

    Rect blind_req_text_rect = BLIND_REQ_TEXT_RECT;
    alchemy_blind_requirement =
        blind_get_requirement(g_game_vars.current_blind, g_game_vars.ante);
    u32 blind_requirement = alchemy_blind_requirement;

    char blind_req_str_buff[UINT_MAX_DIGITS + 1];

    truncate_uint_to_suffixed_str(
        blind_requirement,
        rect_width(&BLIND_REQ_TEXT_RECT) / TTE_CHAR_SIZE,
        blind_req_str_buff
    );

    // Update text rect for right alignment AFTER shortening the number
    update_text_rect_to_right_align_str(&blind_req_text_rect, blind_req_str_buff, OVERFLOW_RIGHT);

    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        blind_req_text_rect.left,
        blind_req_text_rect.top,
        TTE_RED_PB,
        blind_req_str_buff
    );
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}$%d",
        BLIND_REWARD_RECT.left,
        BLIND_REWARD_RECT.top,
        TTE_YELLOW_PB,
        blind_get_reward(g_game_vars.current_blind)
    ); // Blind reward

    deck_shuffle(); // Shuffle the deck at the start of the round

    /* Note that since cards_in_hand_update_loop() handles card highlight there's no need
     * to call a selection changed callback to highlight the initial card, this wouldn't work
     * otherwise or for the buttons.
     */
    game_playing_selection_grid.selection = GAME_PLAYING_INIT_SEL;
    alchemy_focus = false;
    alchemy_selected_slot = 0;
    alchemy_pending_use_slot = UNDEFINED;
    tte_erase_rect_wrapper(ALCHEMY_DETAIL_RECT);
    tte_erase_rect_wrapper(ALCHEMY_FEEDBACK_RECT);
    game_alchemy_sync_round_objects();
    if (g_game_vars.current_blind >= BLIND_TYPE_BOSS)
        game_alchemy_set_feedback(blind_get_description(g_game_vars.current_blind));
}

// Playing state functions
static bool can_discard_hand(void)
{
    return (
        g_game_vars.discards > 0 && get_hand_state() == HAND_SELECT &&
        hand_get_nb_selected_cards() > 0
    );
}

static void game_playing_discard_on_pressed(void)
{
    if (!can_discard_hand())
        return;

    game_playing_execute_discard();

    // Move back to hand selection
    selection_grid_move_selection_vert(&game_playing_selection_grid, -1);
}

static void game_playing_execute_discard(void)
{
    if (!can_discard_hand())
        return;

    if (boss_blind_active(BLIND_TYPE_SERPENT))
        boss_draw_count = 3;
    set_hand_state(HAND_DISCARD);
    --g_game_vars.discards;
    display_discards();
    compute_hand_value_info();
}

static void game_playing_sort_by_rank_on_pressed(void)
{
    hand_change_sort(false);
}

static void game_playing_sort_by_suit_on_pressed(void)
{
    hand_change_sort(true);
}

static void game_playing_play_hand_on_pressed(void)
{
    if (!can_play_hand())
        return;

    game_playing_execute_play_hand();

    // Move back to hand selection
    selection_grid_move_selection_vert(&game_playing_selection_grid, -1);
}

static void game_playing_execute_play_hand(void)
{
    if (!can_play_hand())
        return;

    enum HandType played_type = get_hand_type();
    last_played_hand_type = played_type;
    memset(red_seal_retriggered, 0, sizeof(red_seal_retriggered));
    memset(red_seal_held_retriggered, 0, sizeof(red_seal_held_retriggered));
    if (played_type > NONE && played_type < ALCHEMICAL_HAND_TYPE_COUNT)
    {
        if (boss_blind_active(BLIND_TYPE_OX))
        {
            if (played_type == boss_ox_hand_type)
            {
                g_game_vars.money = 0;
                display_money();
            }
        }
        if (boss_blind_active(BLIND_TYPE_ARM) &&
            g_game_vars.alchemy.hand_levels[played_type] > 0)
        {
            g_game_vars.alchemy.hand_levels[played_type]--;
            compute_hand_value_info();
        }
        if (g_game_vars.hand_play_counts[played_type] < UINT16_MAX)
            g_game_vars.hand_play_counts[played_type]++;
        boss_hand_types_played |= (u16)(1U << played_type);
        if (boss_locked_hand_type == NONE)
            boss_locked_hand_type = played_type;
    }
    if (boss_blind_active(BLIND_TYPE_FLINT))
    {
        chips = max(1, chips / 2);
        mult = max(1, mult / 2);
        display_chips();
        display_mult();
    }
    if (boss_blind_active(BLIND_TYPE_TOOTH))
    {
        g_game_vars.money = max(0, g_game_vars.money - hand_get_nb_selected_cards());
        display_money();
    }
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
    {
        if (hand[i] == NULL || hand[i]->card == NULL ||
            !card_object_is_selected(hand[i]))
            continue;
        /*
         * The Pillar also has to remember a card replayed during its own
         * blind (for example after Phosphorus returns the discard pile).
         * Boss cleanup removes this Ante-only flag before the next Ante.
         */
        hand[i]->card->boss_flags |= CARD_BOSS_PLAYED_ANTE;
        hand[i]->card->boss_flags &= (u8)~(CARD_BOSS_FACE_DOWN | CARD_BOSS_FORCED);
    }
    if (boss_blind_active(BLIND_TYPE_ACORN))
        boss_acorn_hidden = false;
    set_hand_state(HAND_PLAY);
    --g_game_vars.hands;
    display_hands();
}

static int game_playing_hand_row_get_size(void)
{
    return hand_nb_held_cards();
}

// card moving logic

static bool game_playing_hand_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    int prev_card_idx = UNDEFINED;
    int next_card_idx = UNDEFINED;

    // Do not use FRAMES(x) here as we are counting real frames ignoring game speed
    card_moved_too_fast = (selection_hit_timer != UNDEFINED) &&
                          (g_game_vars.timer - selection_hit_timer) < card_swap_time_threshold;

    if (prev_selection->y == GAME_PLAYING_HAND_SEL_Y)
    {
        prev_card_idx = hand_sel_idx_to_card_idx(prev_selection->x);
    }

    if (new_selection->y == GAME_PLAYING_HAND_SEL_Y)
    {
        next_card_idx = hand_sel_idx_to_card_idx(new_selection->x);
    }

    bool on_the_same_row = new_selection->y == prev_selection->y; // == GAME_PLAYING_HAND_SEL_Y

    if (on_the_same_row && key_is_down(SELECT_CARD) && !card_moved_too_fast &&
        !card_selected_instead_of_moved)
    {
        bool moved_by_one_tile = abs(new_selection->x - prev_selection->x) == 1;

        // Avoid swapping when selection wraps
        if (!moved_by_one_tile)
        {
            // Abort the selection if swapping so it doesn't wrap
            return false;
        }
        else
        {
            swap_cards_in_hand(prev_card_idx, next_card_idx);
            moving_card = true;
            reorder_card_sprites_layers();

            /* Keep card reordering OAM-only; selection has its own SFX. */
        }
    }
    else
    {
        // select current card if we tried moving it too fast
        if (key_released(SELECT_CARD) || (card_moved_too_fast && !moving_card))
        {
            hand_select_card(prev_card_idx);
            card_selected_instead_of_moved = true;
        }
        /* Cursor-only movement deliberately has no streamed sound effect. */
    }

    return true;
}

static void game_playing_hand_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
)
{
    if (key_hit(SELECT_CARD))
    {
        selection_hit_timer = g_game_vars.timer;
    }
    else if (key_released(SELECT_CARD))
    {
        if (!moving_card && !card_selected_instead_of_moved)
        {
            hand_select_card(hand_sel_idx_to_card_idx(selection->x));
        }
        moving_card = false;
        card_moved_too_fast = false;
        card_selected_instead_of_moved = false;
        selection_hit_timer = UNDEFINED;
    }
    else if (key_hit(DESELECT_CARDS))
    {
        hand_deselect_all_cards();
        compute_hand_value_info();
    }
    else if (key_hit(PLAY_HAND_KEY))
    {
        game_playing_execute_play_hand();
    }
    else if (key_hit(DISCARD_HAND_KEY))
    {
        game_playing_execute_discard();
    }
}

static int game_playing_button_row_get_size(void)
{
    return NUM_ELEM_IN_ARR(game_playing_buttons);
}

static inline void game_playing_button_set_highlight(int btn_idx, bool highlight)
{
    if (btn_idx < 0 || btn_idx >= game_playing_button_row_get_size())
        return;
    button_set_highlight(&game_playing_buttons[btn_idx], highlight);
}

static bool game_playing_button_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    // The selection grid system only guarantees that the new selection is within bounds
    // but not the previous one...
    // As of writing (PR #348), this check is not strictly needed for this row but it is
    // left in, in case that ever changes. It can be reconsidered and removed.
    if (prev_selection->y == row_idx && prev_selection->x >= 0 &&
        prev_selection->x < game_playing_button_row_get_size())
    {
        game_playing_button_set_highlight(prev_selection->x, false);
    }

    if (new_selection->y == row_idx && new_selection->x >= 0 &&
        new_selection->x < game_playing_button_row_get_size())
    {
        game_playing_button_set_highlight(new_selection->x, true);
    }

    return true;
}

static void game_playing_button_row_on_key_hit(SelectionGrid* selection_grid, Selection* selection)
{
    if (key_hit(SELECT_CARD) && selection->x >= 0 &&
        selection->x < game_playing_button_row_get_size())
    {
        button_press(&game_playing_buttons[selection->x]);
    }
}

static bool can_play_hand(void)
{
    if (get_hand_state() != HAND_SELECT || hand_get_nb_selected_cards() == 0)
        return false;
    if (boss_blind_active(BLIND_TYPE_PSYCHIC) && hand_get_nb_selected_cards() != 5)
    {
        game_alchemy_set_feedback("Must play 5 cards");
        return false;
    }
    enum HandType type = get_hand_type();
    if (boss_blind_active(BLIND_TYPE_EYE) && type > NONE &&
        (boss_hand_types_played & (1U << type)))
    {
        game_alchemy_set_feedback("Hand type already used");
        return false;
    }
    if (boss_blind_active(BLIND_TYPE_MOUTH) && boss_locked_hand_type != NONE &&
        type != boss_locked_hand_type)
    {
        game_alchemy_set_feedback("Must repeat hand type");
        return false;
    }
    return true;
}

/**
 * @brief Converts a selection index from the selection grid into a card index within the hand array
 * @param selection_index The selection index from the selection grid.
 * @return The index within the hand stack array.
 * Note that the result is not valid if hand size is 0.
 */
static inline int hand_sel_idx_to_card_idx(int selection_index)
{
    // This is because the hand is drawn from right to left.
    // There is no particular reason for why that was done, it's just how it was done.
    // Maybe one day it can be reverted and made consistent so this conversion is not needed.
    return hand_nb_held_cards() - selection_index - 1;
}

static inline void game_playing_process_hand_select_input(void)
{
    selection_grid_process_input(&game_playing_selection_grid);
}

static inline bool card_draw(void)
{
    if (deck_top < 0 || get_hand_top() >= g_game_vars.hand_size - 1 ||
        get_hand_top() >= MAX_HAND_SIZE - 1)
        return false;

    Card* card = deck_pop();
    if (card == NULL)
        return false;
    card->boss_flags &= (u8)~(CARD_BOSS_FACE_DOWN | CARD_BOSS_FORCED);
    if (!(card->alchemy_flags & CARD_ALCHEMY_OILED) &&
        (boss_house_initial_draw ||
         (boss_blind_active(BLIND_TYPE_FISH) &&
          boss_fish_next_draw_face_down) ||
         (boss_blind_active(BLIND_TYPE_WHEEL) && rng_get_u32() % 7 == 0) ||
         (boss_blind_active(BLIND_TYPE_MARK) && card_has_rank(card) && card->rank >= JACK &&
          card->rank <= KING)))
    {
        card->boss_flags |= CARD_BOSS_FACE_DOWN;
    }
    boss_refresh_card(card);
    CardObject* card_object = card_object_new(card);
    if (card_object == NULL)
    {
        deck_push(card);
        return false;
    }

    const FIXED deck_x = int2fx(CARD_DRAW_POS.x);
    const FIXED deck_y = int2fx(CARD_DRAW_POS.y);

    card_object->sprite_object->x = deck_x;
    card_object->sprite_object->y = deck_y;

    set_hand_top(get_hand_top() + 1);
    get_hand_array()[get_hand_top()] = card_object;

    // Sort the hand after drawing a card
    sort_cards();

    play_sfx(
        SFX_CARD_DRAW,
        MM_BASE_PITCH_RATE + cards_drawn * PITCH_STEP_DRAW_SFX,
        SFX_DEFAULT_VOLUME
    );
    return true;
}

static void game_alchemy_set_feedback(const char* message)
{
    snprintf(alchemy_feedback, sizeof(alchemy_feedback), "%.23s", message);
    alchemy_feedback_timer = ALCHEMY_FEEDBACK_FRAMES;
    tte_erase_rect_wrapper(ALCHEMY_FEEDBACK_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%.21s",
        ALCHEMY_FEEDBACK_RECT.left,
        ALCHEMY_FEEDBACK_RECT.top,
        TTE_YELLOW_PB,
        alchemy_feedback
    );
}

static void game_alchemy_set_success_feedback(
    enum AlchemicalId id,
    const AlchemicalInfo* info,
    int selected_count,
    int drawn_count
)
{
    char message[24];
    switch (id)
    {
        case ALCHEMICAL_IGNIS:
            snprintf(message, sizeof(message), "+1 discard");
            break;
        case ALCHEMICAL_AQUA:
            snprintf(message, sizeof(message), "+1 hand");
            break;
        case ALCHEMICAL_TERRA:
            snprintf(message, sizeof(message), "Blind target -15%%");
            break;
        case ALCHEMICAL_AERO:
            snprintf(message, sizeof(message), "Drew %hhu card%s", (u8)drawn_count,
                     drawn_count == 1 ? "" : "s");
            break;
        case ALCHEMICAL_QUICKSILVER:
            snprintf(message, sizeof(message), "Size +2, drew %hhu", (u8)drawn_count);
            break;
        case ALCHEMICAL_SALT:
            snprintf(message, sizeof(message), "+$4");
            break;
        case ALCHEMICAL_SULFUR:
            snprintf(message, sizeof(message), "Hands sold for cash");
            break;
        case ALCHEMICAL_PHOSPHORUS:
            if (drawn_count > 0)
                snprintf(
                    message,
                    sizeof(message),
                    "Shuffled, drew %hhu",
                    (u8)drawn_count
                );
            else
                snprintf(message, sizeof(message), "Discards shuffled in");
            break;
        case ALCHEMICAL_GOLD:
            snprintf(message, sizeof(message), "Hold Gold: +$2 each");
            break;
        case ALCHEMICAL_BISMUTH:
            snprintf(message, sizeof(message), "Selected cards: x1.5");
            break;
        case ALCHEMICAL_MANGANESE:
            snprintf(message, sizeof(message), "Selected cards: Steel");
            break;
        case ALCHEMICAL_GLASS:
            snprintf(message, sizeof(message), "Selected cards: x2");
            break;
        case ALCHEMICAL_SILVER:
            snprintf(message, sizeof(message), "Selected cards: Lucky");
            break;
        case ALCHEMICAL_OIL:
            snprintf(message, sizeof(message), "Hand debuffs removed");
            break;
        case ALCHEMICAL_COBALT:
            snprintf(message, sizeof(message), "Poker hand leveled");
            break;
        case ALCHEMICAL_ARSENIC:
            snprintf(message, sizeof(message), "Hands <-> discards");
            break;
        case ALCHEMICAL_ANTIMONY:
            snprintf(message, sizeof(message), "Left Joker retriggers");
            break;
        case ALCHEMICAL_SOAP:
            snprintf(message, sizeof(message), "Replaced %d card%s", selected_count,
                     selected_count == 1 ? "" : "s");
            break;
        case ALCHEMICAL_WAX:
            snprintf(message, sizeof(message), "Made 2 temp copies");
            break;
        case ALCHEMICAL_BORAX:
            snprintf(message, sizeof(message), "Suits changed");
            break;
        case ALCHEMICAL_MAGNET:
            snprintf(message, sizeof(message), "Drew matching ranks");
            break;
        case ALCHEMICAL_ACID:
            snprintf(message, sizeof(message), "Rank hidden for blind");
            break;
        case ALCHEMICAL_BRIMSTONE:
            snprintf(message, sizeof(message), "+2 each, Joker muted");
            break;
        case ALCHEMICAL_URANIUM:
            snprintf(message, sizeof(message), "Modifiers copied");
            break;
        default:
            snprintf(message, sizeof(message), "%s used", info->name);
            break;
    }
    game_alchemy_set_feedback(message);
}

static void game_alchemy_show_detail(void)
{
    /* In-blind descriptions use the same focused-card interaction as the rest
     * of the game; never paint transient prose over the playing field. */
    tte_erase_rect_wrapper(ALCHEMY_DETAIL_RECT);
}

static void game_alchemy_sync_round_objects(void)
{
    static const int icon_x[ALCHEMICAL_HELD_LIMIT] = {175, 191, 207};
    alchemical_object_init();
    for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
    {
        alchemical_object_destroy(&round_alchemicals[i]);
        if (i >= g_game_vars.alchemy.count)
            continue;
        round_alchemicals[i] = alchemical_object_new(g_game_vars.alchemy.held[i], i);
        if (round_alchemicals[i] != NULL)
            sprite_object_position(round_alchemicals[i]->sprite_object, icon_x[i], 16);
    }
}

bool game_debug_add_alchemical(enum AlchemicalId id)
{
    if (!alchemical_inventory_add(&g_game_vars.alchemy, id))
        return false;
    if (game_get_state() == GAME_STATE_PLAYING)
        game_alchemy_sync_round_objects();
    else if (game_get_state() == GAME_STATE_SHOP)
        game_shop_debug_refresh();
    return true;
}

bool game_debug_apply_card_modifier(int category, int value)
{
    if (game_get_state() != GAME_STATE_PLAYING)
        return false;
    CardObject** hand = get_hand_array();
    Card* selected = NULL;
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL &&
            card_object_is_selected(hand[i]))
        {
            selected = hand[i]->card;
            break;
        }
    if (selected == NULL)
        return false;
    if (category == 0 && value >= 0 && value < CARD_ENHANCEMENT_COUNT)
        selected->enhancement = value;
    else if (category == 1 && value >= 0 && value < CARD_EDITION_COUNT)
        selected->edition = value;
    else if (category == 2 && value >= 0 && value < CARD_SEAL_COUNT)
        selected->seal = value;
    else
        return false;
    reorder_card_sprites_layers();
    compute_hand_value_info();
    return true;
}

void game_debug_win_blind(void)
{
    if (game_get_state() != GAME_STATE_PLAYING)
        return;
    g_game_vars.score = max(g_game_vars.score, max(1, alchemy_blind_requirement));
    display_score(g_game_vars.score);
}

static void game_alchemy_refresh_round_objects_after_use(int removed_slot)
{
    (void)removed_slot;
    static const int icon_x[ALCHEMICAL_HELD_LIMIT] = {175, 191, 207};
    for (int slot = 0; slot < ALCHEMICAL_HELD_LIMIT; slot++)
    {
        if (slot >= g_game_vars.alchemy.count)
        {
            alchemical_object_destroy(&round_alchemicals[slot]);
            continue;
        }
        if (round_alchemicals[slot] == NULL)
            round_alchemicals[slot] =
                alchemical_object_new(g_game_vars.alchemy.held[slot], slot);
        else
            alchemical_object_set_id(
                round_alchemicals[slot],
                g_game_vars.alchemy.held[slot]
            );
        if (round_alchemicals[slot] != NULL)
            sprite_object_position(
                round_alchemicals[slot]->sprite_object,
                icon_x[slot],
                16
            );
    }
}

static int game_playing_top_row_get_size(void)
{
    return jokers_sel_row_get_size() + g_game_vars.alchemy.count;
}

static bool game_playing_top_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    int joker_count = jokers_sel_row_get_size();
    bool prev_is_joker =
        prev_selection->y == row_idx && prev_selection->x >= 0 && prev_selection->x < joker_count;
    bool new_is_joker =
        new_selection->y == row_idx && new_selection->x >= 0 && new_selection->x < joker_count;

    if (prev_is_joker && new_is_joker)
    {
        return jokers_sel_row_on_selection_changed(
            selection_grid,
            row_idx,
            prev_selection,
            new_selection
        );
    }

    if (prev_is_joker)
    {
        JokerObject* joker = list_get_at_idx(get_jokers_list(), prev_selection->x);
        if (joker != NULL)
        {
            sprite_object_erase_text_under(joker->sprite_object);
            joker_object_set_focus(joker, false);
        }
    }
    else if (prev_selection->y == row_idx && prev_selection->x >= joker_count)
    {
        int slot = prev_selection->x - joker_count;
        if (slot >= 0 && slot < ALCHEMICAL_HELD_LIMIT && round_alchemicals[slot] != NULL)
        {
            sprite_object_erase_text_under(round_alchemicals[slot]->sprite_object);
            alchemical_object_set_focus(round_alchemicals[slot], false);
        }
        alchemy_focus = false;
        tte_erase_rect_wrapper(ALCHEMY_DETAIL_RECT);
    }

    if (new_is_joker)
    {
        JokerObject* joker = list_get_at_idx(get_jokers_list(), new_selection->x);
        if (joker != NULL)
        {
            joker_object_set_focus(joker, true);
            sprite_object_print_price_under(
                joker->sprite_object,
                joker_get_sell_value(joker->joker)
            );
        }
    }
    else if (new_selection->y == row_idx && new_selection->x >= joker_count)
    {
        int slot = new_selection->x - joker_count;
        if (slot >= 0 && slot < g_game_vars.alchemy.count && round_alchemicals[slot] != NULL)
        {
            alchemy_selected_slot = slot;
            alchemy_focus = true;
            alchemical_object_set_focus(round_alchemicals[slot], true);
            sprite_object_print_price_under(
                round_alchemicals[slot]->sprite_object,
                alchemical_get_sell_value(g_game_vars.alchemy.held[slot])
            );
            game_alchemy_show_detail();
        }
    }

    return true;
}

static void game_playing_top_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
)
{
    int joker_count = jokers_sel_row_get_size();
    if (selection->x < joker_count)
    {
        jokers_sel_row_on_key_transit(selection_grid, selection);
        return;
    }

    int slot = selection->x - joker_count;
    if (slot < 0 || slot >= g_game_vars.alchemy.count)
        return;

    alchemy_selected_slot = slot;
    if (key_hit(SELL_KEY))
    {
        enum AlchemicalId id = g_game_vars.alchemy.held[slot];
        int sell_value = alchemical_get_sell_value(id);
        if (round_alchemicals[slot] != NULL)
            sprite_object_erase_text_under(round_alchemicals[slot]->sprite_object);
        selection_grid_move_selection_vert(selection_grid, SCREEN_DOWN);
        if (alchemical_inventory_sell(&g_game_vars.alchemy, slot))
        {
            g_game_vars.money =
                g_game_vars.money > INT_MAX - sell_value
                    ? INT_MAX
                    : g_game_vars.money + sell_value;
            display_money();
            game_alchemy_refresh_round_objects_after_use(slot);
            game_alchemy_set_feedback("Alchemical sold");
        }
        return;
    }
    if (key_hit(SELECT_CARD))
    {
        if (round_alchemicals[slot] != NULL)
        {
            sprite_object_erase_text_under(round_alchemicals[slot]->sprite_object);
            alchemical_object_set_focus(round_alchemicals[slot], false);
        }
        alchemy_focus = false;
        card_selected_instead_of_moved = true;
        selection_grid->selection = (Selection){0, GAME_PLAYING_HAND_SEL_Y};
        alchemy_pending_use_slot = slot;
    }
}

static void game_alchemy_recount_selection(void)
{
    int count = 0;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        count += hand[i] != NULL && hand[i]->card != NULL && card_object_is_selected(hand[i]);
    hand_set_nb_selected_cards(count);
}

static Card* game_alchemy_remove_hand_card(int index)
{
    CardObject** hand = get_hand_array();
    int top = get_hand_top();
    if (index < 0 || index > top || hand[index] == NULL)
        return NULL;
    Card* card = hand[index]->card;
    card_object_destroy(&hand[index]);
    for (int i = index; i < top; i++)
        hand[i] = hand[i + 1];
    hand[top] = NULL;
    set_hand_top(top - 1);
    return card;
}

static bool game_alchemy_add_card_to_hand(Card* card, int capacity)
{
    int top = get_hand_top();
    capacity = min(max(1, capacity), MAX_HAND_SIZE);
    if (card == NULL || top >= capacity - 1)
        return false;
    CardObject* object = card_object_new(card);
    if (object == NULL)
        return false;
    if (object->sprite_object == NULL)
    {
        card_object_destroy(&object);
        return false;
    }
    sprite_object_position(object->sprite_object, CARD_DRAW_POS.x, CARD_DRAW_POS.y);
    get_hand_array()[top + 1] = object;
    set_hand_top(top + 1);
    return true;
}

static Card* game_alchemy_take_deck_card(int index)
{
    if (index < 0 || index > deck_top)
        return NULL;
    Card* card = deck[index];
    for (int i = index; i < deck_top; i++)
        deck[i] = deck[i + 1];
    deck[deck_top--] = NULL;
    return card;
}

static int game_alchemy_draw_cards(int amount, int required_rank, int capacity)
{
    int drawn = 0;
    capacity = min(max(1, capacity), MAX_HAND_SIZE);
    while (drawn < amount && get_hand_top() < capacity - 1 && deck_top >= 0)
    {
        int index = deck_top;
        if (required_rank >= 0)
        {
            while (index >= 0 &&
                   (deck[index] == NULL || !card_has_rank(deck[index]) ||
                    deck[index]->rank != required_rank))
                index--;
            if (index < 0)
                break;
        }
        Card* card = game_alchemy_take_deck_card(index);
        if (card == NULL)
            break;
        if (!game_alchemy_add_card_to_hand(card, capacity))
        {
            deck_push(card);
            break;
        }
        drawn++;
    }
    if (drawn > 0)
    {
        sort_cards();
        game_playing_selection_grid.selection.x = 0;
        play_sfx(SFX_CARD_DRAW, MM_BASE_PITCH_RATE, SFX_DEFAULT_VOLUME);
    }
    return drawn;
}

static Card* game_alchemy_get_selected_card(void)
{
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL && card_object_is_selected(hand[i]))
            return hand[i]->card;
    return NULL;
}

static bool game_alchemy_card_has_flag_or_equivalent(const Card* card, u8 flag)
{
    if (card == NULL)
        return false;
    if ((card->alchemy_flags & flag) != 0)
        return true;

    switch (flag)
    {
        case CARD_ALCHEMY_POLY:
            return card->edition == CARD_EDITION_POLYCHROME;
        case CARD_ALCHEMY_STEEL:
            return card->enhancement == CARD_ENHANCEMENT_STEEL;
        case CARD_ALCHEMY_GLASS:
            return card->enhancement == CARD_ENHANCEMENT_GLASS;
        case CARD_ALCHEMY_GOLD:
            return card->enhancement == CARD_ENHANCEMENT_GOLD;
        case CARD_ALCHEMY_LUCKY:
            return card->enhancement == CARD_ENHANCEMENT_LUCKY;
        default:
            return false;
    }
}

static bool game_alchemy_card_can_apply_flag(const Card* card, u8 flag)
{
    if (card == NULL)
        return false;
    u8 exclusive_flags = flag;
    if ((flag & CARD_ALCHEMY_ENHANCEMENT_MASK) != 0)
        exclusive_flags = CARD_ALCHEMY_ENHANCEMENT_MASK;
    return alchemical_card_flag_can_apply(
        card->alchemy_flags,
        flag,
        game_alchemy_card_has_flag_or_equivalent(card, flag),
        exclusive_flags
    );
}

static int game_alchemy_apply_flag_to_selected(u8 flag, bool include_temp_copies)
{
    int changed = 0;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL && card_object_is_selected(hand[i]))
        {
            Card* card = hand[i]->card;
            /*
             * Boss-debuffed cards cannot produce scoring or held-card effects.
             * Reject them here rather than consuming a modifier that appears to
             * apply but is guaranteed to do nothing for the rest of the blind.
             */
            if (card->boss_flags & CARD_BOSS_DEBUFFED)
                continue;
            if (!include_temp_copies &&
                (card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY))
            {
                continue;
            }
            if (!game_alchemy_card_can_apply_flag(card, flag))
                continue;
            card->alchemy_flags |= flag;
            changed++;
        }
    if (changed > 0)
        reorder_card_sprites_layers();
    return changed;
}

static int game_alchemy_count_selected_without_flag(u8 flag, bool include_temp_copies)
{
    int count = 0;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL &&
            card_object_is_selected(hand[i]) &&
            !(hand[i]->card->boss_flags & CARD_BOSS_DEBUFFED) &&
            (include_temp_copies ||
             !(hand[i]->card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY)) &&
            game_alchemy_card_can_apply_flag(hand[i]->card, flag))
        {
            count++;
        }
    return count;
}

static void game_alchemy_animate_selected(void)
{
    bool played_sound = false;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
    {
        if (hand[i] == NULL || hand[i]->card == NULL || !card_object_is_selected(hand[i]))
            continue;
        card_object_shake(hand[i], played_sound ? UNDEFINED : SFX_CARD_SELECT);
        played_sound = true;
    }
}

static bool game_alchemy_sync_scalar(enum AlchemicalId id, int* cards_to_draw)
{
    AlchemicalScalarState state = {
        .hands = g_game_vars.hands,
        .discards = g_game_vars.discards,
        .hand_size = g_game_vars.hand_size,
        .money = g_game_vars.money,
        .cards_to_draw = 0,
        .blind_requirement = alchemy_blind_requirement
    };
    if (!alchemical_apply_scalar(id, &state))
        return false;
    g_game_vars.hands = min(99, max(0, state.hands));
    g_game_vars.discards = min(99, max(0, state.discards));
    g_game_vars.money = max(0, state.money);
    alchemy_blind_requirement = max(1, state.blind_requirement);
    if (id == ALCHEMICAL_TERRA)
        display_alchemy_blind_requirement();
    int old_hand_size = g_game_vars.hand_size;
    g_game_vars.hand_size = min(MAX_HAND_SIZE, max(1, state.hand_size));
    if (id == ALCHEMICAL_QUICKSILVER)
        alchemy_temporary_hand_size += g_game_vars.hand_size - old_hand_size;
    if (cards_to_draw != NULL)
        *cards_to_draw = min(MAX_HAND_SIZE, state.cards_to_draw);
    return true;
}

static int game_alchemy_most_common_suit(void)
{
    int counts[NUM_SUITS] = {0};
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= deck_top; i++)
        if (card_has_rank(deck[i]) && deck[i]->suit < NUM_SUITS)
            counts[deck[i]->suit]++;
    for (int i = 0; i <= discard_top; i++)
        if (card_has_rank(discard_pile[i]) &&
            discard_pile[i]->suit < NUM_SUITS)
            counts[discard_pile[i]->suit]++;
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && card_has_rank(hand[i]->card) &&
            hand[i]->card->suit < NUM_SUITS)
            counts[hand[i]->card->suit]++;
    for (int i = 0; i <= played_top; i++)
        if (played[i] != NULL && card_has_rank(played[i]->card) &&
            played[i]->card->suit < NUM_SUITS)
            counts[played[i]->card->suit]++;
    for (int i = 0; i < alchemy_acid_count; i++)
        if (card_has_rank(alchemy_acid_cards[i]) &&
            alchemy_acid_cards[i]->suit < NUM_SUITS)
            counts[alchemy_acid_cards[i]->suit]++;
    int best = alchemical_most_common_index(counts, NUM_SUITS);
    return best >= 0 ? best : DIAMONDS;
}

static int game_alchemy_count_accessible_cards_outside_rank(int rank)
{
    int count = 0;
    CardObject** hand = get_hand_array();
    for (int i = 0; i <= get_hand_top(); i++)
        if (hand[i] != NULL && hand[i]->card != NULL &&
            (!card_has_rank(hand[i]->card) || hand[i]->card->rank != rank))
            count++;
    for (int i = 0; i <= deck_top; i++)
        if (deck[i] != NULL &&
            (!card_has_rank(deck[i]) || deck[i]->rank != rank))
            count++;
    return count;
}

static bool game_alchemy_has_unmuted_joker(void)
{
    ListItr itr = list_itr_create(&_owned_jokers_list);
    JokerObject* joker = NULL;
    while ((joker = list_itr_next(&itr)))
    {
        if (joker->joker != NULL && joker->sprite_object != NULL &&
            !game_is_joker_disabled(joker))
            return true;
    }
    return false;
}

static bool game_alchemy_left_joker_can_retrigger(void)
{
    JokerObject* joker = list_get_at_idx(&_owned_jokers_list, 0);
    if (joker == NULL || joker->joker == NULL ||
        game_is_joker_disabled(joker))
        return false;

    bool passive =
        joker->joker->id == PAREIDOLIA_JOKER_ID ||
        joker->joker->id == SHORTCUT_JOKER_ID ||
        joker->joker->id == FOUR_FINGERS_JOKER_ID;
    bool scoring_edition =
        joker->joker->modifier > BASE_EDITION &&
        joker->joker->modifier < NEGATIVE_EDITION;
    return !passive || scoring_edition;
}

/*
 * A consumable must never disappear when its complete result would be a no-op.
 * Keep these checks next to the live game state rather than in alchemy.c, whose
 * scalar API intentionally knows nothing about the hand, deck, or card flags.
 */
static bool game_alchemy_effect_would_change(enum AlchemicalId id)
{
    Card* selected = game_alchemy_get_selected_card();
    switch (id)
    {
        case ALCHEMICAL_IGNIS:
            if (g_game_vars.discards < 99)
                return true;
            break;
        case ALCHEMICAL_AQUA:
            if (g_game_vars.hands < 99)
                return true;
            break;
        case ALCHEMICAL_TERRA:
            if (alchemy_blind_requirement > 1)
                return true;
            break;
        case ALCHEMICAL_AERO:
            if (deck_top >= 0 && hand_nb_held_cards() < MAX_HAND_SIZE)
            {
                return true;
            }
            game_alchemy_set_feedback(
                deck_top < 0 ? "No card left to draw" : "Hand already at max"
            );
            return false;
        case ALCHEMICAL_QUICKSILVER:
            if (alchemical_can_expand_hand(g_game_vars.hand_size, 2, MAX_HAND_SIZE))
                return true;
            game_alchemy_set_feedback("Needs 2 size capacity");
            return false;
        case ALCHEMICAL_SALT:
            if (g_game_vars.money < INT_MAX)
                return true;
            break;
        case ALCHEMICAL_SULFUR:
            if (g_game_vars.hands > 1)
                return true;
            break;
        case ALCHEMICAL_PHOSPHORUS:
            if (discard_top >= 0)
                return true;
            break;
        case ALCHEMICAL_BISMUTH:
        {
            CardObject** hand = get_hand_array();
            for (int i = 0; i <= get_hand_top(); i++)
                if (hand[i] != NULL && hand[i]->card != NULL &&
                    card_object_is_selected(hand[i]) &&
                    !(hand[i]->card->boss_flags & CARD_BOSS_DEBUFFED) &&
                    !(hand[i]->card->alchemy_flags & CARD_ALCHEMY_POLY) &&
                    hand[i]->card->edition != CARD_EDITION_POLYCHROME)
                    return true;
            game_alchemy_set_feedback("Needs active non-Poly card");
            return false;
            break;
        }
        case ALCHEMICAL_ARSENIC:
            if (g_game_vars.hands != g_game_vars.discards)
                return true;
            break;
        case ALCHEMICAL_SOAP:
            if (deck_top + 1 >= hand_get_nb_selected_cards())
                return true;
            game_alchemy_set_feedback("Not enough deck cards");
            return false;
        case ALCHEMICAL_MANGANESE:
            if (game_alchemy_count_selected_without_flag(CARD_ALCHEMY_STEEL, true) > 0)
                return true;
            game_alchemy_set_feedback("Needs clean active card");
            return false;
        case ALCHEMICAL_WAX:
            if (deck_get_max_size() <= MAX_CARDS - 2 &&
                alchemical_wax_hand_capacity(
                    g_game_vars.hand_size,
                    hand_nb_held_cards(),
                    MAX_HAND_SIZE
                ) >= 0)
            {
                return true;
            }
            game_alchemy_set_feedback(
                deck_get_max_size() > MAX_CARDS - 2
                    ? "Needs 2 card slots"
                    : "Hand hard limit reached"
            );
            return false;
        case ALCHEMICAL_BORAX:
        {
            int suit = game_alchemy_most_common_suit();
            CardObject** hand = get_hand_array();
            for (int i = 0; i <= get_hand_top(); i++)
                if (hand[i] != NULL && hand[i]->card != NULL &&
                    card_object_is_selected(hand[i]) &&
                    card_has_rank(hand[i]->card) &&
                    hand[i]->card->enhancement != CARD_ENHANCEMENT_WILD &&
                    hand[i]->card->suit != suit)
                {
                    return true;
                }
            break;
        }
        case ALCHEMICAL_GLASS:
            if (game_alchemy_count_selected_without_flag(CARD_ALCHEMY_GLASS, true) > 0)
                return true;
            game_alchemy_set_feedback("Needs clean active card");
            return false;
        case ALCHEMICAL_MAGNET:
            if (selected == NULL || !card_has_rank(selected))
            {
                game_alchemy_set_feedback("Select a ranked card");
                return false;
            }
            if (hand_nb_held_cards() < MAX_HAND_SIZE)
            {
                for (int i = 0; i <= deck_top; i++)
                    if (deck[i] != NULL && deck[i]->rank == selected->rank)
                        return true;
            }
            game_alchemy_set_feedback("No matching card/space");
            return false;
        case ALCHEMICAL_GOLD:
            if (game_alchemy_count_selected_without_flag(CARD_ALCHEMY_GOLD, false) > 0)
                return true;
            game_alchemy_set_feedback("Needs clean active card");
            return false;
            break;
        case ALCHEMICAL_SILVER:
            if (game_alchemy_count_selected_without_flag(CARD_ALCHEMY_LUCKY, true) > 0)
                return true;
            game_alchemy_set_feedback("Needs clean active card");
            return false;
        case ALCHEMICAL_OIL:
        {
            CardObject** hand = get_hand_array();
            for (int i = 0; i <= get_hand_top(); i++)
                if (hand[i] != NULL && hand[i]->card != NULL &&
                    (hand[i]->card->boss_flags &
                     (CARD_BOSS_DEBUFFED | CARD_BOSS_FACE_DOWN)))
                    return true;
            break;
        }
        case ALCHEMICAL_ACID:
        {
            if (selected == NULL || !card_has_rank(selected))
            {
                game_alchemy_set_feedback("Select a ranked card");
                return false;
            }
            int remaining =
                game_alchemy_count_accessible_cards_outside_rank(selected->rank);
            int required = boss_blind_active(BLIND_TYPE_PSYCHIC) ? 5 : 1;
            if (alchemical_acid_can_leave_playable(remaining, required))
            {
                return true;
            }
            game_alchemy_set_feedback(
                boss_blind_active(BLIND_TYPE_PSYCHIC)
                    ? "Psychic needs 5 cards"
                    : "Would leave no cards"
            );
            return false;
        }
        case ALCHEMICAL_BRIMSTONE:
            if (alchemical_can_add_bounded(g_game_vars.hands, 2, 99) &&
                alchemical_can_add_bounded(g_game_vars.discards, 2, 99) &&
                alchemy_debuffed_joker_count < ALCHEMICAL_HELD_LIMIT &&
                game_alchemy_has_unmuted_joker())
            {
                return true;
            }
            game_alchemy_set_feedback(
                !game_alchemy_has_unmuted_joker()
                    ? "Needs active Joker"
                    : "Hands/discards capped"
            );
            return false;
            break;
        case ALCHEMICAL_COBALT:
        {
            enum HandType type = get_hand_type();
            if (type > NONE && type < ALCHEMICAL_HAND_TYPE_COUNT &&
                g_game_vars.alchemy.hand_levels[type] < 20)
            {
                return true;
            }
            break;
        }
        case ALCHEMICAL_ANTIMONY:
            if (alchemy_antimony_retriggers < ALCHEMICAL_HELD_LIMIT)
                return true;
            break;
        case ALCHEMICAL_URANIUM:
            if (selected != NULL)
            {
                CardObject** hand = get_hand_array();
                for (int i = 0; i <= get_hand_top(); i++)
                    if (hand[i] != NULL && hand[i]->card != NULL &&
                        hand[i]->card != selected &&
                        alchemical_copy_target_is_clean(
                            hand[i]->card->alchemy_flags,
                            hand[i]->card->enhancement,
                            hand[i]->card->edition,
                            hand[i]->card->seal
                        ))
                    {
                        return true;
                    }
            }
            game_alchemy_set_feedback("No clean copy target");
            return false;
        default:
            return false;
    }

    game_alchemy_set_feedback("Effect already active");
    return false;
}

static void game_alchemy_remove_rank_from_array(Card** cards, int* top, int rank)
{
    if (cards == NULL || top == NULL)
        return;
    for (int i = *top; i >= 0 && alchemy_acid_count < ALCHEMY_ACID_CAPACITY; i--)
    {
        if (!card_has_rank(cards[i]) || cards[i]->rank != rank)
            continue;
        alchemy_acid_cards[alchemy_acid_count++] = cards[i];
        for (int j = i; j < *top; j++)
            cards[j] = cards[j + 1];
        cards[(*top)--] = NULL;
    }
}

static bool game_alchemy_apply_complex(enum AlchemicalId id)
{
    Card* selected = game_alchemy_get_selected_card();
    CardObject** hand = get_hand_array();
    const AlchemicalInfo* info = alchemical_get_info(id);
    if (info == NULL || (info->min_selected > 0 && selected == NULL))
        return false;

    switch (id)
    {
        case ALCHEMICAL_PHOSPHORUS:
            while (discard_top >= 0 && deck_top < MAX_DECK_SIZE - 1)
                deck_push(discard_pop());
            deck_shuffle();
            return true;
        case ALCHEMICAL_BISMUTH:
        {
            int changed = 0;
            for (int i = 0; i <= get_hand_top(); i++)
            {
                if (hand[i] == NULL || hand[i]->card == NULL ||
                    !card_object_is_selected(hand[i]) ||
                    (hand[i]->card->boss_flags & CARD_BOSS_DEBUFFED) ||
                    hand[i]->card->edition == CARD_EDITION_POLYCHROME ||
                    (hand[i]->card->alchemy_flags & CARD_ALCHEMY_POLY))
                    continue;
                hand[i]->card->alchemy_flags |= CARD_ALCHEMY_POLY;
                changed++;
            }
            if (changed > 0)
                reorder_card_sprites_layers();
            return changed > 0;
        }
        case ALCHEMICAL_COBALT:
        {
            enum HandType type = get_hand_type();
            if (type <= NONE || type >= ALCHEMICAL_HAND_TYPE_COUNT)
                return false;
            return alchemical_level_hand(
                &g_game_vars.alchemy.hand_levels[type],
                2,
                20
            );
        }
        case ALCHEMICAL_ANTIMONY:
        {
            JokerObject* target = list_get_at_idx(&_owned_jokers_list, 0);
            if (target == NULL ||
                alchemy_antimony_retriggers >= ALCHEMICAL_HELD_LIMIT)
                return false;
            /*
             * Preserve the target of every use. If Jokers are reordered
             * between two Antimony cards, earlier retriggers must not migrate
             * to the newest left Joker.
             */
            alchemy_antimony_jokers[alchemy_antimony_retriggers++] = target;
            return true;
        }
        case ALCHEMICAL_SOAP:
        {
            Card* replaced[3] = {NULL};
            int replaced_count = 0;
            for (int i = get_hand_top(); i >= 0; i--)
                if (hand[i] != NULL && card_object_is_selected(hand[i]))
                    replaced[replaced_count++] = game_alchemy_remove_hand_card(i);
            hand_set_nb_selected_cards(0);
            deck_shuffle();
            int drawn =
                game_alchemy_draw_cards(replaced_count, UNDEFINED, MAX_HAND_SIZE);
            for (int i = 0; i < replaced_count; i++)
                if (replaced[i] != NULL && !deck_push(replaced[i]))
                    card_destroy(&replaced[i]);
            deck_shuffle();
            /*
             * If the fixed CardObject pool is exhausted, drawing can become
             * partial after the selected cards already moved into the deck.
             * The state did change, so consume Soap instead of allowing the
             * same card to be reused against a partially replaced hand.
             */
            (void)drawn;
            return replaced_count > 0;
        }
        case ALCHEMICAL_MANGANESE:
            return game_alchemy_apply_flag_to_selected(CARD_ALCHEMY_STEEL, true) > 0;
        case ALCHEMICAL_WAX:
        {
            int expanded_hand_size = alchemical_wax_hand_capacity(
                g_game_vars.hand_size,
                hand_nb_held_cards(),
                MAX_HAND_SIZE
            );
            if (expanded_hand_size < 0)
                return false;

            Card* copies[2] = {NULL};
            CardObject* objects[2] = {NULL};
            bool ready = selected != NULL;
            for (int i = 0; i < 2 && ready; i++)
            {
                copies[i] = card_new(selected->suit, selected->rank);
                if (copies[i] == NULL)
                {
                    ready = false;
                    break;
                }
                copies[i]->alchemy_flags =
                    selected->alchemy_flags | CARD_ALCHEMY_TEMP_COPY;
                copies[i]->alchemy_original_suit = selected->alchemy_original_suit;
                copies[i]->enhancement = selected->enhancement;
                copies[i]->edition = selected->edition;
                copies[i]->seal = selected->seal;
                copies[i]->boss_flags =
                    selected->boss_flags &
                    (CARD_BOSS_DEBUFFED | CARD_BOSS_FACE_DOWN);
                boss_refresh_card(copies[i]);
                objects[i] = card_object_new(copies[i]);
                if (objects[i] == NULL || objects[i]->sprite_object == NULL)
                    ready = false;
            }
            if (!ready)
            {
                for (int i = 0; i < 2; i++)
                {
                    if (objects[i] != NULL)
                        card_object_destroy(&objects[i]);
                    if (copies[i] != NULL)
                        card_destroy(&copies[i]);
                }
                return false;
            }

            int hand_size_increase = expanded_hand_size - g_game_vars.hand_size;
            if (hand_size_increase > 0)
            {
                g_game_vars.hand_size = expanded_hand_size;
                alchemy_temporary_hand_size += hand_size_increase;
            }

            int top = get_hand_top();
            for (int i = 0; i < 2; i++)
            {
                sprite_object_position(objects[i]->sprite_object, CARD_DRAW_POS.x, CARD_DRAW_POS.y);
                hand[top + 1 + i] = objects[i];
            }
            set_hand_top(top + 2);
            sort_cards();
            game_alchemy_recount_selection();
            return true;
        }
        case ALCHEMICAL_BORAX:
        {
            int suit = game_alchemy_most_common_suit();
            int changed = 0;
            for (int i = 0; i <= get_hand_top(); i++)
            {
                if (hand[i] == NULL || hand[i]->card == NULL ||
                    !card_object_is_selected(hand[i]))
                    continue;
                Card* card = hand[i]->card;
                if (!card_has_rank(card) ||
                    card->enhancement == CARD_ENHANCEMENT_WILD ||
                    card->suit == suit)
                    continue;
                if (!(card->alchemy_flags & CARD_ALCHEMY_BORAX))
                    card->alchemy_original_suit = card->suit;
                card->alchemy_flags |= CARD_ALCHEMY_BORAX;
                card->suit = suit;
                boss_refresh_card(card);
                changed++;
            }
            sort_cards();
            return changed > 0;
        }
        case ALCHEMICAL_GLASS:
            return game_alchemy_apply_flag_to_selected(CARD_ALCHEMY_GLASS, true) > 0;
        case ALCHEMICAL_MAGNET:
            return selected != NULL &&
                   game_alchemy_draw_cards(2, selected->rank, MAX_HAND_SIZE) > 0;
        case ALCHEMICAL_GOLD:
            return game_alchemy_apply_flag_to_selected(CARD_ALCHEMY_GOLD, false) > 0;
        case ALCHEMICAL_SILVER:
            return game_alchemy_apply_flag_to_selected(CARD_ALCHEMY_LUCKY, true) > 0;
        case ALCHEMICAL_OIL:
        {
            int changed = 0;
            for (int i = 0; i <= get_hand_top(); i++)
                if (hand[i] != NULL && hand[i]->card != NULL)
                {
                    u8 removable =
                        hand[i]->card->boss_flags &
                        (CARD_BOSS_DEBUFFED | CARD_BOSS_FACE_DOWN);
                    if (removable != 0)
                    {
                        changed++;
                        hand[i]->card->alchemy_flags |= CARD_ALCHEMY_OILED;
                        hand[i]->card->boss_flags &=
                            (u8)~(CARD_BOSS_DEBUFFED | CARD_BOSS_FACE_DOWN);
                    }
                }
            if (changed > 0)
                reorder_card_sprites_layers();
            return changed > 0;
        }
        case ALCHEMICAL_ACID:
        {
            int rank = selected->rank;
            for (int i = get_hand_top(); i >= 0; i--)
                if (hand[i] != NULL && hand[i]->card != NULL &&
                    card_has_rank(hand[i]->card) &&
                    hand[i]->card->rank == rank &&
                    alchemy_acid_count < ALCHEMY_ACID_CAPACITY)
                    alchemy_acid_cards[alchemy_acid_count++] = game_alchemy_remove_hand_card(i);
            game_alchemy_remove_rank_from_array(deck, &deck_top, rank);
            game_alchemy_remove_rank_from_array(discard_pile, &discard_top, rank);
            hand_set_nb_selected_cards(0);
            if (get_hand_top() >= 0)
                sort_cards();
            game_playing_selection_grid.selection.x = 0;
            int held_cards = hand_nb_held_cards();
            if (held_cards < g_game_vars.hand_size && deck_top >= 0)
            {
                /*
                 * Acid can remove several visible cards at once. Refill the
                 * partial hand, not only an empty one: otherwise The Psychic
                 * can leave the player with fewer than the five cards it
                 * requires and no legal action despite cards remaining.
                 */
                set_hand_state(HAND_DRAW);
                cards_drawn = held_cards;
                boss_draw_count = UNDEFINED;
            }
            else if (get_hand_top() < 0)
            {
                game_playing_selection_grid.selection =
                    (Selection){0, GAME_PLAYING_BUTTONS_SEL_Y};
            }
            return true;
        }
        case ALCHEMICAL_BRIMSTONE:
        {
            if (alchemy_debuffed_joker_count >= ALCHEMICAL_HELD_LIMIT)
                return false;
            ListItr itr = list_itr_create(&_owned_jokers_list);
            JokerObject* joker = NULL;
            while ((joker = list_itr_next(&itr)))
            {
                if (joker->joker != NULL && joker->sprite_object != NULL &&
                    !game_is_joker_disabled(joker))
                {
                    alchemy_debuffed_jokers[alchemy_debuffed_joker_count++] = joker;
                    if (boss_blind_active(BLIND_TYPE_PLANT))
                        boss_refresh_hand();
                    return true;
                }
            }
            return false;
        }
        case ALCHEMICAL_URANIUM:
        {
            int copied = 0;
            for (int i = 0; i <= get_hand_top() && copied < 3; i++)
            {
                if (hand[i] == NULL || hand[i]->card == NULL)
                    continue;
                Card* target = hand[i]->card;
                if (target == selected ||
                    !alchemical_copy_target_is_clean(
                        target->alchemy_flags,
                        target->enhancement,
                        target->edition,
                        target->seal
                    ))
                    continue;
                if (!game_alchemy_backup_uranium_card(target))
                    break;

                u8 target_original_suit = target->suit;
                target->alchemy_original_suit = target->suit;
                target->alchemy_flags =
                    selected->alchemy_flags & (u8)~CARD_ALCHEMY_TEMP_COPY;
                target->enhancement = selected->enhancement;
                target->edition = selected->edition;
                target->seal = selected->seal;
                if (target->alchemy_flags & CARD_ALCHEMY_BORAX)
                {
                    target->suit = selected->suit;
                    target->alchemy_original_suit = target_original_suit;
                }
                if (target->alchemy_flags & CARD_ALCHEMY_OILED)
                    target->boss_flags &=
                        (u8)~(CARD_BOSS_DEBUFFED | CARD_BOSS_FACE_DOWN);
                boss_refresh_card(target);
                copied++;
            }
            sort_cards();
            return copied > 0;
        }
        default:
            return false;
    }
}

static bool game_alchemy_use_selected(void)
{
    if (alchemy_selected_slot < 0 || alchemy_selected_slot >= g_game_vars.alchemy.count)
        return false;
    game_alchemy_recount_selection();
    enum AlchemicalId id = g_game_vars.alchemy.held[alchemy_selected_slot];
    const AlchemicalInfo* info = alchemical_get_info(id);
    if (info == NULL)
    {
        game_alchemy_set_feedback("Invalid consumable");
        return false;
    }
    int selected_count = hand_get_nb_selected_cards();
    bool has_joker = !list_is_empty(&_owned_jokers_list);
    if (!alchemical_can_use(id, true, selected_count, has_joker))
    {
        if (info->requires_joker && !has_joker)
        {
            game_alchemy_set_feedback("Needs a Joker");
        }
        else
        {
            char condition[24];
            if (info->min_selected == info->max_selected)
                snprintf(condition, sizeof(condition), "Select %d card", info->min_selected);
            else
                snprintf(
                    condition,
                    sizeof(condition),
                    "Select %d-%d cards",
                    info->min_selected,
                    info->max_selected
                );
            game_alchemy_set_feedback(condition);
        }
        return false;
    }

    /*
     * Do not consume a card when the current state makes its result a no-op.
     * This is especially important for Arsenic at the start of a blind, where
     * hands and discards are commonly equal and a successful-looking swap
     * would otherwise change nothing.
     */
    if (id == ALCHEMICAL_ARSENIC && g_game_vars.hands == g_game_vars.discards)
    {
        game_alchemy_set_feedback("Hands/discards equal");
        return false;
    }
    if (id == ALCHEMICAL_PHOSPHORUS && discard_top < 0)
    {
        game_alchemy_set_feedback("Discard pile empty");
        return false;
    }
    if (id == ALCHEMICAL_WAX &&
        (deck_get_max_size() > MAX_CARDS - 2 ||
         alchemical_wax_hand_capacity(
             g_game_vars.hand_size,
             hand_nb_held_cards(),
             MAX_HAND_SIZE
         ) < 0))
    {
        game_alchemy_set_feedback(
            deck_get_max_size() > MAX_CARDS - 2
                ? "Needs 2 card slots"
                : "Hand hard limit reached"
        );
        return false;
    }
    if (id == ALCHEMICAL_URANIUM)
    {
        Card* selected = game_alchemy_get_selected_card();
        u8 copyable_flags =
            CARD_ALCHEMY_POLY | CARD_ALCHEMY_STEEL | CARD_ALCHEMY_GLASS |
            CARD_ALCHEMY_GOLD | CARD_ALCHEMY_LUCKY | CARD_ALCHEMY_OILED |
            CARD_ALCHEMY_BORAX;
        if (selected == NULL ||
            ((selected->alchemy_flags & copyable_flags) == 0 &&
             selected->enhancement == CARD_ENHANCEMENT_NONE &&
             selected->edition == CARD_EDITION_NONE &&
             selected->seal == CARD_SEAL_NONE))
        {
            game_alchemy_set_feedback("Card has no modifier");
            return false;
        }
    }
    if (id == ALCHEMICAL_ANTIMONY && !game_alchemy_left_joker_can_retrigger())
    {
        game_alchemy_set_feedback("Left Joker is passive");
        return false;
    }
    if (!game_alchemy_effect_would_change(id))
        return false;

    int cards_to_draw = 0;
    int drawn_count = 0;
    bool applied;
    if (id == ALCHEMICAL_BRIMSTONE)
    {
        /*
         * Brimstone is a compound bargain: mute the Joker before committing
         * the +2 resources, so a late target failure cannot grant half of an
         * unconsumed effect.
         */
        applied = game_alchemy_apply_complex(id);
        if (applied)
            applied = game_alchemy_sync_scalar(id, &cards_to_draw);
    }
    else
    {
        applied = game_alchemy_sync_scalar(id, &cards_to_draw);
        if (!applied)
            applied = game_alchemy_apply_complex(id);
    }
    if (!applied)
    {
        game_alchemy_set_feedback("No valid target");
        return false;
    }
    if (id == ALCHEMICAL_AERO)
    {
        drawn_count = game_alchemy_draw_cards(cards_to_draw, UNDEFINED, MAX_HAND_SIZE);
        if (drawn_count <= 0)
        {
            game_alchemy_set_feedback("Could not draw cards");
            return false;
        }
    }
    else if (id == ALCHEMICAL_QUICKSILVER)
        drawn_count =
            game_alchemy_draw_cards(
                g_game_vars.hand_size - hand_nb_held_cards(),
                UNDEFINED,
                g_game_vars.hand_size
            );
    else if (id == ALCHEMICAL_PHOSPHORUS)
    {
        /*
         * If the normal draw stopped because the deck was empty, restoring the
         * discard pile must also resume that interrupted draw.  Without this,
         * Phosphorus could be the only legal rescue yet leave an empty hand
         * and immediately lose the blind.
         */
        drawn_count =
            game_alchemy_draw_cards(
                g_game_vars.hand_size - hand_nb_held_cards(),
                UNDEFINED,
                g_game_vars.hand_size
            );
    }

    game_alchemy_animate_selected();

    int removed_slot = alchemy_selected_slot;
    alchemical_inventory_remove(&g_game_vars.alchemy, removed_slot);
    if (alchemy_selected_slot >= g_game_vars.alchemy.count)
        alchemy_selected_slot = max(0, g_game_vars.alchemy.count - 1);
    alchemy_focus = false;
    game_alchemy_refresh_round_objects_after_use(removed_slot);
    game_alchemy_show_detail();
    display_hands();
    display_discards();
    display_money();
    display_deck_size_max();
    game_alchemy_recount_selection();
    compute_hand_value_info();
    boss_ensure_forced_card();
    game_alchemy_set_success_feedback(id, info, selected_count, drawn_count);
    if (id == ALCHEMICAL_TERRA && g_game_vars.score >= alchemy_blind_requirement)
    {
        /*
         * Terra may lower the target below the score already earned. Finish via
         * the normal shuffling path so temporary effects and every card object
         * are cleaned by the same code as a hand-earned blind victory.
         */
        game_capture_round_end_card_modifiers();
        hand_deselect_all_cards();
        hand_set_nb_selected_cards(0);
        set_hand_state(HAND_SHUFFLING);
        g_game_vars.timer = TM_ZERO;
    }
    return true;
}

static void game_alchemy_reset_card(Card* card)
{
    if (card == NULL)
        return;
    if (card->alchemy_flags & CARD_ALCHEMY_BORAX)
        card->suit = card->alchemy_original_suit;
    game_alchemy_remove_uranium_backup(card, true);
    card->alchemy_flags = 0;
    card->alchemy_original_suit = card->suit;
    card->boss_flags &= CARD_BOSS_PLAYED_ANTE;
    if (g_game_vars.current_blind >= BLIND_TYPE_BOSS)
        card->boss_flags = 0;
}

static void game_alchemy_clean_card_array(Card** cards, int* top)
{
    if (cards == NULL || top == NULL)
        return;
    for (int i = *top; i >= 0; i--)
    {
        Card* card = cards[i];
        if (card == NULL)
        {
            for (int j = i; j < *top; j++)
                cards[j] = cards[j + 1];
            cards[(*top)--] = NULL;
            continue;
        }
        if (card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY)
        {
            for (int j = i; j < *top; j++)
                cards[j] = cards[j + 1];
            cards[(*top)--] = NULL;
            card_destroy(&card);
        }
        else
        {
            game_alchemy_reset_card(card);
        }
    }
}

static int game_collect_boss_reward_cards(Card** eligible, int category)
{
    if (eligible == NULL || category < 0 || category > 2)
        return 0;

    int eligible_count = 0;
    CardObject** hand = get_hand_array();
    for (int location = 0; location < 3; location++)
    {
        int top = location == 0 ? deck_top
                  : location == 1 ? discard_top
                                  : get_hand_top();
        for (int i = 0; i <= top && eligible_count < MAX_DECK_SIZE; i++)
        {
            Card* card = location == 0 ? deck[i]
                         : location == 1 ? discard_pile[i]
                                         : (hand[i] != NULL ? hand[i]->card : NULL);
            if (card == NULL || (card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY))
                continue;
            bool missing_modifier =
                (category == 0 && card->enhancement == CARD_ENHANCEMENT_NONE) ||
                (category == 1 && card->edition == CARD_EDITION_NONE) ||
                (category == 2 && card->seal == CARD_SEAL_NONE);
            if (missing_modifier)
                eligible[eligible_count++] = card;
        }
    }
    return eligible_count;
}

static void game_award_boss_card_modifier(void)
{
    boss_card_reward[0] = '\0';
    if (g_game_vars.current_blind <= BLIND_TYPE_BIG ||
        g_game_vars.score < alchemy_blind_requirement)
        return;

    /*
     * Tarot is intentionally not part of this build yet. Give one permanent
     * modifier after each defeated Boss so the playing-card modifier system is
     * available in a normal run without consuming the shared
     * Planet/Alchemical Shop slot.
     */
    int category_roll = rng_get_u32() % 100;
    int first_category =
        category_roll < 60 ? 0 : category_roll < 85 ? 2 : 1;
    for (int attempt = 0; attempt < 3; attempt++)
    {
        int category = (first_category + attempt) % 3;
        Card* eligible[MAX_DECK_SIZE];
        int eligible_count = game_collect_boss_reward_cards(eligible, category);
        if (eligible_count == 0)
            continue;

        Card* card = eligible[rng_get_u32() % eligible_count];
        const char* modifier_name = NULL;
        if (category == 0)
        {
            static const u8 safe_enhancements[] = {
                CARD_ENHANCEMENT_BONUS,
                CARD_ENHANCEMENT_MULT,
                CARD_ENHANCEMENT_STEEL,
                CARD_ENHANCEMENT_GOLD,
                CARD_ENHANCEMENT_LUCKY
            };
            card->enhancement =
                safe_enhancements[rng_get_u32() % NUM_ELEM_IN_ARR(safe_enhancements)];
            modifier_name = card_get_enhancement_name(card->enhancement);
        }
        else if (category == 1)
        {
            int edition_roll = rng_get_u32() % 100;
            card->edition = edition_roll < 50
                              ? CARD_EDITION_FOIL
                              : edition_roll < 85 ? CARD_EDITION_HOLOGRAPHIC
                                                  : CARD_EDITION_POLYCHROME;
            modifier_name = card_get_edition_name(card->edition);
        }
        else
        {
            card->seal = 1 + rng_get_u32() % (CARD_SEAL_COUNT - 1);
            modifier_name = card_get_seal_name(card->seal);
        }
        static const char* const rank_codes[NUM_RANKS] = {
            "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
        };
        static const char suit_codes[NUM_SUITS] = {'D', 'C', 'H', 'S'};
        snprintf(
            boss_card_reward,
            sizeof(boss_card_reward),
            "%s%c +%s",
            rank_codes[card->rank],
            suit_codes[card->suit],
            modifier_name != NULL ? modifier_name : "modifier"
        );
        return;
    }
}

static void game_alchemy_end_blind(void)
{
    if (round_end_gold_cards > 0)
        g_game_vars.money =
            round_end_gold_cards > (INT_MAX - g_game_vars.money) / 3
              ? INT_MAX
              : g_game_vars.money + round_end_gold_cards * 3;
    if (round_end_blue_seals > 0 && last_played_hand_type > NONE &&
        last_played_hand_type < ALCHEMICAL_HAND_TYPE_COUNT)
    {
        /*
         * Planets are immediate Shop upgrades rather than held consumables, so
         * Blue Seals apply the same hand-level reward directly. Tying the
         * reward to free Alchemical slots made a full inventory silently
         * disable the Seal.
         */
        int level_grants = round_end_blue_seals;
        g_game_vars.alchemy.hand_levels[last_played_hand_type] =
            min(20,
                g_game_vars.alchemy.hand_levels[last_played_hand_type] +
                    level_grants);
    }
    g_game_vars.money = max(0, g_game_vars.money);
    if (round_end_alchemy_gold_cards > 0)
    {
        int reward =
            round_end_alchemy_gold_cards > (INT_MAX - g_game_vars.money) / 2
                       ? INT_MAX
                       : g_game_vars.money + round_end_alchemy_gold_cards * 2;
        g_game_vars.money = max(0, reward);
    }

    CardObject** hand = get_hand_array();
    for (int i = get_hand_top(); i >= 0; i--)
    {
        if (hand[i] == NULL || hand[i]->card == NULL)
            continue;
        Card* card = hand[i]->card;
        if (card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY)
        {
            card = game_alchemy_remove_hand_card(i);
            card_destroy(&card);
        }
        else
        {
            game_alchemy_reset_card(card);
        }
    }
    game_alchemy_clean_card_array(deck, &deck_top);
    game_alchemy_clean_card_array(discard_pile, &discard_top);

    for (int i = alchemy_acid_count - 1; i >= 0; i--)
    {
        Card* card = alchemy_acid_cards[i];
        alchemy_acid_cards[i] = NULL;
        if (card == NULL)
            continue;
        if (card->alchemy_flags & CARD_ALCHEMY_TEMP_COPY)
            card_destroy(&card);
        else
        {
            game_alchemy_reset_card(card);
            if (!deck_push(card) && !discard_push(card))
                card_destroy(&card);
        }
    }
    alchemy_acid_count = 0;
    memset(alchemy_uranium_backups, 0, sizeof(alchemy_uranium_backups));
    alchemy_uranium_backup_count = 0;
    deck_shuffle();
    game_award_boss_card_modifier();

    g_game_vars.hand_size =
        clamp(g_game_vars.hand_size + boss_hand_size_penalty -
                  alchemy_temporary_hand_size,
              1,
              MAX_HAND_SIZE);
    boss_hand_size_penalty = 0;
    boss_disabled_joker = NULL;
    boss_ox_hand_type = NONE;
    boss_verdant_active = false;
    boss_fish_next_draw_face_down = false;
    boss_acorn_hidden = false;
    /*
     * Do not depend on another GAME_STATE_PLAYING frame to undo a Boss visual.
     * Round End and the Shop render these same objects, so a Crimson Heart scale
     * or Amber Acorn hide would otherwise leak into the following screen.
     */
    ListItr joker_itr = list_itr_create(&_owned_jokers_list);
    JokerObject* owned_joker;
    while ((owned_joker = list_itr_next(&joker_itr)))
    {
        if (owned_joker == NULL || owned_joker->sprite_object == NULL)
            continue;
        owned_joker->sprite_object->tscale = FIX_ONE;
        if (owned_joker->sprite_object->sprite != NULL)
            obj_unhide(owned_joker->sprite_object->sprite->obj, ATTR0_AFF);
    }
    alchemy_temporary_hand_size = 0;
    memset(alchemy_antimony_jokers, 0, sizeof(alchemy_antimony_jokers));
    alchemy_antimony_retriggers = 0;
    memset(alchemy_debuffed_jokers, 0, sizeof(alchemy_debuffed_jokers));
    alchemy_debuffed_joker_count = 0;
    round_end_gold_cards = 0;
    round_end_blue_seals = 0;
    round_end_alchemy_gold_cards = 0;
    last_played_hand_type = NONE;
    alchemy_focus = false;
    alchemy_pending_use_slot = UNDEFINED;
    for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
        alchemical_object_destroy(&round_alchemicals[i]);
    tte_erase_rect_wrapper(ALCHEMY_DETAIL_RECT);
    tte_erase_rect_wrapper(ALCHEMY_FEEDBACK_RECT);
}

static inline void game_playing_handle_round_over(void)
{
    enum GameState next_state = GAME_STATE_ROUND_END;

    if (g_game_vars.score >= alchemy_blind_requirement)
    {
        if (g_game_vars.current_blind > BLIND_TYPE_BIG)
        {
            if (g_game_vars.ante < MAX_ANTE)
            {
                g_game_vars.ante++;
                display_ante();

                // mark current boss blind as beaten and allow for reroll
                set_blind_beaten(g_game_vars.next_boss_blind);
            }
            else
            {
                next_state = GAME_STATE_WIN;
            }
        }
    }
    else
    {
        /*
         * Every caller reaches this function because the blind can no longer
         * continue. Never cash out a below-target round merely because a new
         * exhaustion guard, boss rule, or resource fallback ended it before
         * the ordinary hands==0 path.
         */
        next_state = GAME_STATE_LOSE;
    }

    game_alchemy_end_blind();
    game_change_state(next_state);
}

static inline void card_in_hand_loop_handle_discard_and_shuffling(
    int card_idx,
    FIXED* hand_x,
    FIXED* hand_y,
    bool* break_loop
)
{
    if (get_hand_state() != HAND_DISCARD && get_hand_state() != HAND_SHUFFLING)
    {
        // Assumes hand_state is one of these
        return;
    }

    CardObject** hand = get_hand_array();

    *break_loop = false;
    if (card_object_is_selected(hand[card_idx]) || get_hand_state() == HAND_SHUFFLING)
    {
        if (!discarded_card)
        {
            *hand_x = int2fx(CARD_DISCARD_PNT.x);
            *hand_y = int2fx(CARD_DISCARD_PNT.y);

            if (!sound_played)
            {
                play_sfx(
                    SFX_CARD_DRAW,
                    MM_BASE_PITCH_RATE + cards_drawn * PITCH_STEP_DISCARD_SFX,
                    SFX_DEFAULT_VOLUME
                );
                sound_played = true;
            }

            if (hand[card_idx]->sprite_object->x >= *hand_x)
            {
                FIXED discarded_x = hand[card_idx]->sprite_object->x;
                FIXED discarded_y = hand[card_idx]->sprite_object->y;
                if (get_hand_state() == HAND_DISCARD)
                    game_card_modifier_on_discard(hand[card_idx]->card);
                discard_push(hand[card_idx]->card);
                card_object_destroy(&hand[card_idx]);
                reorder_card_sprites_layers();

                set_hand_top(get_hand_top() - 1);
                // This technically isn't drawing cards, I'm just reusing the variable
                cards_drawn++;
                sound_played = false;
                g_game_vars.timer = TM_ZERO;

                *hand_y = discarded_y;
                *hand_x = discarded_x;
            }

            discarded_card = true;
        }
        else
        {
            if (get_hand_state() == HAND_DISCARD)
            {
                // Don't raise the card if we're mass discarding, it looks stupid.
                *hand_y -= int2fx(15);
            }
            else // hand_state == HAND_SHUFFLING
            {
                *hand_y += int2fx(24);
            }
            *hand_x = *hand_x + (int2fx(card_idx) - int2fx(get_hand_top()) / 2) *
                                    -HAND_SPACING_LUT[get_hand_top()];
        }
    }
    else
    {
        *hand_x = *hand_x + (int2fx(card_idx) - int2fx(get_hand_top()) / 2) *
                                -HAND_SPACING_LUT[get_hand_top()];
    }

    if (card_idx == 0 && discarded_card == false && g_game_vars.timer % FRAMES(10) == 0)
    {
        // This is never reached in the case of HAND_SHUFFLING. Not sure why but that's how it's
        // supposed to be.
        set_hand_state(HAND_DRAW);
        sound_played = false;
        cards_drawn = 0;
        hand_set_nb_selected_cards(0);
        g_game_vars.timer = TM_ZERO;
        *break_loop = true;
        return;
    };
}

static inline void select_flush_and_straight_cards_in_played_hand(void)
{
    // Special handling because Four Fingers might be active
    bool final_selection[MAX_SELECTION_SIZE] = {false};
    const int played_count = clamp(played_top + 1, 0, MAX_SELECTION_SIZE);

    // Will be 4 if Four Fingers is in effect, otherwise 5
    int min_len = get_straight_and_flush_size();

    // if we have a flush in our hand
    if (get_hand_type() == FLUSH || get_hand_type() == STRAIGHT_FLUSH ||
        get_hand_type() == ROYAL_FLUSH)
    {
        bool flush_selection[MAX_HAND_SIZE] = {false};
        find_flush_in_played_cards(played, played_top, min_len, flush_selection);
        // Add the results into the final selection
        for (int i = 0; i < played_count; i++)
        {
            final_selection[i] = flush_selection[i];
        }
    }

    // If we have a straight in our hand
    if (get_hand_type() == STRAIGHT || get_hand_type() == STRAIGHT_FLUSH ||
        get_hand_type() == ROYAL_FLUSH)
    {
        bool straight_selection[MAX_HAND_SIZE] = {false};
        find_straight_in_played_cards(
            played,
            played_top,
            is_shortcut_joker_active(),
            min_len,
            straight_selection
        );
        // Add the results into the final selection
        for (int i = 0; i < played_count; i++)
        {
            final_selection[i] = final_selection[i] || straight_selection[i];
        }
        // If Four Fingers is active, pairs can happen in a valid straight
        // If Four Fingers is not active, pairs are impossible so this will not affect things
        select_paired_cards_in_hand(played, played_top, final_selection);
    }

    // Finally, set mark the cards as selected based final_selection
    for (int i = 0; i < played_count; i++)
    {
        if (final_selection[i])
        {
            card_object_set_selected(played[i], true);
        }
    }
}

static inline void select_all_five_cards_in_played_hand(void)
{
    for (int i = 0; i <= played_top; i++)
    {
        card_object_set_selected(played[i], true);
    }
}

static inline void select_four_of_a_kind_cards_in_played_hand(void)
{
    int counts[NUM_RANKS] = {0};
    for (int i = 0; i <= played_top; i++)
        if (card_has_rank(played[i]->card))
            counts[played[i]->card->rank]++;
    for (int rank = 0; rank < NUM_RANKS; rank++)
    {
        if (counts[rank] >= 4)
        {
            for (int i = 0; i <= played_top; i++)
                if (card_has_rank(played[i]->card) && played[i]->card->rank == rank)
                    card_object_set_selected(played[i], true);
            return;
        }
    }
}

static inline void select_three_of_a_kind_cards_in_played_hand(void)
{
    int counts[NUM_RANKS] = {0};
    for (int i = 0; i <= played_top; i++)
        if (card_has_rank(played[i]->card))
            counts[played[i]->card->rank]++;
    for (int rank = 0; rank < NUM_RANKS; rank++)
        if (counts[rank] >= 3)
        {
            int selected = 0;
            for (int i = 0; i <= played_top && selected < 3; i++)
                if (card_has_rank(played[i]->card) && played[i]->card->rank == rank)
                {
                    card_object_set_selected(played[i], true);
                    selected++;
                }
            return;
        }
}

static inline void select_two_pair_cards_in_played_hand(void)
{
    int counts[NUM_RANKS] = {0};
    for (int i = 0; i <= played_top; i++)
        if (card_has_rank(played[i]->card))
            counts[played[i]->card->rank]++;
    int pairs = 0;
    for (int rank = 0; rank < NUM_RANKS && pairs < 2; rank++)
    {
        if (counts[rank] < 2)
            continue;
        int selected = 0;
        for (int i = 0; i <= played_top && selected < 2; i++)
            if (card_has_rank(played[i]->card) && played[i]->card->rank == rank)
            {
                card_object_set_selected(played[i], true);
                selected++;
            }
        pairs++;
    }
}

static inline void select_pair_cards_in_played_hand(void)
{
    int counts[NUM_RANKS] = {0};
    for (int i = 0; i <= played_top; i++)
        if (card_has_rank(played[i]->card))
            counts[played[i]->card->rank]++;
    for (int rank = 0; rank < NUM_RANKS; rank++)
    {
        if (counts[rank] < 2)
            continue;
        int selected = 0;
        for (int i = 0; i <= played_top && selected < 2; i++)
        {
            if (card_has_rank(played[i]->card) && played[i]->card->rank == rank)
            {
                card_object_set_selected(played[i], true);
                selected++;
            }
        }
        return;
    }
}

static inline void select_highcard_cards_in_played_hand(void)
{
    int highest_rank_index = UNDEFINED;

    for (int i = 0; i <= played_top; i++)
    {
        if (card_has_rank(played[i]->card) &&
            (highest_rank_index == UNDEFINED ||
             played[i]->card->rank > played[highest_rank_index]->card->rank))
        {
            highest_rank_index = i;
        }
    }

    if (highest_rank_index != UNDEFINED)
        card_object_set_selected(played[highest_rank_index], true);
}

// returns true if a joker was scored, false otherwise
static bool check_and_score_joker_for_event(
    ListItr* starting_joker_itr,
    CardObject* card_object,
    enum JokerEvent joker_event
)
{
    JokerObject* joker;

    while ((joker = list_itr_next(starting_joker_itr)))
    {
        if (game_is_joker_disabled(joker))
            continue;
        if (joker_object_score(joker, card_object, joker_event))
        {
            for (int i = 0; i < alchemy_antimony_retriggers; i++)
                if (joker == alchemy_antimony_jokers[i])
                    joker_object_score(joker, card_object, joker_event);
            return true;
        }
    }
    return false;
}

static inline bool game_round_is_over(void)
{
    return g_game_vars.hands == 0 || g_game_vars.score >= alchemy_blind_requirement;
}

// Basically a copy of HAND_DISCARD
// returns true if the current card has been discarded
static bool play_ended_played_cards_update(int played_idx)
{
    if (!discarded_card && g_game_vars.timer > FRAMES(40))
    {
        // play the sound only once per card, when it is pushed off-screen to the right
        if (!sound_played)
        {
            play_sfx(
                SFX_CARD_DRAW,
                MM_BASE_PITCH_RATE + cards_drawn * PITCH_STEP_DISCARD_SFX,
                SFX_DEFAULT_VOLUME
            );
            sound_played = true;
        }

        // card has exited the screen, now discard it and set it to NULL
        if (played[played_idx]->sprite_object->x >= int2fx(CARD_DISCARD_PNT.x))
        {
            Card* finished_card = played[played_idx]->card;
            bool destroy_card =
                (finished_card->boss_flags & CARD_RUNTIME_DESTROY) != 0;
            if (!destroy_card)
                discard_push(finished_card); // Push the card to the discard pile
            card_object_destroy(&played[played_idx]);
            if (destroy_card)
            {
                /*
                 * A temporary Uranium copy of Glass may legitimately break.
                 * Drop its backup before the fixed Card pool can reuse the
                 * address for an unrelated card later in the same blind.
                 */
                game_alchemy_remove_uranium_backup(finished_card, false);
                card_destroy(&finished_card);
            }

            // played_top--;
            cards_drawn++; // This technically isn't drawing cards, I'm just reusing the variable
            sound_played = false; // Allow for the sound for the next card to be played

            // we reached hand_top, all cards have been discarded
            if (played_idx == played_top)
            {
                if (game_round_is_over())
                {
                    game_capture_round_end_card_modifiers();
                    set_hand_state(HAND_SHUFFLING);
                }
                else
                {
                    set_hand_state(boss_state_after_play());
                }

                play_state = PLAY_STARTING;
                cards_drawn = 0;
                hand_set_nb_selected_cards(0);
                played_top = -1; // Reset the played stack
                scored_card_index = 0;
                _joker_scored_itr = list_itr_create(&_owned_jokers_list);
                g_game_vars.timer = TM_ZERO;
            }

            return true; // return early to avoid accessing played[played_idx] == NULL
        }

        // put target X position off screen to the right
        played[played_idx]->sprite_object->tx = int2fx(CARD_DISCARD_PNT.x);
        discarded_card = true;
    }

    return false;
}

static inline void play_starting_played_cards_update(int played_idx)
{
    /*
     * scored_card_index starts at played_top + 1.  The old expression
     * played[played_top - scored_card_index] therefore read played[-1] on
     * the first frame.  Apart from being undefined behaviour, that could
     * advance the entrance sequence early and leave the edge card without a
     * reliable animation.
     */
    int previous_revealed_idx = played_top - scored_card_index;
    bool card_selected =
        previous_revealed_idx >= 0 && previous_revealed_idx <= played_top &&
        card_object_is_selected(played[previous_revealed_idx]);
    if (played_idx == played_top && (g_game_vars.timer % FRAMES(10) == 0 || !card_selected) &&
        g_game_vars.timer > FRAMES(40))
    {
        scored_card_index--;

        if (scored_card_index == 0)
        {
            _joker_scored_itr = list_itr_create(&_owned_jokers_list);
            g_game_vars.timer = TM_ZERO;
            play_state = PLAY_BEFORE_SCORING;
        }
    }

    played[played_idx]->sprite_object->tx =
        int2fx(HAND_PLAY_POS.x) + (int2fx(played_top - played_idx) - int2fx(played_top) / 2) * -27;
    played[played_idx]->sprite_object->ty = int2fx(HAND_PLAY_POS.y);

    card_selected = card_object_is_selected(played[played_idx]);
    if (card_selected && played_top - played_idx >= scored_card_index)
    {
        played[played_idx]->sprite_object->ty -= int2fx(10);
    }
}

// returns true if the scoring loop has returned early
static inline bool play_before_scoring_cards_update(void)
{
    // Activate Jokers with an effect just before the hand is scored
    if (check_and_score_joker_for_event(&_joker_scored_itr, NULL, JOKER_EVENT_ON_HAND_PLAYED))
    {
        return true;
    }

    play_state = PLAY_SCORING_CARDS;
    return false;
}

// returns true if the scoring loop has returned early
static inline bool play_scoring_cards_update(void)
{
    if (g_game_vars.timer % FRAMES(30) == 0 && g_game_vars.timer > FRAMES(40))
    {
        // We are about to score played Cards.
        // Start from the current card index
        // and seek the next scoring card
        while (scored_card_index <= played_top &&
               !card_object_is_selected(played[scored_card_index]))
        {
            scored_card_index++;
        }

        // go to the next state if there are no cards left to score
        if (scored_card_index > played_top)
        {
            // reuse these variables for held cards
            _joker_scored_itr = list_itr_create(&_owned_jokers_list);
            scored_card_index = get_hand_top();
            memset(alchemy_steel_scored, 0, sizeof(alchemy_steel_scored));

            play_state = PLAY_SCORING_HELD_CARDS;

            return false;
        }

        tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

        CardObject* scored_card_object = played[scored_card_index];

        if (card_object_is_selected(scored_card_object))
        {
            // Offset of 1 tile to keep the text on the card
            tte_set_pos(
                fx2int(scored_card_object->sprite_object->x) + TILE_SIZE,
                SCORED_CARD_TEXT_Y
            );

            // Set text color to blue from background memory
            tte_set_special(TTE_BLUE_PB * TTE_SPECIAL_PB_MULT_OFFSET);

            u8 card_value = card_get_value(scored_card_object->card);
            u8 flags = scored_card_object->card->alchemy_flags;
            bool boss_debuffed =
                (scored_card_object->card->boss_flags & CARD_BOSS_DEBUFFED) != 0;
            u32 card_chips = boss_debuffed ? 0 : card_value;
            if (!boss_debuffed)
            {
                switch (scored_card_object->card->enhancement)
                {
                    case CARD_ENHANCEMENT_BONUS:
                        card_chips = u32_protected_add(card_chips, 30);
                        break;
                    case CARD_ENHANCEMENT_STONE:
                        card_chips = u32_protected_add(card_chips, 50);
                        break;
                    default:
                        break;
                }
                if (scored_card_object->card->edition == CARD_EDITION_FOIL)
                    card_chips = u32_protected_add(card_chips, 50);
            }
            /*
             * Show the complete chip contribution of the card, including
             * visible Alchemical bonuses, rather than printing only its base
             * rank while silently changing the score panel.
             */
            char score_buffer[UINT_MAX_DIGITS + 2]; // for '+' and null terminator
            snprintf(score_buffer, sizeof(score_buffer), "+%lu", card_chips);
            tte_write(score_buffer);

            card_object_shake(scored_card_object, SFX_CHIPS_CARD);
            /*
             * Give every scoring card a small positional kick as well as the
             * affine shake.  The fifth/rightmost card immediately transitions
             * the state machine toward held-card scoring, which made its
             * rotation-only pulse easy to miss on fast game speeds.
             */
            scored_card_object->sprite_object->vy -= int2fx(1);

            // Relocated card scoring logic here
            chips = u32_protected_add(chips, card_chips);
            if (!boss_debuffed &&
                scored_card_object->card->enhancement == CARD_ENHANCEMENT_MULT)
                mult = u32_protected_add(mult, 4);
            if (!boss_debuffed &&
                scored_card_object->card->edition == CARD_EDITION_HOLOGRAPHIC)
                mult = u32_protected_add(mult, 10);
            if (!boss_debuffed &&
                scored_card_object->card->edition == CARD_EDITION_POLYCHROME)
            {
                u64 scaled = ((u64)mult * 3U) / 2U;
                mult = scaled > UINT_MAX ? UINT_MAX : (u32)scaled;
            }
            if (!boss_debuffed && (flags & CARD_ALCHEMY_POLY) &&
                scored_card_object->card->edition != CARD_EDITION_POLYCHROME)
            {
                u64 scaled = ((u64)mult * 3U) / 2U;
                mult = scaled > UINT_MAX ? UINT_MAX : (u32)scaled;
            }
            if (!boss_debuffed &&
                ((flags & CARD_ALCHEMY_GLASS) ||
                 scored_card_object->card->enhancement == CARD_ENHANCEMENT_GLASS))
                mult = u32_protected_mult(mult, 2);
            if (!boss_debuffed &&
                (scored_card_object->card->enhancement == CARD_ENHANCEMENT_LUCKY))
            {
                if (rng_get_u32() % 5 == 0)
                    mult = u32_protected_add(mult, 20);
                if (rng_get_u32() % 15 == 0)
                    g_game_vars.money =
                        g_game_vars.money > INT_MAX - 20 ? INT_MAX
                                                        : g_game_vars.money + 20;
            }
            if (!boss_debuffed && (flags & CARD_ALCHEMY_LUCKY))
            {
                if (rng_get_u32() % 5 == 0)
                    mult = u32_protected_add(mult, 10);
                if (rng_get_u32() % 15 == 0)
                    g_game_vars.money =
                        g_game_vars.money > INT_MAX - 5 ? INT_MAX : g_game_vars.money + 5;
            }
            if (!boss_debuffed && scored_card_object->card->seal == CARD_SEAL_GOLD)
                g_game_vars.money =
                    g_game_vars.money > INT_MAX - 3 ? INT_MAX : g_game_vars.money + 3;
            if (!boss_debuffed &&
                scored_card_object->card->enhancement == CARD_ENHANCEMENT_GLASS &&
                rng_get_u32() % 4 == 0)
                scored_card_object->card->boss_flags |= CARD_RUNTIME_DESTROY;
            display_chips();
            display_mult();
            display_money();

            // Allow Joker scoring
            _joker_scored_itr = list_itr_create(&_owned_jokers_list);
            _joker_card_scored_end_itr = list_itr_create(&_owned_jokers_list);
        }

        play_state = PLAY_SCORING_CARD_JOKERS;
        return true;
    }

    return false;
}

// Activate "on scored" Jokers for the previous scored card if any
// returns true if the scoring loop has returned early
static inline bool play_scoring_card_jokers_update(void)
{
    if (g_game_vars.timer % FRAMES(30) == 0 && g_game_vars.timer > FRAMES(40))
    {
        tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);
        if (scored_card_index < 0 || scored_card_index > get_played_top() ||
            scored_card_index >= MAX_HAND_SIZE ||
            played[scored_card_index] == NULL ||
            played[scored_card_index]->card == NULL)
        {
            /*
             * A destroyed card or interrupted scoring animation cannot be
             * allowed to turn a recoverable state mismatch into an invalid
             * pointer jump.
             */
            scored_card_index++;
            play_state = PLAY_SCORING_CARDS;
            return false;
        }
        if (played[scored_card_index]->card->boss_flags & CARD_BOSS_DEBUFFED)
        {
            scored_card_index++;
            play_state = PLAY_SCORING_CARDS;
            return false;
        }

        // since we sought the next scoring card index in the previous state,
        // scored_card_index is guaranteed to be a scoring card
        if (check_and_score_joker_for_event(
                &_joker_scored_itr,
                played[scored_card_index],
                JOKER_EVENT_ON_CARD_SCORED
            ))
        {
            return true;
        }

        // Trigger all Jokers that have an effect when a card finishes scoring
        // (e.g. retriggers) after activating all the other scored_card Jokers normally
        if (check_and_score_joker_for_event(
                &_joker_card_scored_end_itr,
                played[scored_card_index],
                JOKER_EVENT_ON_CARD_SCORED_END
            ))
        {
            // If we just scored a retrigger, return early and go back to the
            // previous state score the same card again without incrementing
            // scored_card_index to score the current card again
            if (retrigger)
            {
                retrigger = false;
                play_state = PLAY_SCORING_CARDS;
            }
            return true;
        }

        if (played[scored_card_index]->card->seal == CARD_SEAL_RED &&
            !red_seal_retriggered[scored_card_index])
        {
            red_seal_retriggered[scored_card_index] = true;
            _joker_scored_itr = list_itr_create(&_owned_jokers_list);
            _joker_card_scored_end_itr = list_itr_create(&_owned_jokers_list);
            play_state = PLAY_SCORING_CARDS;
            return true;
        }

        // increment index to start seeking the next scoring card from the next card
        scored_card_index++;
        play_state = PLAY_SCORING_CARDS;
    }

    return false;
}

// returns true if the scoring loop has returned early
static inline bool play_scoring_held_cards_update(int played_idx)
{
    if (played_idx == 0 && (g_game_vars.timer % FRAMES(30) == 0) && g_game_vars.timer > FRAMES(40))
    {
        tte_erase_rect_wrapper(HELD_CARDS_SCORES_RECT);

        CardObject** hand = get_hand_array();

        // Go through all held cards and see if they activate Jokers
        for (; scored_card_index >= 0; scored_card_index--)
        {
            if (scored_card_index > get_hand_top() ||
                hand[scored_card_index] == NULL ||
                hand[scored_card_index]->card == NULL)
            {
                continue;
            }
            if (!(hand[scored_card_index]->card->boss_flags & CARD_BOSS_DEBUFFED) &&
                !alchemy_steel_scored[scored_card_index] &&
                ((hand[scored_card_index]->card->alchemy_flags & CARD_ALCHEMY_STEEL) ||
                 hand[scored_card_index]->card->enhancement == CARD_ENHANCEMENT_STEEL))
            {
                if (hand[scored_card_index]->card->enhancement ==
                    CARD_ENHANCEMENT_STEEL)
                {
                    u64 scaled = ((u64)mult * 3U) / 2U;
                    mult = scaled > UINT_MAX ? UINT_MAX : (u32)scaled;
                }
                else
                {
                    u64 scaled = ((u64)mult * 3U) / 2U;
                    mult = scaled > UINT_MAX ? UINT_MAX : (u32)scaled;
                }
                alchemy_steel_scored[scored_card_index] = true;
                display_mult();
            }
            if (check_and_score_joker_for_event(
                    &_joker_scored_itr,
                    hand[scored_card_index],
                    JOKER_EVENT_ON_CARD_HELD
                ))
            {
                card_object_shake(hand[scored_card_index], SFX_CARD_SELECT);
                return true;
            }
            _joker_scored_itr = list_itr_create(&_owned_jokers_list);

            /*
             * A Red Seal retriggers the complete held-card pass, including
             * Steel and Joker effects that listen for cards held in hand.
             */
            if (hand[scored_card_index]->card->seal == CARD_SEAL_RED &&
                !red_seal_held_retriggered[scored_card_index])
            {
                red_seal_held_retriggered[scored_card_index] = true;
                alchemy_steel_scored[scored_card_index] = false;
                scored_card_index++;
            }
        }

        scored_card_index = 0;
        _joker_round_end_itr = list_itr_create(&_owned_jokers_list);
        play_state = PLAY_SCORING_INDEPENDENT_JOKERS;
    }

    return false;
}

// Score Jokers normally (independent)
// returns true if the scoring loop has returned early
static inline bool play_scoring_independent_jokers_update(int played_idx)
{
    if (played_idx == 0 && (g_game_vars.timer % FRAMES(30) == 0) && g_game_vars.timer > FRAMES(40))
    {

        tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

        if (check_and_score_joker_for_event(&_joker_scored_itr, NULL, JOKER_EVENT_INDEPENDENT))
        {
            return true;
        }

        scored_card_index =
            played_top + 1; // Reset the scored card index to the top of the played stack

        play_state = PLAY_SCORING_HAND_SCORED_END;
    }

    return false;
}

// Trigger hand end effect for all jokers once they are done scoring
static inline bool play_scoring_hand_scored_end_update(int played_idx)
{
    if (played_idx == 0 && (g_game_vars.timer % FRAMES(30) == 0) && g_game_vars.timer > FRAMES(40))
    {

        tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

        bool scored = check_and_score_joker_for_event(
            &_joker_round_end_itr,
            NULL,
            JOKER_EVENT_ON_HAND_SCORED_END
        );

        if (scored)
        {
            return true;
        }

        g_game_vars.timer = TM_ZERO;
        play_state = PLAY_ENDING;
    }

    return false;
}

// This is the reverse of PLAY_STARTING. The cards get reset back to their neutral position
// sequentially
static inline void play_ending_played_cards_update(int played_idx)
{
    int previous_reset_idx = played_top - scored_card_index;
    bool card_selected =
        previous_reset_idx >= 0 && previous_reset_idx <= played_top &&
        card_object_is_selected(played[previous_reset_idx]);
    if (played_idx == played_top && (g_game_vars.timer % FRAMES(10) == 0 || !card_selected) &&
        g_game_vars.timer > FRAMES(40))
    {
        scored_card_index--;

        /* SFX_CHIPS_ACCUM has been pitch shifted to perserve high frequencies in downsampling.
         * Now it needs to be pitch shifted back to the original frequency.
         */
        int static const CHIPS_ACCUM_SFX_PITCH_RATIO = 2;

        if (scored_card_index == 0)
        {
            play_sfx(
                SFX_CHIPS_ACCUM,
                CHIPS_ACCUM_SFX_PITCH_RATIO * MM_BASE_PITCH_RATE,
                SFX_DEFAULT_VOLUME
            );
            g_game_vars.timer = TM_ZERO;
            play_state = PLAY_ENDED;
        }
    }

    if (card_object_is_selected(played[played_idx]) && played_top - played_idx >= scored_card_index)
    {
        played[played_idx]->sprite_object->ty = int2fx(HAND_PLAY_POS.y);
    }
}

static inline void played_cards_update_loop(void)
{
    // So this one is a bit fucking weird because I have to work kinda backwards for everything
    // because of the order of the pushed cards from the hand to the play stack (also crazy that the
    // company that published Balatro is called "Playstack" and this is a play stack, but I digress)
    for (int played_idx = 0; played_idx <= played_top; played_idx++)
    {
        if (played[played_idx] == NULL)
        {
            continue;
        }

        if (card_object_get_sprite(played[played_idx]) == NULL)
        {
            // Set the sprite for the played card object
            card_object_set_sprite(played[played_idx], played_idx + MAX_HAND_SIZE);
        }

        switch (play_state)
        {
            case PLAY_STARTING:

                play_starting_played_cards_update(played_idx);
                break;

            case PLAY_BEFORE_SCORING:

                if (play_before_scoring_cards_update())
                {
                    return;
                }
                break;

            case PLAY_SCORING_CARDS:

                if (play_scoring_cards_update())
                {
                    return;
                }
                break;

            case PLAY_SCORING_CARD_JOKERS:

                if (play_scoring_card_jokers_update())
                {
                    return;
                }
                break;

            case PLAY_SCORING_HELD_CARDS:

                if (play_scoring_held_cards_update(played_idx))
                {
                    return;
                }
                break;

            case PLAY_SCORING_INDEPENDENT_JOKERS:

                if (play_scoring_independent_jokers_update(played_idx))
                {
                    return;
                }
                break;

            case PLAY_SCORING_HAND_SCORED_END:

                if (play_scoring_hand_scored_end_update(played_idx))
                {
                    return;
                }
                break;

            case PLAY_ENDING:

                play_ending_played_cards_update(played_idx);
                break;

            case PLAY_ENDED:

                if (play_ended_played_cards_update(played_idx))
                {
                    // we continue here instead of returning for performance
                    // to instantly go to the next card to discard at played_idx+1,
                    // instead of  starting over from index 0 and going up
                    // to that card again
                    continue;
                }
                break;
        }

        played[played_idx]->sprite_object->tscale = FIX_ONE;
    }
}

static inline void game_playing_process_input_and_state(void)
{
    if (get_hand_state() == HAND_SELECT)
    {
        game_playing_process_hand_select_input();
    }
    else if (play_state == PLAY_ENDING)
    {
        if (mult > 0)
        {
            // protect against score overflow
            temp_score = u32_protected_mult(chips, mult);
            score_lerp_progress = 0;

            display_temp_score(temp_score);

            chips = 0;
            mult = 0;
            display_mult();
            display_chips();

            static const int SCORE_CALC_SFX_PITCH_SHIFT = -102; // -10% OF MM_BASE_PITCH_RATE
            static const int SCORE_CALC_SFX_VOLUME = 204;       // 80% MM_SFX_FULL_VOLUME

            // The chips calculation SFX is the same as button
            play_sfx(
                SFX_BUTTON,
                MM_BASE_PITCH_RATE + SCORE_CALC_SFX_PITCH_SHIFT,
                SCORE_CALC_SFX_VOLUME
            );
        }
    }
    else if (play_state == PLAY_ENDED && g_game_vars.timer % FRAMES(TM_SCORE_LERP_INTERVAL) == 0)
    {
        u32 speed = clamp(g_game_vars.game_speed, GAME_SPEED_MIN, GAME_SPEED_MAX);
        score_lerp_progress =
            min((u32)NUM_SCORE_LERP_STEPS, score_lerp_progress + speed);

        u32 added =
            (u32)(((u64)temp_score * score_lerp_progress) / NUM_SCORE_LERP_STEPS);
        u32 remaining = temp_score - added;

        if (score_lerp_progress < NUM_SCORE_LERP_STEPS)
        {
            // Set the score display first because it's more important
            // in case there isn't enough time within the frame to display both
            display_score(u32_protected_add(g_game_vars.score, added));
            display_temp_score(remaining);
        }
        else
        {
            g_game_vars.score = u32_protected_add(g_game_vars.score, temp_score);
            temp_score = 0;
            score_lerp_progress = 0;

            tte_erase_rect_wrapper(TEMP_SCORE_RECT); // Just erase the temp score

            display_score(g_game_vars.score);
        }
    }
}

static inline void game_playing_process_card_draw()
{
    int draw_target =
        boss_draw_count == UNDEFINED ? g_game_vars.hand_size : boss_draw_count;
    if (get_hand_state() == HAND_DRAW && cards_drawn < draw_target)
    {
        if (g_game_vars.timer % FRAMES(10) == 0) // Draw a card every 10 frames
        {
            bool has_capacity =
                get_hand_top() < g_game_vars.hand_size - 1 &&
                get_hand_top() < MAX_HAND_SIZE - 1;
            if (deck_top < 0 || !has_capacity)
            {
                /* Do not spend ten frames per impossible draw. */
                cards_drawn = draw_target;
            }
            else if (card_draw())
            {
                cards_drawn++;
            }
            else
            {
                /*
                 * The deck still had a card and the hand had room, so this is
                 * a fixed-pool allocation failure. Continuing to retry would
                 * hard-lock the blind; end it safely instead.
                 */
                hand_deselect_all_cards();
                g_game_vars.hands = 0;
                display_hands();
                set_hand_state(HAND_SHUFFLING);
                cards_drawn = 0;
                boss_draw_count = UNDEFINED;
                g_game_vars.timer = TM_ZERO;
            }
        }
    }
    else if (get_hand_state() == HAND_DRAW)
    {
        cards_drawn = 0;
        boss_draw_count = UNDEFINED;
        boss_house_initial_draw = false;
        boss_fish_next_draw_face_down = false;

        int held_cards = hand_nb_held_cards();
        bool has_phosphorus_rescue = false;
        if (deck_top < 0 && discard_top >= 0)
        {
            int required_cards =
                boss_blind_active(BLIND_TYPE_PSYCHIC) ? 5 : 1;
            for (int i = 0; i < g_game_vars.alchemy.count; i++)
            {
                if (g_game_vars.alchemy.held[i] == ALCHEMICAL_PHOSPHORUS &&
                    alchemical_phosphorus_can_rescue_hand(
                        held_cards,
                        discard_top + 1,
                        required_cards
                    ))
                {
                    has_phosphorus_rescue = true;
                    break;
                }
            }
        }
        bool no_legal_action =
            held_cards == 0 && deck_top < 0 && !has_phosphorus_rescue;
        if (!no_legal_action && boss_blind_active(BLIND_TYPE_PSYCHIC) &&
            held_cards < 5 && deck_top < 0)
        {
            int wax_cards = 0;
            for (int i = 0; i < g_game_vars.alchemy.count; i++)
                wax_cards +=
                    g_game_vars.alchemy.held[i] == ALCHEMICAL_WAX;
            no_legal_action =
                !has_phosphorus_rescue &&
                !alchemical_wax_can_rescue_hand(
                    g_game_vars.hand_size,
                    held_cards,
                    wax_cards,
                    deck_get_max_size(),
                    5,
                    MAX_HAND_SIZE,
                    MAX_CARDS
                );
        }
        if (no_legal_action)
        {
            /*
             * Empty decks are legal late in a blind, but an empty hand (or
             * fewer than five cards under The Psychic) offers no input that
             * can advance the state. Convert that dead screen into a normal
             * failed-blind transition.
             */
            hand_deselect_all_cards();
            g_game_vars.hands = 0;
            display_hands();
            set_hand_state(HAND_SHUFFLING);
            g_game_vars.timer = TM_ZERO;
            return;
        }

        set_hand_state(HAND_SELECT); // Change the hand state to select after drawing all the cards
        if (held_cards == 0 && has_phosphorus_rescue)
        {
            /*
             * The hand row is empty, so directional navigation cannot leave
             * it. Focus the rescuing consumable directly and keep the player
             * in control instead of requiring an impossible Up transition.
             */
            int phosphorus_slot = 0;
            while (phosphorus_slot < g_game_vars.alchemy.count &&
                   g_game_vars.alchemy.held[phosphorus_slot] !=
                       ALCHEMICAL_PHOSPHORUS)
                phosphorus_slot++;
            Selection previous = game_playing_selection_grid.selection;
            Selection rescue = {
                jokers_sel_row_get_size() + phosphorus_slot,
                0
            };
            if (phosphorus_slot < g_game_vars.alchemy.count &&
                game_playing_top_row_on_selection_changed(
                    &game_playing_selection_grid,
                    0,
                    &previous,
                    &rescue
                ))
            {
                game_playing_selection_grid.selection = rescue;
            }
        }
        boss_force_random_card();
        g_game_vars.timer = TM_ZERO;
    }
}

static inline void game_playing_discarded_cards_loop(void)
{
    // Discarded cards loop (mainly for shuffling)
    if (hand_nb_held_cards() == 0 && get_hand_state() == HAND_SHUFFLING && discard_top >= -1 &&
        g_game_vars.timer > FRAMES(10))
    {
        // Change the background to the round end background. This is how it works in Balatro, so
        // I'm doing it this way too.
        change_background(BG_ROUND_END, false);

        // We take each discarded card and put it back into the deck with a short animation
        static CardObject* discarded_card_object = NULL;
        if (discarded_card_object == NULL)
        {
            /*
             * Acid and other hand/deck effects can legitimately leave no card
             * to animate.  The old code created a CardObject from NULL and
             * immediately dereferenced it.
             */
            if (discard_top < 0)
            {
                game_playing_handle_round_over();
                return;
            }

            Card* card = discard_pop();
            discarded_card_object = card_object_new(card);
            if (discarded_card_object == NULL)
            {
                /*
                 * If the visual object pool is exhausted, skip this one
                 * animation but keep the ownership transition progressing.
                 * Retrying the same allocation forever freezes the result
                 * screen and is worse than a dropped cosmetic animation.
                 */
                if (!deck_push(card))
                    discard_push(card);
                return;
            }
            // discarded_card_object->sprite = sprite_new(ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF,
            // ATTR1_SIZE_32,
            // card_sprite_lut[discarded_card_object->card->suit][discarded_card_object->card->rank],
            // 0, 0);
            // Set the sprite for the discarded card object
            card_object_set_sprite(discarded_card_object, 0);
            sprite_object_reset_transform(discarded_card_object->sprite_object);

            discarded_card_object->sprite_object->tx = int2fx(204);
            discarded_card_object->sprite_object->ty = int2fx(112);
            discarded_card_object->sprite_object->x = int2fx(240);
            discarded_card_object->sprite_object->y = int2fx(80);
        }
        else
        {
            if (discarded_card_object->sprite_object->y >= discarded_card_object->sprite_object->ty)
            {
                deck_push(discarded_card_object->card); // Put the card back into the deck
                card_object_destroy(&discarded_card_object);

                play_sfx(
                    SFX_CARD_DRAW,
                    MM_BASE_PITCH_RATE + PITCH_STEP_UNDISCARD_SFX,
                    SFX_DEFAULT_VOLUME
                );
            }
        }

        // If there are no more discarded cards, stop shuffling
        if (discard_top == -1 && discarded_card_object == NULL)
        {
            // After HAND_SHUFFLING the round is over
            game_playing_handle_round_over();
        }
    }
}

static inline void select_cards_in_played_hand()
{
    switch (get_hand_type()) // select the cards that apply to the hand type
    {
        case NONE:
            break;
        case HIGH_CARD:
            select_highcard_cards_in_played_hand();
            break;
        case PAIR:
            select_pair_cards_in_played_hand();
            break;
        case TWO_PAIR:
            select_two_pair_cards_in_played_hand();
            break;
        case THREE_OF_A_KIND:
            select_three_of_a_kind_cards_in_played_hand();
            break;
        case FOUR_OF_A_KIND:
            select_four_of_a_kind_cards_in_played_hand();
            break;
        case STRAIGHT:
            /* FALL THROUGH */
        case FLUSH:
            /* FALL THROUGH */
        case STRAIGHT_FLUSH:
            /* FALL THROUGH */
        case ROYAL_FLUSH:
            /*
             * With the normal five-card rule, a five-card selection already
             * classified as a Straight/Flush necessarily scores all five.
             * Mark them directly so the lowest/rightmost card cannot be lost
             * by the separate animation-selection reconstruction.
             */
            if (played_top == MAX_SELECTION_SIZE - 1 &&
                get_straight_and_flush_size() == MAX_SELECTION_SIZE)
            {
                for (int i = 0; i <= played_top; i++)
                    card_object_set_selected(played[i], true);
                break;
            }
            select_flush_and_straight_cards_in_played_hand();
            break;
        case FULL_HOUSE:
            /* FALL THROUGH */
        case FIVE_OF_A_KIND:
            /* FALL THROUGH */
        case FLUSH_HOUSE:
            /* FALL THROUGH */
        case FLUSH_FIVE: // Select all played cards in the hand
            select_all_five_cards_in_played_hand();
            break;
    }
    /* Stone cards never form a poker hand, but always score when played. */
    for (int i = 0; i <= played_top; i++)
        if (played[i] != NULL && played[i]->card != NULL &&
            played[i]->card->enhancement == CARD_ENHANCEMENT_STONE)
            card_object_set_selected(played[i], true);
}

static inline void cards_in_hand_update_loop(void)
{
    int selected_card_idx = hand_sel_idx_to_card_idx(game_playing_selection_grid.selection.x);

    // TODO: Break this function up into smaller ones, Gods be good
    // Start from the end of the hand and work backwards because that's how Balatro does it
    CardObject** hand = get_hand_array();

    for (int i = get_hand_top(); i >= 0; i--)
    {
        if (hand[i] != NULL)
        {
            FIXED hand_x = int2fx(HAND_START_POS.x);
            FIXED hand_y = int2fx(HAND_START_POS.y);
            bool cursor_focused =
                get_hand_state() == HAND_SELECT && i == selected_card_idx &&
                game_playing_selection_grid.selection.y == GAME_PLAYING_HAND_SEL_Y;

            switch (get_hand_state())
            {
                case HAND_DRAW:
                    hand_x = hand_x + (int2fx(i) - int2fx(get_hand_top()) / 2) *
                                          -HAND_SPACING_LUT[get_hand_top()];
                    break;
                case HAND_SELECT:
                    if (cursor_focused && !card_object_is_selected(hand[i]))
                    {
                        hand_y -= int2fx(CARD_FOCUSED_UNSEL_Y);
                    }
                    else if (!cursor_focused && card_object_is_selected(hand[i]))
                    {
                        hand_y -= int2fx(CARD_UNFOCUSED_SEL_Y);
                    }
                    else if (cursor_focused && card_object_is_selected(hand[i]))
                    {
                        hand_y -= int2fx(CARD_FOCUSED_SEL_Y);
                    }
                    if (i != selected_card_idx && hand[i]->sprite_object->y > hand_y)
                    {
                        hand[i]->sprite_object->y = hand_y;
                        // Set target y to match y. Ensures target is updated even when vy becomes
                        // 0, preventing immediate snap back.
                        hand[i]->sprite_object->ty = hand_y;
                        hand[i]->sprite_object->vy = 0;
                    }

                    hand_x = hand_x + (int2fx(i) - int2fx(get_hand_top()) / 2) *
                                          -HAND_SPACING_LUT[get_hand_top()]; // TODO: Change this
                                                                             // later to reference a
                                                                             // 2D LUT of positions
                    break;
                case HAND_SHUFFLING:
                    /* FALL THROUGH */
                case HAND_DISCARD: // TODO: Add sound
                    bool break_loop;
                    card_in_hand_loop_handle_discard_and_shuffling(
                        i,
                        &hand_x,
                        &hand_y,
                        &break_loop
                    );
                    if (break_loop)
                        break;

                    break;
                case HAND_PLAY:
                    hand_x = hand_x + (int2fx(i) - int2fx(get_hand_top()) / 2) *
                                          -HAND_SPACING_LUT[get_hand_top()];
                    hand_y += int2fx(24);

                    if (card_object_is_selected(hand[i]) && discarded_card == false &&
                        g_game_vars.timer % FRAMES(10) == 0)
                    {
                        card_object_set_selected(hand[i], false);
                        played_push(hand[i]);
                        sprite_destroy(&hand[i]->sprite_object->sprite);
                        hand[i] = NULL;
                        reorder_card_sprites_layers();

                        play_sfx(
                            SFX_CARD_DRAW,
                            MM_BASE_PITCH_RATE + cards_drawn * PITCH_STEP_DISCARD_SFX,
                            SFX_DEFAULT_VOLUME
                        );

                        set_hand_top(get_hand_top() - 1);
                        hand_set_nb_selected_cards(hand_get_nb_selected_cards() - 1);
                        cards_drawn++;

                        discarded_card = true;
                    }

                    if (i == 0 && discarded_card == false && g_game_vars.timer % FRAMES(10) == 0)
                    {
                        set_hand_state(HAND_PLAYING);
                        cards_drawn = 0;
                        hand_set_nb_selected_cards(0);
                        g_game_vars.timer = TM_ZERO;
                        scored_card_index = played_top + 1;

                        select_cards_in_played_hand();
                    }

                    break;
                // Don't need to do anything here, just wait for the player to select cards
                case HAND_PLAYING:
                    hand_x = hand_x + (int2fx(i) - int2fx(get_hand_top()) / 2) *
                                          -HAND_SPACING_LUT[get_hand_top()];
                    hand_y += int2fx(24);
                    break;
            }

            /*
             * Playing/discarding the current top card may have removed this
             * very entry during the switch above.  Re-check it before writing
             * the target transform; the outer if described the old state.
             */
            if (hand[i] != NULL && hand[i]->sprite_object != NULL)
            {
                hand[i]->sprite_object->tx = hand_x;
                hand[i]->sprite_object->ty = hand_y;
            }
        }
    }
}

static inline void game_playing_ui_text_update(void)
{
    static int last_hand_size = 0;
    static int last_hand_capacity = 0;
    static int last_deck_size = 0;
    static enum BackgroundId last_background = BG_NONE;

    int current_hand_size = hand_nb_held_cards();
    int current_deck_size = deck_get_size();

    if (last_hand_size != current_hand_size ||
        last_hand_capacity != g_game_vars.hand_size ||
        last_deck_size != current_deck_size ||
        last_background != background_legacy)
    {
        if (background_legacy == BG_CARD_SELECTING)
        {
            // Hand size/max size
            tte_printf(
                "#{P:%d,%d; cx:0x%X000}%2d/%-2ld",
                HAND_SIZE_RECT_SELECT.left,
                HAND_SIZE_RECT_SELECT.top,
                TTE_WHITE_PB,
                current_hand_size,
                g_game_vars.hand_size
            );
        }
        else if (background_legacy == BG_CARD_PLAYING)
        {
            // Hand size/max size
            tte_printf(
                "#{P:%d,%d; cx:0x%X000}%2d/%-2ld",
                HAND_SIZE_RECT_PLAYING.left,
                HAND_SIZE_RECT_PLAYING.top,
                TTE_WHITE_PB,
                current_hand_size,
                g_game_vars.hand_size
            );
        }

        // Deck size/max size
        display_deck_size_max();

        last_hand_size = current_hand_size;
        last_hand_capacity = g_game_vars.hand_size;
        last_deck_size = current_deck_size;
        last_background = background_legacy;
    }
}

static inline void game_playing_process_flaming_score(void)
{
    static u8 flame_score_frame = 0;

    if (score_flames_active)
    {
        if (g_game_vars.timer % SCORE_FLAMES_ANIM_FREQ == 0)
        {
            Rect frame_rect = SCORE_FLAME_FRAMES_START;
            flame_score_frame = (flame_score_frame + 1) % NUM_SCORE_FLAMES_FRAMES;

            // chips flame (blue)
            frame_rect.top += flame_score_frame;
            frame_rect.bottom += flame_score_frame;
            main_bg_se_copy_rect(frame_rect, SCORE_FLAME_CHIPS_POS);

            // mult flame (red)
            frame_rect.left += SCORE_FLAME_FRAME_WIDTH;
            frame_rect.right += SCORE_FLAME_FRAME_WIDTH;
            main_bg_se_copy_rect(frame_rect, SCORE_FLAME_MULT_POS);
        }
    }
}

static void game_playing_on_update(void)
{
    // Background logic (thissss might be moved to the card'ssss logic later. I'm a sssssnake)
    if (get_hand_state() == HAND_DRAW || get_hand_state() == HAND_DISCARD ||
        get_hand_state() == HAND_SELECT)
    {
        change_background(BG_CARD_SELECTING, false);
    }
    else if (get_hand_state() != HAND_SHUFFLING)
    {
        change_background(BG_CARD_PLAYING, false);
    }

    game_playing_process_input_and_state();
    boss_enforce_forced_card();

    /*
     * Never consume/destroy an AlchemicalObject from inside a SelectionGrid
     * callback.  At this point the grid has finished using its Selection
     * pointer, so inventory compaction and sprite destruction are safe.
     */
    if (alchemy_pending_use_slot != UNDEFINED)
    {
        alchemy_selected_slot = alchemy_pending_use_slot;
        alchemy_pending_use_slot = UNDEFINED;
        game_alchemy_use_selected();
    }

    if (alchemy_feedback_timer > 0 && --alchemy_feedback_timer == 0)
        tte_erase_rect_wrapper(ALCHEMY_FEEDBACK_RECT);

    // Card logic

    game_playing_process_card_draw();

    game_playing_discarded_cards_loop();

    discarded_card = false;

    cards_in_hand_update_loop();
    played_cards_update_loop();

    game_playing_ui_text_update();

    // animate score flames if we exceed the score requirement
    game_playing_process_flaming_score();
}

void game_start(void)
{
    affine_background_change_background(AFFINE_BG_GAME);
    tte_colors_setup();

    bool new_run = deck_top < 0;
    if (new_run)
    {
        g_game_vars.money = deck_get_starting_money(
            (enum DeckType)g_game_vars.deck,
            STARTING_MONEY
        );
        g_game_vars.hand_size = min(
            MAX_HAND_SIZE,
            deck_get_hand_size(
                (enum DeckType)g_game_vars.deck,
                DEFAULT_HAND_SIZE
            )
        );
    }
    g_game_vars.hands = deck_get_hands_per_blind(
        (enum DeckType)g_game_vars.deck,
        voucher_get_hands_per_blind(&g_game_vars.vouchers, MAX_HANDS)
    );
    g_game_vars.discards = deck_get_discards_per_blind(
        (enum DeckType)g_game_vars.deck,
        voucher_get_discards_per_blind(&g_game_vars.vouchers, MAX_DISCARDS)
    );

    // A resumed run already restored its exact deck from SRAM.
    if (deck_top < 0)
    {
        bool deck_ready = true;
        for (int suit = 0; suit < NUM_SUITS; suit++)
        {
            for (int rank = 0; rank < NUM_RANKS; rank++)
            {
                Card* card = card_new(suit, rank);
                if (card == NULL || !deck_push(card))
                {
                    card_destroy(&card);
                    deck_ready = false;
                    break;
                }
            }
            if (!deck_ready)
                break;
        }
        if (!deck_ready)
        {
            /*
             * Never start a run with a truncated deck after fixed-pool
             * exhaustion.  Roll the partial construction back and return to
             * setup, where the user can retry after the previous state has
             * released its resources.
             */
            while (deck_top >= 0)
            {
                Card* card = deck_pop();
                card_destroy(&card);
            }
            game_change_state(GAME_STATE_RUN_SETUP);
            return;
        }
    }

    change_background(BG_BLIND_SELECT, false);

    // Deck size/max size
    tte_erase_rect_wrapper(DECK_SIZE_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%d/%d",
        DECK_SIZE_RECT.left,
        DECK_SIZE_RECT.top,
        TTE_WHITE_PB,
        deck_get_size(),
        deck_get_max_size()
    );

    display_round();                  // Set the round display
    display_score(g_game_vars.score); // Set the score display

    display_chips(); // Set the chips display
    display_mult();  // Set the multiplier display

    display_hands();    // Hand
    display_discards(); // Discard

    display_money(); // Set the money display
    display_ante();

    game_change_state(GAME_STATE_BLIND_SELECT);
}
