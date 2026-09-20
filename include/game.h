#ifndef GAME_H
#define GAME_H

#include "game/common_ui.h"
#include "game_variables.h"
#include "graphic_utils.h"

#include <tonc.h>

#define MAX_DECK_SIZE        64
#define BASE_JOKERS_HELD_SIZE 5
#define MAX_JOKERS_HELD_SIZE  8
#define MAX_SHOP_JOKERS       4
#define MAX_SELECTION_SIZE   5
#define FRAMES(x)            (((x) + (g_game_vars.game_speed) - 1) / (g_game_vars.game_speed))

// TODO: Can make these dynamic to support interest-related jokers and vouchers
#define MAX_INTEREST   5
#define INTEREST_PER_5 1

// Input bindings
#define SELECT_CARD    KEY_A
#define DESELECT_CARDS KEY_B
#define PEEK_DECK      KEY_L // Not implemented
#define SORT_HAND      KEY_R
#define PAUSE_GAME     KEY_START // Not implemented
#define SELL_KEY       KEY_L
#define TAB_LEFT       KEY_L
#define TAB_RIGHT      KEY_R

// Matching the position of the on-screen buttons
#define PLAY_HAND_KEY KEY_L
// Same value as SELL_KEY - activated on the joker row, while this is activated on the hand row

#define DISCARD_HAND_KEY KEY_R

struct List;
typedef struct List List;

// Utility functions for other files
typedef struct CardObject CardObject;
typedef struct Card Card;
typedef struct Joker Joker;
typedef struct JokerObject JokerObject;

// Enum value names in ../include/def_state_info_table.h
enum GameState
{
#define DEF_STATE_INFO(stateEnum, on_init, on_update, on_exit) stateEnum,
#include "def_state_info_table.h"
#undef DEF_STATE_INFO
    GAME_STATE_MAX,
    GAME_STATE_UNDEFINED
};

enum PlayState
{
    PLAY_STARTING,
    PLAY_BEFORE_SCORING,
    PLAY_SCORING_CARDS,
    PLAY_SCORING_CARD_JOKERS,
    PLAY_SCORING_HELD_CARDS,
    PLAY_SCORING_INDEPENDENT_JOKERS,
    PLAY_SCORING_HAND_SCORED_END,
    PLAY_ENDING,
    PLAY_ENDED
};

// Game functions
void game_init(void);
int game_get_joker_capacity(void);
bool game_can_add_joker(const Joker* joker);
bool game_is_joker_disabled(const JokerObject* joker);
int game_export_deck(Card* cards, int capacity);
bool game_restore_deck(const Card* cards, int count);
void game_clear_deck(void);
bool game_debug_apply_card_modifier(int category, int value);
const char* game_get_boss_card_reward(void);

/**
 * @brief Called when exiting the Game Over screen (both win or lose) to reset game variables
 *         and start a fresh new run.
 *
 * WARNING: This function is currently only meant to be called from the "GAME_OVER" state
 * and shouldn't be called from other states, otherwise some data such as shop jokers
 * may not be properly reset.
 */
void game_reset(void);

void game_update(void);
void game_change_state(enum GameState new_game_state);
enum GameState game_get_state(void);

CardObject** get_played_array(void);
void game_notify_joker_sold(void);
int get_played_top(void);
int get_scored_card_index(void);
bool is_joker_owned(int joker_id);
bool card_is_face(const Card* card);
bool add_joker(JokerObject* joker_object);
void remove_owned_joker(int owned_joker_idx);
List* get_jokers_list(void);
List* get_expired_jokers_list(void);
List* get_discarded_jokers_list(void);

int get_deck_top(void);
int get_num_discards_remaining(void);
int get_num_hands_remaining(void);

void display_deck_size_max(void);
u32 get_chips(void);
void set_chips(u32 new_chips);
void display_chips(void);
u32 get_mult(void);
void set_mult(u32 new_mult);
void display_mult(void);
void display_money(void);
void set_retrigger(bool new_retrigger);

// joker specific functions
bool is_shortcut_joker_active(void);
int get_straight_and_flush_size(void);

void game_start(void);

// Temporary change for Refactor. Currently this compatibility binder is to allow
// simultaneous integration of the new system in `common_ui` with the the existing
// old system incrementally and without losing functionality.
void change_background_legacy(enum BackgroundId id);

void display_round(void);

void reset_background(void);
void display_hands(void);
void display_discards(void);
void display_score(u32 value);
void display_status_panel(void);

// Compile-time debug menu helpers. They are inert in DEBUG_MENU=0 builds.
bool game_debug_add_alchemical(enum AlchemicalId id);
void game_debug_win_blind(void);

#endif // GAME_H
