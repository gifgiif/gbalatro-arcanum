/**
 * @file shop.c
 *
 * @brief Shop state functions implementation.
 */

#include "game/shop.h"

#include "alchemical_object.h"
#include "alchemy.h"
#include "audio_utils.h"
#include "background_shop_gfx.h"
#include "bitset.h"
#include "button.h"
#include "game.h"
#include "game/blind_select.h"
#include "game/joker_row.h"
#include "game_variables.h"
#include "joker.h"
#include "layout.h"
#include "list.h"
#include "planet.h"
#include "planet_object.h"
#include "random.h"
#include "save.h"
#include "soundbank.h"
#include "state_machine.h"
#include "timer.h"
#include "util.h"
#include "voucher_object.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

// Timer defs
#define TM_END_GAME_SHOP_INTRO    12
#define TM_CREATE_SHOP_ITEMS_WAIT 1
#define TM_SHIFT_SHOP_ICON_WAIT   7
#define TM_SHOW_CARD_DESC_WAIT    12

// Pixel sized
#define ITEM_SHOP_Y               71
#define VOUCHER_SHOP_X            92
#define VOUCHER_SHOP_Y            106
#define ALCHEMICAL_SHOP_X         148
#define ALCHEMICAL_SHOP_Y         106
#define VOUCHER_PRICE_ANCHOR_X    80
#define VOUCHER_PRICE_ANCHOR_Y    100
#define OWNED_CARDS_HIDE_Y_OFFSET 50

// Shop
#define REROLL_BASE_COST     5 // Base cost for rerolling the shop items
#define NEXT_ROUND_BTN_SEL_X 0
#define SPECIAL_SHOP_CHANCE 75
#define PLANET_SHOP_CHANCE  40
/* A Planet or an Alchemical can occupy the shared lower-right offer slot. */
#define MAX_SHOP_ALCHEMICALS 1

// Palette IDs
#define REROLL_BTN_PAL_IDX                     3
#define NEXT_ROUND_BTN_SELECTED_BORDER_PAL_IDX 5
#define SHOP_PANEL_SHADOW_PAL_IDX              6
#define REROLL_BTN_SELECTED_BORDER_PAL_IDX     7
#define SHOP_LIGHTS_1_PAL_IDX                  8
#define SHOP_LIGHTS_2_PAL_IDX                  14
#define NEXT_ROUND_BTN_PAL_IDX                 16
#define SHOP_LIGHTS_3_PAL_IDX                  17
#define SHOP_LIGHTS_4_PAL_IDX                  22
#define SHOP_BOTTOM_PANEL_BORDER_PAL_IDX       26
#define SHOP_DESC_RARITY_MAIN_COLOR_PAL_IDX    27
#define SHOP_DESC_RARITY_SHADOW_COLOR_PAL_IDX  28

#define SHOP_LIGHTS_1_CLR 0xFFFF
#define SHOP_LIGHTS_2_CLR 0x32BE
#define SHOP_LIGHTS_3_CLR 0x4B5F
#define SHOP_LIGHTS_4_CLR 0x5F9F

// clang-format off
// Positions in tiles
static const BG_POINT SHOP_CLEAR_3X3_SRC_POS        = { 29,  0};
static const Rect     SHOP_ICON_FROM_RECT           = {  0, 26,  8, 26};
static const BG_POINT SHOP_ICON_TO_POS              = {  0,  0};
static const BG_POINT OWNED_CARDS_PANEL_3X3_SRC_POS = { 29, 21};
static const Rect     OWNED_JOKERS_PANEL_RECT       = {  9,  1, 21,  5};
static const Rect     OWNED_CONSUMABLES_PANEL_RECT  = { 23,  1, 28,  5};
static const Rect     OWNED_CARDS_PANEL_RECT        = {  9,  1, 28,  5};
static const Rect     CARD_DESC_9_PTCH_TO_RECT      = {  9,  6, 28, 18};
static const NinePatchRect CARD_DESC_9_PTCH_SRC = {
                                        .patch_rect = { 27, 25, 31, 31},
                                        .margins    = {  2,  3,  2,  3}
};
static const int      CARD_DESC_MAX_TEXT_HEIGHT     = CARD_DESC_9_PTCH_TO_RECT.bottom -
                                                      CARD_DESC_9_PTCH_TO_RECT.top + 1 -
                                                      CARD_DESC_9_PTCH_SRC.margins.top -
                                                      CARD_DESC_9_PTCH_SRC.margins.bottom;
static const Rect     CARD_DESC_TEXT_RECT           = { 11,  9, 26, 18};
static const Rect     CARD_NAME_TEXT_RECT           = { 10,  7, 27,  7};
/* One short status line in the free strip above the shop panel. */
static const Rect     ALCHEMICAL_FEEDBACK_RECT      = { 72, 44, 232, 53};
static const Rect     VOUCHER_STATUS_RECT           = { 76,112, 132,136};

// Positions in pixels
static const BG_POINT SHOP_JOKER_SPRITES_INIT_POS = {120, 160};
static const BG_POINT CARD_DESCRIPTION_SPRITE_POS = {135,   9};
static const Rect     SHOP_PRICES_TEXT_RECT       = { 72,  56, 192, 160 };
/* The shared Planet/Alchemical price belongs to the shop slot, not to the
 * animated sprite. Keeping one fixed rectangle prevents focus/description
 * movement from shifting or leaving copies of the label behind. */
static const Rect     SPECIAL_PRICE_TEXT_RECT     = {144, 143, 192, 152 };
static const Rect     SHOP_REROLL_RECT            = { 88,  96, UNDEFINED, UNDEFINED };
// clang-format on

static List s_shop_jokers_list = LIST_DEFAULT;
BITSET_DEFINE(s_avail_jokers_bitset, MAX_DEFINABLE_JOKERS)
static AlchemicalObject* s_shop_alchemicals[MAX_SHOP_ALCHEMICALS] = {NULL};
static PlanetObject* s_shop_planet = NULL;
static VoucherObject* s_shop_voucher = NULL;
static AlchemicalObject* s_held_alchemicals[ALCHEMICAL_HELD_LIMIT] = {NULL};
static int s_shop_feedback_timer = 0;
static bool s_shop_feedback_visible = false;
/*
 * The introductory special is a one-shot for the first Shop visit, not a
 * guarantee on every reroll made while g_game_vars.round is still 1.
 */
static bool s_first_shop_special_pending = false;
static bool s_first_shop_entered = false;

static inline int game_shop_alchemical_offer_count(void)
{
    /*
     * The object pointer is the source of truth. Keeping a separate count for
     * a one-slot offer allowed a visible object and a zero-sized navigation
     * row to get out of sync, making the card impossible to focus or buy.
     */
    return s_shop_alchemicals[0] != NULL ? 1 : 0;
}

static inline int game_shop_special_offer_count(void)
{
    return s_shop_planet != NULL ? 1 : game_shop_alchemical_offer_count();
}

enum GameShopStates
{
    GAME_SHOP_INTRO,
    GAME_SHOP_ACTIVE,
    GAME_SHOP_SHOW_CARD_DESC,
    GAME_SHOP_HIDE_CARD_DESC,
    GAME_SHOP_EXIT,
    GAME_SHOP_MAX
};

static void game_shop_intro(void);
static void game_shop_process_user_input(void);
static void game_shop_show_card_desc(void);
static void game_shop_hide_card_desc(void);
static void game_shop_outro(void);

static StateInfo shop_state_actions[GAME_SHOP_MAX] = {
    STATE_INFO_UPDATE_FN_ONLY(game_shop_intro),
    STATE_INFO_UPDATE_FN_ONLY(game_shop_process_user_input),
    STATE_INFO_UPDATE_FN_ONLY(game_shop_show_card_desc),
    STATE_INFO_UPDATE_FN_ONLY(game_shop_hide_card_desc),
    STATE_INFO_UPDATE_FN_ONLY(game_shop_outro),
};

static StateMachine shop_sm = STATE_MACHINE_DEFINE(shop_state_actions, GAME_SHOP_MAX);

// Shop SelectionGrid

static int shop_top_row_get_size(void);
static int shop_owned_row_get_size(void);
static bool shop_owned_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);
static void shop_owned_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
);
static bool shop_top_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);
static void shop_top_row_on_key_transit(SelectionGrid* selection_grid, Selection* selection);
static int shop_voucher_row_get_size(void);
static bool shop_voucher_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);
static void shop_voucher_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
);
static int shop_alchemical_row_get_size(void);
static bool shop_alchemical_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);
static void shop_alchemical_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
);
static int shop_reroll_row_get_size(void);
static bool shop_reroll_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
);
static void shop_reroll_row_on_key_transit(SelectionGrid* selection_grid, Selection* selection);

static SelectionGridRow shop_selection_rows[] = {
    {0, shop_owned_row_get_size,  shop_owned_row_on_selection_changed,  shop_owned_row_on_key_transit,  {.wrap = false, .has_h_exit_idx = false, .h_exit_idx = 0}},
    {1, shop_top_row_get_size,    shop_top_row_on_selection_changed,    shop_top_row_on_key_transit,    {.wrap = false, .has_h_exit_idx = false, .h_exit_idx = 0}},
    /*
     * The lower-right consumable is the next real row after shop Jokers.
     * This makes a normal SelectionGrid Down movement reach it directly.
     */
    {2, shop_alchemical_row_get_size, shop_alchemical_row_on_selection_changed, shop_alchemical_row_on_key_transit, {.wrap = false, .has_h_exit_idx = false, .h_exit_idx = 0}},
    {3, shop_voucher_row_get_size, shop_voucher_row_on_selection_changed, shop_voucher_row_on_key_transit, {.wrap = false, .has_h_exit_idx = false, .h_exit_idx = 0}},
    {4, shop_reroll_row_get_size, shop_reroll_row_on_selection_changed, shop_reroll_row_on_key_transit, {.wrap = false, .has_h_exit_idx = true, .h_exit_idx = 1} },
};

static const Selection SHOP_INIT_SEL = {-1, 1};

static SelectionGrid shop_selection_grid = {
    shop_selection_rows,
    NUM_ELEM_IN_ARR(shop_selection_rows),
    SHOP_INIT_SEL
};

// Shop Buttons

static void next_round_on_pressed(void);
static void reroll_on_pressed(void);
static bool reroll_can_be_pressed(void);
static Button next_round_button =
    {NEXT_ROUND_BTN_SELECTED_BORDER_PAL_IDX, NEXT_ROUND_BTN_PAL_IDX, next_round_on_pressed, NULL};
static Button reroll_button = {
    REROLL_BTN_SELECTED_BORDER_PAL_IDX,
    REROLL_BTN_PAL_IDX,
    reroll_on_pressed,
    reroll_can_be_pressed
};

// Shop internal variables

static int timer;

static int reroll_cost = REROLL_BASE_COST;

// Variables relative to the Card we are showing the description of
static JokerObject* description_card = NULL;
static AlchemicalObject* description_alchemical = NULL;
static PlanetObject* description_planet = NULL;
static VoucherObject* description_voucher = NULL;
static bool description_is_purchase = false;
static FIXED description_card_original_x_pos = UNDEFINED;
static FIXED description_card_original_y_pos = UNDEFINED;
static List* description_card_original_list = NULL;

JokerObject* game_shop_get_description_card(void)
{
    return description_card;
}

static SpriteObject* game_shop_get_description_sprite(void)
{
    if (description_voucher != NULL)
        return description_voucher->sprite_object;
    if (description_planet != NULL)
        return description_planet->sprite_object;
    if (description_alchemical != NULL)
        return description_alchemical->sprite_object;
    return description_card != NULL ? description_card->sprite_object : NULL;
}

static inline void reset_shop_jokers(void)
{
    int num_jokers = get_joker_registry_size();

    bitset_clear(&s_avail_jokers_bitset);
    for (int i = 0; i < num_jokers; i++)
    {
        bitset_set_idx(&s_avail_jokers_bitset, i, true);
    }
}

void game_shop_reset(void)
{
    planet_object_destroy(&s_shop_planet);
    voucher_object_destroy(&s_shop_voucher);
    for (int i = 0; i < MAX_SHOP_ALCHEMICALS; i++)
        alchemical_object_destroy(&s_shop_alchemicals[i]);
    for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
        alchemical_object_destroy(&s_held_alchemicals[i]);
    ListItr itr = list_itr_create(&s_shop_jokers_list);
    JokerObject* joker_object = NULL;
    while ((joker_object = list_itr_next(&itr)))
        joker_object_destroy(&joker_object);
    list_clear(&s_shop_jokers_list);
    s_shop_jokers_list = list_init();
    reset_shop_jokers();
    /*
     * Every object referenced by the description overlay has just been
     * destroyed.  Clear the complete overlay/navigation state as one
     * transaction so a new run cannot inherit a dangling pointer, an old
     * purchase mode, or a feedback timer from the previous Shop.
     */
    description_card = NULL;
    description_alchemical = NULL;
    description_planet = NULL;
    description_voucher = NULL;
    description_is_purchase = false;
    description_card_original_x_pos = UNDEFINED;
    description_card_original_y_pos = UNDEFINED;
    description_card_original_list = NULL;
    timer = TM_ZERO;
    reroll_cost = REROLL_BASE_COST;
    shop_selection_grid.selection = SHOP_INIT_SEL;
    s_shop_feedback_timer = 0;
    s_shop_feedback_visible = false;
    s_first_shop_special_pending = false;
    s_first_shop_entered = false;
}

static int game_shop_discounted_price(int base_price)
{
    return voucher_discount_price(&g_game_vars.vouchers, base_price);
}

static int game_shop_current_reroll_cost(void)
{
    return voucher_get_reroll_cost(&g_game_vars.vouchers, reroll_cost);
}

static void game_shop_draw_voucher_status(void)
{
    tte_erase_rect_wrapper(VOUCHER_STATUS_RECT);
    if (s_shop_voucher != NULL)
        return;

    int owned_count = 0;
    uint16_t owned = g_game_vars.vouchers.owned_mask & VOUCHER_OWNED_MASK;
    while (owned != 0)
    {
        owned_count += owned & 1U;
        owned >>= 1;
    }
    if (owned_count <= 0)
        return;

    tte_printf("#{P:80,112; cx:0x%X000}OWNED", TTE_GREEN_PB);
    tte_printf("#{P:96,124; cx:0x%X000}x%d", TTE_WHITE_PB, owned_count);
}

static enum AlchemicalId game_shop_roll_alchemical(void)
{
    enum AlchemicalRarity rarity;
    int roll = rng_get_u32() % 9;
    if (roll < 5)
        rarity = ALCHEMICAL_COMMON;
    else if (roll < 8)
        rarity = ALCHEMICAL_UNCOMMON;
    else
        rarity = ALCHEMICAL_RARE;

    int matching[ALCHEMICAL_ID_COUNT];
    int count = 0;
    for (int id = 0; id < ALCHEMICAL_ID_COUNT; id++)
    {
        const AlchemicalInfo* info = alchemical_get_info(id);
        if (info != NULL && info->rarity == rarity &&
            alchemical_is_shop_enabled((enum AlchemicalId)id))
            matching[count++] = id;
    }
    return count > 0 ? matching[rng_get_u32() % count] : ALCHEMICAL_IGNIS;
}

static enum PlanetId game_shop_roll_planet(void)
{
    enum PlanetId matching[PLANET_COUNT];
    int count = 0;
    for (int id = 0; id < PLANET_COUNT; id++)
    {
        enum PlanetId planet = (enum PlanetId)id;
        if (planet_is_unlocked(planet, g_game_vars.hand_play_counts) &&
            planet_can_apply(planet, g_game_vars.alchemy.hand_levels))
        {
            matching[count++] = planet;
        }
    }
    return count > 0 ? matching[rng_get_u32() % count] : PLANET_COUNT;
}

static void game_shop_erase_text_at_anchor(
    SpriteObject* sprite_object,
    int anchor_x,
    int anchor_y
)
{
    if (sprite_object == NULL)
        return;

    FIXED saved_tx = sprite_object->tx;
    FIXED saved_ty = sprite_object->ty;
    sprite_object->tx = int2fx(anchor_x);
    sprite_object->ty = int2fx(anchor_y);
    sprite_object_erase_text_under(sprite_object);
    sprite_object->tx = saved_tx;
    sprite_object->ty = saved_ty;
}

static void game_shop_destroy_alchemical(AlchemicalObject** object, bool erase_price)
{
    if (object == NULL || *object == NULL)
        return;
    if (erase_price)
    {
        if ((*object)->slot >= ALCHEMICAL_HELD_LIMIT)
            tte_erase_rect_wrapper(SPECIAL_PRICE_TEXT_RECT);
        else
            sprite_object_erase_text_under((*object)->sprite_object);
    }
    alchemical_object_destroy(object);
}

static void game_shop_destroy_planet(bool erase_price)
{
    if (s_shop_planet == NULL)
        return;
    if (erase_price)
        tte_erase_rect_wrapper(SPECIAL_PRICE_TEXT_RECT);
    planet_object_destroy(&s_shop_planet);
}

static void game_shop_print_price_at_anchor(
    SpriteObject* sprite_object,
    int price,
    int anchor_x,
    int anchor_y
)
{
    if (sprite_object == NULL)
        return;

    /*
     * Voucher and Alchemical art can be tuned independently of their labels.
     * Temporarily use the original placement only for calculating the text
     * rectangle, then restore the real sprite target.
     */
    FIXED saved_tx = sprite_object->tx;
    FIXED saved_ty = sprite_object->ty;
    sprite_object->tx = int2fx(anchor_x);
    sprite_object->ty = int2fx(anchor_y);
    sprite_object_print_price_under(sprite_object, price);
    sprite_object->tx = saved_tx;
    sprite_object->ty = saved_ty;
}

static void game_shop_print_joker_price(JokerObject* object)
{
    if (object == NULL || object->joker == NULL || object->sprite_object == NULL)
        return;
    game_shop_print_price_at_anchor(
        object->sprite_object,
        game_shop_discounted_price(object->joker->value),
        fx2int(object->sprite_object->tx),
        ITEM_SHOP_Y
    );
}

static void game_shop_erase_joker_price(JokerObject* object)
{
    if (object == NULL || object->sprite_object == NULL)
        return;
    game_shop_erase_text_at_anchor(
        object->sprite_object,
        fx2int(object->sprite_object->tx),
        ITEM_SHOP_Y
    );
}

static inline void game_shop_print_alchemical_price(
    AlchemicalObject* object,
    int price
)
{
    if (object == NULL)
        return;
    char price_text[INT_MAX_DIGITS + 2];
    snprintf(price_text, sizeof(price_text), "$%d", price);
    Rect text_rect = SPECIAL_PRICE_TEXT_RECT;
    update_text_rect_to_center_str(&text_rect, price_text, SCREEN_LEFT);
    tte_erase_rect_wrapper(SPECIAL_PRICE_TEXT_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        text_rect.left,
        SPECIAL_PRICE_TEXT_RECT.top,
        TTE_YELLOW_PB,
        price_text
    );
}

static inline void game_shop_print_planet_price(PlanetObject* object)
{
    if (object == NULL)
        return;
    char price_text[INT_MAX_DIGITS + 2];
    snprintf(
        price_text,
        sizeof(price_text),
        "$%d",
        game_shop_discounted_price(PLANET_BASE_COST)
    );
    Rect text_rect = SPECIAL_PRICE_TEXT_RECT;
    update_text_rect_to_center_str(&text_rect, price_text, SCREEN_LEFT);
    tte_erase_rect_wrapper(SPECIAL_PRICE_TEXT_RECT);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        text_rect.left,
        SPECIAL_PRICE_TEXT_RECT.top,
        TTE_YELLOW_PB,
        price_text
    );
}

static inline void game_shop_print_voucher_price(VoucherObject* object)
{
    if (object == NULL)
        return;
    game_shop_print_price_at_anchor(
        object->sprite_object,
        VOUCHER_BASE_COST,
        VOUCHER_PRICE_ANCHOR_X,
        VOUCHER_PRICE_ANCHOR_Y
    );
}

static void game_shop_sync_held_alchemicals(void)
{
    static const int held_x[ALCHEMICAL_HELD_LIMIT] = {175, 191, 207};
    alchemical_object_init();
    for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
    {
        game_shop_destroy_alchemical(&s_held_alchemicals[i], false);
        if (i >= g_game_vars.alchemy.count)
            continue;
        s_held_alchemicals[i] = alchemical_object_new(g_game_vars.alchemy.held[i], i);
        if (s_held_alchemicals[i] != NULL)
            sprite_object_position(s_held_alchemicals[i]->sprite_object, held_x[i], 16);
    }
}

static void game_shop_clear_alchemical_feedback(void)
{
    tte_erase_rect_wrapper(ALCHEMICAL_FEEDBACK_RECT);
    s_shop_feedback_timer = 0;
    s_shop_feedback_visible = false;
}

static void game_shop_show_alchemical_feedback(const char* message)
{
    game_shop_clear_alchemical_feedback();
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%s",
        ALCHEMICAL_FEEDBACK_RECT.left,
        ALCHEMICAL_FEEDBACK_RECT.top,
        TTE_YELLOW_PB,
        message
    );
    s_shop_feedback_timer = 0;
    s_shop_feedback_visible = true;
}

static void game_shop_destroy_alchemical_offers(void)
{
    for (int i = 0; i < MAX_SHOP_ALCHEMICALS; i++)
        game_shop_destroy_alchemical(&s_shop_alchemicals[i], true);
    game_shop_destroy_planet(true);
    game_shop_clear_alchemical_feedback();
}

static void game_shop_create_alchemicals(void)
{
    alchemical_object_init();
    planet_object_init();
    game_shop_destroy_alchemical_offers();

    bool guaranteed = s_first_shop_special_pending;
    if (!guaranteed && (rng_get_u32() % 100) >= SPECIAL_SHOP_CHANCE)
        return;

    enum PlanetId planet = PLANET_COUNT;
    if ((rng_get_u32() % 100) < PLANET_SHOP_CHANCE)
        planet = game_shop_roll_planet();
    if (planet != PLANET_COUNT)
    {
        s_shop_planet = planet_object_new(planet);
        if (s_shop_planet != NULL)
        {
            sprite_object_position(s_shop_planet->sprite_object, ALCHEMICAL_SHOP_X, 160);
            s_shop_planet->sprite_object->ty = int2fx(ALCHEMICAL_SHOP_Y);
            game_shop_print_planet_price(s_shop_planet);
            s_first_shop_special_pending = false;
            return;
        }
    }

    AlchemicalObject* object =
        alchemical_object_new(game_shop_roll_alchemical(), ALCHEMICAL_HELD_LIMIT);
    if (object == NULL)
        return;

    const AlchemicalInfo* info = alchemical_get_info(object->id);
    if (info == NULL)
    {
        alchemical_object_destroy(&object);
        return;
    }

    s_shop_alchemicals[0] = object;
    sprite_object_position(object->sprite_object, ALCHEMICAL_SHOP_X, 160);
    object->sprite_object->ty = int2fx(ALCHEMICAL_SHOP_Y);
    game_shop_print_alchemical_price(
        object,
        game_shop_discounted_price(info->cost)
    );
    /*
     * Consume the first-Shop guarantee only after a real object exists.  If a
     * transient OAM/pool shortage rejects both Planet and Alchemical creation,
     * the next refresh gets another attempt instead of silently presenting an
     * empty guaranteed slot.
     */
    s_first_shop_special_pending = false;
}

static void game_shop_create_voucher(void)
{
    voucher_object_init();
    if (s_shop_voucher != NULL)
    {
        game_shop_print_voucher_price(s_shop_voucher);
        return;
    }

    if (!voucher_prepare_offer(
            &g_game_vars.vouchers,
            g_game_vars.ante,
            rng_get_u32()
        ))
    {
        return;
    }

    enum VoucherId id = (enum VoucherId)g_game_vars.vouchers.offer_id;
    s_shop_voucher = voucher_object_new(id);
    if (s_shop_voucher == NULL)
        return;

    sprite_object_position(s_shop_voucher->sprite_object, VOUCHER_SHOP_X, 160);
    s_shop_voucher->sprite_object->ty = int2fx(VOUCHER_SHOP_Y);
    game_shop_print_voucher_price(s_shop_voucher);
}

static void game_shop_redraw_prices(bool snap_jokers)
{
    tte_erase_rect_wrapper(SHOP_PRICES_TEXT_RECT);

    ListItr itr = list_itr_create(&s_shop_jokers_list);
    JokerObject* joker_object;
    while ((joker_object = list_itr_next(&itr)))
    {
        if (snap_jokers)
        {
            joker_object->sprite_object->x = joker_object->sprite_object->tx;
            joker_object->sprite_object->y = joker_object->sprite_object->ty;
            joker_object->sprite_object->vx = 0;
            joker_object->sprite_object->vy = 0;
        }
        game_shop_print_joker_price(joker_object);
    }

    for (int i = 0; i < game_shop_alchemical_offer_count(); i++)
    {
        AlchemicalObject* object = s_shop_alchemicals[i];
        const AlchemicalInfo* info =
            object != NULL ? alchemical_get_info(object->id) : NULL;
        if (info != NULL)
            game_shop_print_alchemical_price(
                object,
                game_shop_discounted_price(info->cost)
            );
    }
    if (s_shop_planet != NULL)
        game_shop_print_planet_price(s_shop_planet);
    if (s_shop_voucher != NULL)
        game_shop_print_voucher_price(s_shop_voucher);
    else
        game_shop_draw_voucher_status();

    tte_printf(
        "#{P:%d,%d; cx:0x%X000}$%d",
        SHOP_REROLL_RECT.left,
        SHOP_REROLL_RECT.top,
        TTE_WHITE_PB,
        game_shop_current_reroll_cost()
    );
}

/**
 * @brief Set whether a Joker can appear in the shop.
 */
void game_shop_set_joker_avail(int joker_id, bool avail)
{
    bitset_set_idx(&s_avail_jokers_bitset, joker_id, avail);
}

void game_shop_change_background(void)
{
    toggle_windows(false, true);

    GRIT_CPY(pal_bg_mem, background_shop_gfxPal);
    GRIT_CPY(&tile_mem[MAIN_BG_CBB], background_shop_gfxTiles);
    GRIT_CPY(&se_mem[MAIN_BG_SBB], background_shop_gfxMap);

    // Set the outline colors for the shop background. This is used for the alternate shop
    // palettes when opening packs
    pal_bg_mem[SHOP_BOTTOM_PANEL_BORDER_PAL_IDX] = 0x213D;
    pal_bg_mem[SHOP_PANEL_SHADOW_PAL_IDX] = 0x10B4;

    // Reset the shop lights to correct colors
    pal_bg_mem[SHOP_LIGHTS_2_PAL_IDX] = SHOP_LIGHTS_2_CLR;
    pal_bg_mem[SHOP_LIGHTS_3_PAL_IDX] = SHOP_LIGHTS_3_CLR;
    pal_bg_mem[SHOP_LIGHTS_4_PAL_IDX] = SHOP_LIGHTS_4_CLR;
    pal_bg_mem[SHOP_LIGHTS_1_PAL_IDX] = SHOP_LIGHTS_1_CLR;

    // Disable the button highlight colors
    pal_bg_mem[REROLL_BTN_SELECTED_BORDER_PAL_IDX] = pal_bg_mem[REROLL_BTN_PAL_IDX];
    pal_bg_mem[NEXT_ROUND_BTN_SELECTED_BORDER_PAL_IDX] = pal_bg_mem[NEXT_ROUND_BTN_PAL_IDX];
}

void game_shop_on_init(void)
{
    game_shop_change_background();
    tte_erase_screen();
    display_status_panel();

    timer = TM_ZERO;
    /*
     * game_shop_create_items() is also used by reroll. Arm the guarantee only
     * once on entry so first-Shop rerolls use the normal 75% offer chance.
     * Run saves resume at Blind Select, so a process restart cannot re-arm a
     * completed first Shop.
     */
    s_first_shop_special_pending =
        g_game_vars.round == 1 && !s_first_shop_entered;
    if (g_game_vars.round == 1)
        s_first_shop_entered = true;
    game_shop_sync_held_alchemicals();

    state_machine_register(&shop_sm);
    state_machine_change_state(&shop_sm, GAME_SHOP_INTRO);

    // The selection grid is initialized outside of bounds and moved
    // to trigger the selection change so the initial selection is visible
    shop_selection_grid.selection = SHOP_INIT_SEL;
    selection_grid_move_selection_horz(&shop_selection_grid, 1);
}

/**
 * @brief Computes the number of Jokers we can currently roll in the Shop.
 *         The Jokers we own is taken into account and can't be rolled again.
 */
static inline int get_num_shop_jokers_avail(void)
{
    return bitset_num_set_bits(&s_avail_jokers_bitset);
}

/**
 * @brief Rolls a random Joker among the available ones
 */
static inline int game_shop_get_rand_available_joker_id(void)
{
    // Roll for what rarity the joker will be
    int joker_rarity = joker_get_random_rarity();

    // Now determine how many jokers are available based on the rarity
    int jokers_avail_size = get_num_shop_jokers_avail();

    if (jokers_avail_size == 0)
        return UNDEFINED;

    int matching_joker_ids[jokers_avail_size];
    int fallback_random_idx = rng_get_u32() % jokers_avail_size;
    int fallback_random_joker_id = UNDEFINED;
    int match_count = 0;

    BitsetItr itr = bitset_itr_create(&s_avail_jokers_bitset);

    int i = 0;
    int joker_id = UNDEFINED;
    while ((joker_id = bitset_itr_next(&itr)) != UNDEFINED)
    {
        if (i++ == fallback_random_idx)
            fallback_random_joker_id = joker_id;
        const JokerInfo* info = get_joker_registry_entry(joker_id);
        if (info->rarity == joker_rarity)
        {
            matching_joker_ids[match_count++] = joker_id;
        }
    }

    int selected_joker_id = (match_count > 0) ? matching_joker_ids[rng_get_u32() % match_count]
                                              : fallback_random_joker_id;

    return selected_joker_id;
}

/**
 * @brief Returns true if we can't roll any Joker
 */
static inline bool no_avail_jokers(void)
{
    return bitset_is_empty(&s_avail_jokers_bitset);
}

GBAL_UNUSED
static inline bool is_shop_joker_avail(int joker_id)
{
    return bitset_get_idx(&s_avail_jokers_bitset, joker_id);
}

static void game_shop_position_joker_offers(void)
{
    int count = list_get_len(&s_shop_jokers_list);
    if (count <= 0)
        return;

    int spacing = count == 2 ? 32 : (count == 3 ? 28 : 24);
    int start_x = 136 - ((count - 1) * spacing) / 2;
    ListItr itr = list_itr_create(&s_shop_jokers_list);
    JokerObject* joker_object;
    int index = 0;
    while ((joker_object = list_itr_next(&itr)))
    {
        if (joker_object->joker == NULL || joker_object->sprite_object == NULL)
            continue;
        int x = start_x + index++ * spacing;
        joker_object->sprite_object->tx = int2fx(x);
        if (joker_object->sprite_object->y == int2fx(SHOP_JOKER_SPRITES_INIT_POS.y))
            joker_object->sprite_object->x = int2fx(x);
    }
}

static void game_shop_fill_joker_offers(void)
{
    List* shop_jokers_list = &s_shop_jokers_list;
    int target_count = min(
        MAX_SHOP_JOKERS,
        voucher_get_shop_joker_slots(&g_game_vars.vouchers)
    );

    while (!no_avail_jokers() && list_get_len(shop_jokers_list) < target_count)
    {
        int joker_id = 0;
#ifdef TEST_JOKER_ID0 // Allow defining an ID for a joker to always appear in shop and be tested
        if (is_shop_joker_avail(TEST_JOKER_ID0))
        {
            joker_id = TEST_JOKER_ID0;
        }
        else
#endif
#ifdef TEST_JOKER_ID1
            if (is_shop_joker_avail(TEST_JOKER_ID1))
        {
            joker_id = TEST_JOKER_ID1;
        }
        else
#endif
        {
            joker_id = game_shop_get_rand_available_joker_id();
        }

        if (joker_id == UNDEFINED)
            break;

        game_shop_set_joker_avail(joker_id, false);

        Joker* joker = joker_new(joker_id);
        JokerObject* joker_object = joker_object_new(joker);
        if (joker_object == NULL)
        {
            joker_destroy(&joker);
            game_shop_set_joker_avail(joker_id, true);
            break;
        }

        joker_object->sprite_object->x = int2fx(SHOP_JOKER_SPRITES_INIT_POS.x);
        joker_object->sprite_object->y = int2fx(SHOP_JOKER_SPRITES_INIT_POS.y);
        joker_object->sprite_object->tx = joker_object->sprite_object->x;
        joker_object->sprite_object->ty = int2fx(ITEM_SHOP_Y);
        if (!list_push_back(shop_jokers_list, joker_object))
        {
            joker_object_destroy(&joker_object);
            game_shop_set_joker_avail(joker_id, true);
            break;
        }
    }

    game_shop_position_joker_offers();
    ListItr itr = list_itr_create(shop_jokers_list);
    JokerObject* joker_object;
    while ((joker_object = list_itr_next(&itr)))
    {
        if (joker_object->joker == NULL || joker_object->sprite_object == NULL)
            continue;
        /*
         * Only the shop intro animates fresh offers from below. Offers created
         * immediately by Overstock, reroll, or the debug menu must already be
         * at their target before their price text is positioned.
         */
        if (shop_sm.state != GAME_SHOP_INTRO)
        {
            joker_object->sprite_object->x = joker_object->sprite_object->tx;
            joker_object->sprite_object->y = joker_object->sprite_object->ty;
            joker_object->sprite_object->vx = 0;
            joker_object->sprite_object->vy = 0;
        }
        game_shop_print_joker_price(joker_object);
    }
}

/**
 * @brief Setup all purchasable items in the Shop.
 */
static void game_shop_create_items(void)
{
    tte_erase_rect_wrapper(SHOP_PRICES_TEXT_RECT);

    list_clear(&s_shop_jokers_list);
    s_shop_jokers_list = list_init();
    game_shop_fill_joker_offers();
    game_shop_create_voucher();
    game_shop_create_alchemicals();
    game_shop_draw_voucher_status();
}

/**
 * @brief Intro sequence (menu and shop icon coming into frame)
 */
static void game_shop_intro()
{
    main_bg_se_copy_rect_1_tile_vert(POP_MENU_ANIM_RECT, SCREEN_UP);

    if (timer == TM_CREATE_SHOP_ITEMS_WAIT)
    {
        game_shop_create_items();
    }

    if (timer >= TM_SHIFT_SHOP_ICON_WAIT) // Shift the shop icon
    {
        int timer_offset = timer - 6;

        // TODO: Extract to generic function?
        for (int y = 0; y < timer_offset; y++)
        {
            Rect from = SHOP_ICON_FROM_RECT;
            from.top += y - timer_offset;
            from.bottom += y - timer_offset;

            BG_POINT to = SHOP_ICON_TO_POS;
            to.y = y;

            main_bg_se_copy_rect(from, to);
        }
    }

    if (timer == TM_END_GAME_SHOP_INTRO)
    {
        state_machine_change_state(&shop_sm, GAME_SHOP_ACTIVE);
        timer = TM_ZERO; // Reset the timer

        // print initial reroll cost only when the panel is in place
        tte_printf(
            "#{P:%d,%d; cx:0x%X000}$%d",
            SHOP_REROLL_RECT.left,
            SHOP_REROLL_RECT.top,
            TTE_WHITE_PB,
            game_shop_current_reroll_cost()
        );
    }
}

/**
 * @brief Owned Jokers and held Alchemicals share the top inventory strip.
 */
static int shop_owned_row_get_size(void)
{
    return jokers_sel_row_get_size() + g_game_vars.alchemy.count;
}

static bool shop_owned_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    int joker_count = jokers_sel_row_get_size();
    bool prev_is_joker =
        prev_selection->y == row_idx && prev_selection->x >= 0 &&
        prev_selection->x < joker_count;
    bool new_is_joker =
        new_selection->y == row_idx && new_selection->x >= 0 &&
        new_selection->x < joker_count;

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
        if (slot >= 0 && slot < ALCHEMICAL_HELD_LIMIT &&
            s_held_alchemicals[slot] != NULL)
        {
            sprite_object_erase_text_under(s_held_alchemicals[slot]->sprite_object);
            alchemical_object_set_focus(s_held_alchemicals[slot], false);
        }
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
        if (slot >= 0 && slot < g_game_vars.alchemy.count &&
            s_held_alchemicals[slot] != NULL)
        {
            alchemical_object_set_focus(s_held_alchemicals[slot], true);
            sprite_object_print_price_under(
                s_held_alchemicals[slot]->sprite_object,
                alchemical_get_sell_value(g_game_vars.alchemy.held[slot])
            );
        }
    }

    return true;
}

static void shop_owned_row_on_key_transit(
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

    if (key_hit(SELL_KEY))
    {
        enum AlchemicalId id = g_game_vars.alchemy.held[slot];
        int sell_value = alchemical_get_sell_value(id);
        if (s_held_alchemicals[slot] != NULL)
            sprite_object_erase_text_under(s_held_alchemicals[slot]->sprite_object);

        /* Leave the shrinking inventory row before removing its selected item. */
        selection_grid_move_selection_vert(selection_grid, SCREEN_DOWN);
        if (alchemical_inventory_sell(&g_game_vars.alchemy, slot))
        {
            g_game_vars.money =
                g_game_vars.money > INT_MAX - sell_value
                    ? INT_MAX
                    : g_game_vars.money + sell_value;
            display_money();
            game_shop_sync_held_alchemicals();
            game_shop_show_alchemical_feedback("Alchemical sold");
        }
    }
}

/**
 * @brief Computes the number of Jokers up for sale, aka the number of
 *         "buttons" there are on the top row of the Shop.
 */
static int shop_top_row_get_size(void)
{
    // + 1 to account for next round button
    return list_get_len(&s_shop_jokers_list) + 1;
}

/**
 * @brief Add a newly purchased Joker to the list of owned Jokers.
 */
static inline bool add_to_held_jokers(JokerObject* joker_object)
{
    if (joker_object == NULL || joker_object->joker == NULL ||
        joker_object->sprite_object == NULL)
        return false;
    if (!add_joker(joker_object))
        return false;
    joker_object->sprite_object->ty = int2fx(HELD_JOKERS_POS.y);
    return true;
}

/**
 * @brief Called when pressing A on a Shop Joker to buy it.
 */
static inline bool game_shop_buy_joker(int shop_joker_idx)
{
    List* shop_jokers_list = &s_shop_jokers_list;
    JokerObject* joker_object = (JokerObject*)list_get_at_idx(shop_jokers_list, shop_joker_idx);
    if (joker_object == NULL || joker_object->joker == NULL ||
        joker_object->sprite_object == NULL)
        return false;

    int price = game_shop_discounted_price(joker_object->joker->value);
    if (!game_can_add_joker(joker_object->joker) || price < 0 ||
        g_game_vars.money < price)
    {
        return false;
    }

    /*
     * Remove the shop focus before assigning the held-row baseline.  Doing it
     * in the opposite order made set_focus(false) add its 10px raise back to
     * the new baseline, so a freshly bought (often rightmost) Joker sat lower
     * than every other owned Joker.
     */
    game_shop_erase_joker_price(joker_object);
    joker_object_set_focus(joker_object, false);
    if (!add_to_held_jokers(joker_object))
    {
        /* The offer stays in the shop if the owned-list insertion fails. */
        joker_object->sprite_object->ty = int2fx(ITEM_SHOP_Y);
        joker_object_set_focus(joker_object, true);
        game_shop_print_joker_price(joker_object);
        return false;
    }

    g_game_vars.money -= price;
    display_money();
    list_remove_at_idx(shop_jokers_list, shop_joker_idx); // Remove the joker from the shop
    return true;
}

/**
 * @brief Handle button inputs for the "Next Round" button and shop Jokers.
 */
static void shop_top_row_on_key_transit(SelectionGrid* selection_grid, Selection* selection)
{
    if (!key_hit(SELECT_CARD))
        return;

    if (selection->x == NEXT_ROUND_BTN_SEL_X)
    {
        button_press(&next_round_button);
    }
    else
    {
        int shop_joker_idx = selection->x - 1; // - 1 to account for next round button
        JokerObject* joker_object =
            (JokerObject*)list_get_at_idx(&s_shop_jokers_list, shop_joker_idx);
        if (joker_object == NULL || !game_can_add_joker(joker_object->joker) ||
            g_game_vars.money <
                game_shop_discounted_price(joker_object->joker->value))
        {
            if (joker_object != NULL)
            {
                game_shop_show_alchemical_feedback(
                    !game_can_add_joker(joker_object->joker)
                        ? "Joker slots full"
                        : "Not enough money"
                );
            }
            return;
        }

        if (game_shop_buy_joker(shop_joker_idx))
            selection_grid_move_selection_horz(selection_grid, -1);
    }
}

static int shop_voucher_row_get_size(void)
{
    return s_shop_voucher != NULL ? 1 : 0;
}

static bool shop_voucher_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    if (s_shop_voucher == NULL)
        return true;
    if (prev_selection->y == row_idx)
        voucher_object_set_focus(s_shop_voucher, false);
    if (new_selection->y == row_idx)
        voucher_object_set_focus(s_shop_voucher, true);
    return true;
}

static void shop_voucher_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
)
{
    if (!key_hit(SELECT_CARD) || s_shop_voucher == NULL)
    {
        return;
    }
    if (g_game_vars.money < VOUCHER_BASE_COST)
    {
        game_shop_show_alchemical_feedback("Not enough money");
        return;
    }

    VoucherObject* purchased = s_shop_voucher;
    if (!voucher_buy_offer(&g_game_vars.vouchers))
        return;

    g_game_vars.money -= VOUCHER_BASE_COST;
    display_money();
    game_shop_erase_text_at_anchor(
        purchased->sprite_object,
        VOUCHER_PRICE_ANCHOR_X,
        VOUCHER_PRICE_ANCHOR_Y
    );
    voucher_object_set_focus(purchased, false);

    description_card = NULL;
    description_alchemical = NULL;
    description_planet = NULL;
    description_voucher = purchased;
    description_is_purchase = true;
    description_card_original_list = NULL;
    description_card_original_x_pos = purchased->sprite_object->x;
    description_card_original_y_pos = purchased->sprite_object->y;
    voucher_object_set_description_scale(purchased, true);
    timer = TM_ZERO;
    state_machine_change_state(&shop_sm, GAME_SHOP_SHOW_CARD_DESC);
}

/**
 * @brief Handle d-pad inputs for the "Next Round" button and shop Jokers' row.
 */
static bool shop_top_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    List* shop_jokers_list = &s_shop_jokers_list;

    // The selection grid system only guarantees that the new selection is within bounds
    // but not the previous one...
    // This allows using INIT_SEL = {-1, 1} and move to set the initial selection in a hacky way...
    if (prev_selection->y == row_idx && prev_selection->x >= 0 &&
        prev_selection->x < shop_top_row_get_size())
    {
        if (prev_selection->x == NEXT_ROUND_BTN_SEL_X)
        {
            button_set_highlight(&next_round_button, false);
        }
        else
        {
            int idx = prev_selection->x - 1; // -1 to account for next round button
            JokerObject* joker_object = (JokerObject*)list_get_at_idx(shop_jokers_list, idx);
            if (joker_object != NULL)
                joker_object_set_focus(joker_object, false);
        }
    }

    if (new_selection->y == row_idx)
    {
        if (new_selection->x == NEXT_ROUND_BTN_SEL_X)
        {
            button_set_highlight(&next_round_button, true);
        }
        else
        {
            int idx = new_selection->x - 1; // -1 to account for next round button
            JokerObject* joker_object = (JokerObject*)list_get_at_idx(shop_jokers_list, idx);
            if (joker_object != NULL)
                joker_object_set_focus(joker_object, true);
        }
    }

    return true;
}

static int shop_alchemical_row_get_size(void)
{
    return game_shop_special_offer_count();
}

static bool shop_alchemical_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    if (s_shop_planet != NULL)
    {
        if (prev_selection->y == row_idx)
            planet_object_set_focus(s_shop_planet, false);
        if (new_selection->y == row_idx)
            planet_object_set_focus(s_shop_planet, true);
        return true;
    }

    if (prev_selection->y == row_idx && prev_selection->x >= 0 &&
        prev_selection->x < MAX_SHOP_ALCHEMICALS)
    {
        AlchemicalObject* object = s_shop_alchemicals[prev_selection->x];
        if (object != NULL)
            alchemical_object_set_focus(object, false);
    }

    if (new_selection->y == row_idx && new_selection->x >= 0 &&
        new_selection->x < game_shop_alchemical_offer_count())
    {
        AlchemicalObject* object = s_shop_alchemicals[new_selection->x];
        if (object != NULL)
            alchemical_object_set_focus(object, true);
    }

    return true;
}

static void game_shop_remove_alchemical_offer(int index)
{
    if (index < 0 || index >= game_shop_alchemical_offer_count())
        return;

    game_shop_destroy_alchemical(&s_shop_alchemicals[index], true);
}

static void game_shop_remove_planet_offer(void)
{
    game_shop_destroy_planet(true);
}

static void shop_alchemical_row_on_key_transit(
    SelectionGrid* selection_grid,
    Selection* selection
)
{
    if (!key_hit(SELECT_CARD) || selection->x < 0 ||
        selection->x >= game_shop_special_offer_count())
    {
        return;
    }

    if (s_shop_planet != NULL)
    {
        int price = game_shop_discounted_price(PLANET_BASE_COST);
        if (g_game_vars.money < price)
        {
            game_shop_show_alchemical_feedback("Not enough money");
            return;
        }

        enum PlanetId id = s_shop_planet->id;
        const PlanetInfo* info = planet_get_info(id);
        if (info == NULL ||
            !planet_apply(id, g_game_vars.alchemy.hand_levels))
        {
            game_shop_show_alchemical_feedback("Already at max level");
            return;
        }

        g_game_vars.money -= price;
        display_money();
        planet_object_set_focus(s_shop_planet, false);
        game_shop_remove_planet_offer();

        char feedback[48];
        if (id == PLANET_NEPTUNE)
            snprintf(feedback, sizeof(feedback), "Straight/Royal up");
        else
            snprintf(
                feedback,
                sizeof(feedback),
                "%s LV %d",
                info->hand_name,
                planet_get_display_level(id, g_game_vars.alchemy.hand_levels)
            );
        game_shop_show_alchemical_feedback(feedback);
        selection_grid->selection = (Selection){NEXT_ROUND_BTN_SEL_X, 1};
        button_set_highlight(&next_round_button, true);
        return;
    }

    AlchemicalObject* object = s_shop_alchemicals[selection->x];
    const AlchemicalInfo* info = object != NULL ? alchemical_get_info(object->id) : NULL;
    if (info == NULL)
        return;
    if (g_game_vars.alchemy.count >= ALCHEMICAL_HELD_LIMIT)
    {
        game_shop_show_alchemical_feedback("Consumables full");
        return;
    }
    int price = game_shop_discounted_price(info->cost);
    if (g_game_vars.money < price)
    {
        game_shop_show_alchemical_feedback("Not enough money");
        return;
    }
    if (!alchemical_inventory_add(&g_game_vars.alchemy, object->id))
        return;

    int purchased_index = selection->x;
    g_game_vars.money -= price;
    display_money();
    alchemical_object_set_focus(object, false);
    game_shop_remove_alchemical_offer(purchased_index);
    game_shop_sync_held_alchemicals();
    game_shop_clear_alchemical_feedback();

    int remaining = game_shop_alchemical_offer_count();
    if (remaining > 0)
    {
        selection_grid->selection.x = min(purchased_index, remaining - 1);
        alchemical_object_set_focus(
            s_shop_alchemicals[selection_grid->selection.x],
            true
        );
    }
    else
    {
        selection_grid->selection = (Selection){NEXT_ROUND_BTN_SEL_X, 1};
        button_set_highlight(&next_round_button, true);
    }
}

/**
 * @brief Get size of the row with the "Reroll" button
 *
 * @returns Always 1, because there is only the "Reroll" button on that row.
 */
static int shop_reroll_row_get_size()
{
    return 1;
}

/**
 * @brief Handle d-pad inputs for the "Reroll" button's row.
 */
static bool shop_reroll_row_on_selection_changed(
    SelectionGrid* selection_grid,
    int row_idx,
    const Selection* prev_selection,
    const Selection* new_selection
)
{
    if (row_idx == prev_selection->y)
    {
        button_set_highlight(&reroll_button, false);
    }
    else if (row_idx == new_selection->y)
    {
        button_set_highlight(&reroll_button, true);
    }

    return true;
}

/**
 * @brief Reroll items up for sale in the Shop.
 *         Reroll cost will go up by a rate that increases by 1 each reroll.
 */
static inline void game_shop_reroll(void)
{
    g_game_vars.money -= game_shop_current_reroll_cost();
    display_money(); // Update the money display

    List* shop_jokers_list = &s_shop_jokers_list;
    ListItr itr = list_itr_create(shop_jokers_list);
    JokerObject* joker_object;

    while ((joker_object = list_itr_next(&itr)))
    {
        if (joker_object != NULL && joker_object->joker != NULL)
        {
            game_shop_set_joker_avail(joker_object->joker->id, true);
            joker_object_destroy(&joker_object); // Destroy the joker object if it exists
        }
        else
        {
            joker_object_destroy(&joker_object);
        }
    }

    list_clear(shop_jokers_list);
    *shop_jokers_list = list_init();
    game_shop_destroy_alchemical_offers();

    game_shop_create_items();

    itr = list_itr_create(shop_jokers_list);

    while ((joker_object = list_itr_next(&itr)))
    {
        if (joker_object != NULL && joker_object->sprite_object != NULL)
        {
            // Set the y position to the target position
            joker_object->sprite_object->y = joker_object->sprite_object->ty;

            // Give the joker a little wiggle animation
            joker_object_shake(joker_object, UNDEFINED);
        }
    }

    if (reroll_cost < INT_MAX)
        reroll_cost++;
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}$%d",
        SHOP_REROLL_RECT.left,
        SHOP_REROLL_RECT.top,
        TTE_WHITE_PB,
        game_shop_current_reroll_cost()
    );
}

/**
 * @brief Handle button inputs for the "Reroll" button's row.
 */
static void shop_reroll_row_on_key_transit(SelectionGrid* selection_grid, Selection* selection)
{
    if (!key_hit(SELECT_CARD))
    {
        return;
    }

    button_press(&reroll_button);
}

static void next_round_on_pressed(void)
{
    // Go to next blind selection game state
    state_machine_change_state(&shop_sm, GAME_SHOP_EXIT);
    timer = TM_ZERO;
    reroll_cost = REROLL_BASE_COST;

    pal_bg_mem[NEXT_ROUND_BTN_SELECTED_BORDER_PAL_IDX] = pal_bg_mem[SHOP_PANEL_SHADOW_PAL_IDX];
}

static void reroll_on_pressed(void)
{
    // TODO: Add money sound effect
    game_shop_reroll();
}

static bool reroll_can_be_pressed(void)
{
    return g_game_vars.money >= game_shop_current_reroll_cost();
}

/*
 * Change shop rows using explicit coordinates. The generic SelectionGrid
 * preserves a relative X position between rows; that is useful for hands, but
 * ambiguous here because the top shop row also contains the Next Round button.
 * In particular, returning from the single Alchemical slot maps to x=0 (Next
 * Round) instead of the visually nearest Joker.
 */
static void game_shop_set_selection(Selection new_selection)
{
    Selection prev_selection = shop_selection_grid.selection;
    if (prev_selection.y < 0 || prev_selection.y >= shop_selection_grid.num_rows ||
        new_selection.y < 0 || new_selection.y >= shop_selection_grid.num_rows ||
        shop_selection_grid.rows[new_selection.y].get_size == NULL)
    {
        return;
    }

    int new_row_size = shop_selection_grid.rows[new_selection.y].get_size();
    if (new_selection.x < 0 || new_selection.x >= new_row_size)
        return;

    bool proceed = true;
    const SelectionGridRow* prev_row = &shop_selection_grid.rows[prev_selection.y];
    const SelectionGridRow* new_row = &shop_selection_grid.rows[new_selection.y];
    if (prev_row->on_selection_changed != NULL)
    {
        proceed = prev_row->on_selection_changed(
            &shop_selection_grid,
            prev_row->row_idx,
            &prev_selection,
            &new_selection
        );
    }
    if (proceed && new_row->on_selection_changed != NULL)
    {
        proceed = new_row->on_selection_changed(
            &shop_selection_grid,
            new_row->row_idx,
            &prev_selection,
            &new_selection
        );
    }
    if (proceed)
        shop_selection_grid.selection = new_selection;
}

/**
 * @brief Handle user inputs logic in the Shop though a SelectionGrid.
 */
static void game_shop_process_user_input(void)
{
    Selection selection = shop_selection_grid.selection;
    bool has_voucher = s_shop_voucher != NULL;
    bool has_alchemical = game_shop_special_offer_count() > 0;

    /*
     * Treat controls and merchandise as two separate navigation groups:
     *
     *   Next Round  <->  Shop Jokers
     *       |               |
     *     Reroll   <->  Voucher <-> Alchemical
     *
     * A Voucher is therefore never an intermediate stop between Next Round
     * and Reroll. Down from any Joker still reaches the Alchemical directly.
     */
    if (key_hit(KEY_DOWN) && selection.y == 0 && has_alchemical)
    {
        /* A direct, predictable path from owned cards to the special offer. */
        game_shop_set_selection((Selection){0, 2});
    }
    else if (key_hit(KEY_DOWN) && selection.y == 1 &&
             selection.x > NEXT_ROUND_BTN_SEL_X && has_alchemical)
    {
        /*
         * Do not let SelectionGrid preserve the Joker column here: the
         * special-offer row has one element, so every shop Joker goes
         * directly to that element.
         */
        game_shop_set_selection((Selection){0, 2});
    }
    else if (key_hit(KEY_DOWN) && selection.y == 1 &&
             selection.x == NEXT_ROUND_BTN_SEL_X)
    {
        game_shop_set_selection((Selection){0, 4});
    }
    else if (key_hit(KEY_UP) && selection.y == 3)
    {
        int first_joker_x = shop_top_row_get_size() > 1 ? 1 : 0;
        game_shop_set_selection((Selection){first_joker_x, 1});
    }
    else if (key_hit(KEY_UP) && selection.y == 2)
    {
        game_shop_set_selection((Selection){shop_top_row_get_size() - 1, 1});
    }
    else if (key_hit(KEY_RIGHT) && selection.y == 3 && has_alchemical)
    {
        game_shop_set_selection((Selection){0, 2});
    }
    else if (key_hit(KEY_LEFT) && selection.y == 2)
    {
        game_shop_set_selection((Selection){0, has_voucher ? 3 : 4});
    }
    else if (key_hit(KEY_LEFT) && selection.y == 3)
    {
        game_shop_set_selection((Selection){0, 4});
    }
    else if (key_hit(KEY_RIGHT) && selection.y == 4 &&
             (has_voucher || has_alchemical))
    {
        game_shop_set_selection((Selection){0, has_voucher ? 3 : 2});
    }
    else if (key_hit(KEY_UP) && selection.y == 4)
    {
        game_shop_set_selection((Selection){NEXT_ROUND_BTN_SEL_X, 1});
    }
    else if (key_hit(KEY_DOWN) && (selection.y == 2 || selection.y == 3))
    {
        /* Lower merchandise stays in its own group; Down has no hidden jump. */
    }
    else
    {
        selection_grid_process_input(&shop_selection_grid);
    }

    static JokerObject* tmp_card = NULL;
    AlchemicalObject* tmp_alchemical = NULL;
    PlanetObject* tmp_planet = NULL;
    VoucherObject* tmp_voucher = NULL;
    tmp_card = NULL;

    // Determine the Joker we would show the description of
    switch (shop_selection_grid.selection.y)
    {
        // Owned Joker
        case 0:
        {
            int joker_count = jokers_sel_row_get_size();
            if (shop_selection_grid.selection.x < joker_count)
            {
                description_card_original_list = get_jokers_list();
                tmp_card =
                    list_get_at_idx(get_jokers_list(), shop_selection_grid.selection.x);
            }
            else
            {
                description_card_original_list = NULL;
                int slot = shop_selection_grid.selection.x - joker_count;
                if (slot >= 0 && slot < g_game_vars.alchemy.count)
                    tmp_alchemical = s_held_alchemicals[slot];
            }
            break;
        }

        // Jokers for sale
        case 1:
        {
            description_card_original_list = &s_shop_jokers_list;
            tmp_card = (shop_selection_grid.selection.x > 0)
                         ? list_get_at_idx(
                               &s_shop_jokers_list,
                               shop_selection_grid.selection.x - 1
                           )
                         : NULL;
            break;
        }

        // Alchemical or Planet for sale
        case 2:
        {
            description_card_original_list = NULL;
            if (s_shop_planet != NULL)
                tmp_planet = s_shop_planet;
            else
            {
                int index = shop_selection_grid.selection.x;
                if (index >= 0 && index < game_shop_alchemical_offer_count())
                    tmp_alchemical = s_shop_alchemicals[index];
            }
            break;
        }

        // Voucher for sale
        case 3:
        {
            description_card_original_list = NULL;
            tmp_voucher = s_shop_voucher;
            break;
        }

        default:
        {
            description_card_original_list = NULL;
            tmp_card = NULL;
            break;
        }
    }

    // Show description of selected card when pressing B.
    // Always wait for the card in question to be immobile to avoid accumulating
    // errors when pressing and releasing B in quick succession.
    SpriteObject* tmp_sprite =
        tmp_voucher != NULL
            ? tmp_voucher->sprite_object
            : (tmp_planet != NULL
                   ? tmp_planet->sprite_object
                   : (tmp_alchemical != NULL
                          ? tmp_alchemical->sprite_object
                          : (tmp_card != NULL ? tmp_card->sprite_object : NULL)));
    if (tmp_sprite != NULL && tmp_sprite->vx == 0 && tmp_sprite->vy == 0 &&
        key_held(DESELECT_CARDS))
    {
        description_card = tmp_card;
        description_alchemical = tmp_alchemical;
        description_planet = tmp_planet;
        description_voucher = tmp_voucher;
        description_is_purchase = false;
        description_card_original_x_pos = tmp_sprite->x;
        description_card_original_y_pos = tmp_sprite->y;
        if (description_voucher != NULL)
            voucher_object_set_description_scale(description_voucher, true);
        if (description_planet != NULL)
            planet_object_set_description_scale(description_planet, true);
        if (description_alchemical != NULL)
            alchemical_object_set_description_scale(description_alchemical, true);
        timer = TM_ZERO;
        state_machine_change_state(&shop_sm, GAME_SHOP_SHOW_CARD_DESC);
    }
}

static void game_shop_show_card_desc(void)
{
    SpriteObject* description_sprite = game_shop_get_description_sprite();
    if (description_sprite == NULL)
    {
        state_machine_change_state(&shop_sm, GAME_SHOP_ACTIVE);
        return;
    }

    // Anim start
    if (timer == 1)
    {
        // Erase shop text and disable transparency window

        tte_erase_rect_wrapper(PLAYING_SCREEN_RECT);
        game_shop_clear_alchemical_feedback();
        toggle_windows(false, true);

        // Move all other Jokers offscreen

        JokerObject* joker_object = NULL;

        // Owned Jokers
        ListItr itr = list_itr_create(get_jokers_list());
        while ((joker_object = list_itr_next(&itr)))
        {
            if (joker_object != description_card &&
                joker_object->sprite_object != NULL)
                joker_object->sprite_object->ty -= int2fx(OWNED_CARDS_HIDE_Y_OFFSET);
        }

        // Shop Jokers
        itr = list_itr_create(&s_shop_jokers_list);
        while ((joker_object = list_itr_next(&itr)))
        {
            if (joker_object != description_card &&
                joker_object->sprite_object != NULL)
                joker_object->sprite_object->ty = int2fx(SHOP_JOKER_SPRITES_INIT_POS.y + TILE_SIZE);
        }

        for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
            if (s_held_alchemicals[i] != NULL && s_held_alchemicals[i] != description_alchemical)
            {
                s_held_alchemicals[i]->sprite_object->ty -= int2fx(OWNED_CARDS_HIDE_Y_OFFSET);
                if (s_held_alchemicals[i]->sprite_object->sprite != NULL)
                    obj_hide(s_held_alchemicals[i]->sprite_object->sprite->obj);
            }
        for (int i = 0; i < game_shop_alchemical_offer_count(); i++)
            if (s_shop_alchemicals[i] != NULL &&
                s_shop_alchemicals[i] != description_alchemical)
            {
                s_shop_alchemicals[i]->sprite_object->ty =
                    int2fx(SHOP_JOKER_SPRITES_INIT_POS.y + TILE_SIZE);
                if (s_shop_alchemicals[i]->sprite_object->sprite != NULL)
                    obj_hide(s_shop_alchemicals[i]->sprite_object->sprite->obj);
            }
        if (s_shop_planet != NULL && s_shop_planet != description_planet)
        {
            s_shop_planet->sprite_object->ty =
                int2fx(SHOP_JOKER_SPRITES_INIT_POS.y + TILE_SIZE);
            if (s_shop_planet->sprite_object->sprite != NULL)
                obj_hide(s_shop_planet->sprite_object->sprite->obj);
        }
        if (s_shop_voucher != NULL && s_shop_voucher != description_voucher)
        {
            s_shop_voucher->sprite_object->ty =
                int2fx(SHOP_JOKER_SPRITES_INIT_POS.y + TILE_SIZE);
            if (s_shop_voucher->sprite_object->sprite != NULL)
                obj_hide(s_shop_voucher->sprite_object->sprite->obj);
        }

        description_sprite->tx = int2fx(CARD_DESCRIPTION_SPRITE_POS.x);
        description_sprite->ty = int2fx(CARD_DESCRIPTION_SPRITE_POS.y);
    }

    // First 12 anim frames
    if (timer <= TM_SHOW_CARD_DESC_WAIT)
    {
        // Hide Deck (last 5 frames only)
        if (TM_SHOW_CARD_DESC_WAIT - timer < 5)
            main_bg_se_move_rect_1_tile_vert(DECK_ANIM_RECT, SCREEN_DOWN);
        // Hide shop panel
        main_bg_se_move_rect_1_tile_vert(POP_MENU_ANIM_RECT, SCREEN_DOWN);
        // Hide Owned Cards panels
        main_bg_se_move_rect_1_tile_vert(OWNED_CARDS_PANEL_RECT, SCREEN_UP);
    }

    // Anim end
    else if (timer == TM_SHOW_CARD_DESC_WAIT + 1)
    {
        const char* rarity_str = NULL;
        char rarity_label[24] = {0};
        char item_desc[128] = {0};
        int desc_bottom_offset = 0;
        const JokerInfo* info = NULL;
        const AlchemicalInfo* alchemical_info = NULL;
        const PlanetInfo* planet_info = NULL;
        const VoucherInfo* voucher_info = NULL;
        const char* name = NULL;
        u8 rarity = COMMON_JOKER;
        if (description_planet != NULL)
        {
            planet_info = planet_get_info(description_planet->id);
            if (planet_info == NULL)
            {
                timer = TM_ZERO;
                state_machine_change_state(&shop_sm, GAME_SHOP_HIDE_CARD_DESC);
                return;
            }
            snprintf(
                rarity_label,
                sizeof(rarity_label),
                "PLANET  $%d",
                game_shop_discounted_price(PLANET_BASE_COST)
            );
            rarity_str = rarity_label;
            snprintf(
                item_desc,
                sizeof(item_desc),
                TTE_BLACK_TAG "%s. Applies immediately",
                planet_info->description
            );
            int desc_height = tte_printf_justified_in_rect(
                item_desc,
                CARD_DESC_TEXT_RECT,
                JUSTIFY_CENTER,
                SCREEN_LEFT,
                false
            );
            desc_bottom_offset = max(0, CARD_DESC_MAX_TEXT_HEIGHT - desc_height);
            name = planet_info->name;
            rarity = UNCOMMON_JOKER;
        }
        else if (description_alchemical != NULL)
        {
            alchemical_info = alchemical_get_info(description_alchemical->id);
            if (alchemical_info == NULL)
            {
                timer = TM_ZERO;
                state_machine_change_state(&shop_sm, GAME_SHOP_HIDE_CARD_DESC);
                return;
            }
            snprintf(
                rarity_label,
                sizeof(rarity_label),
                description_alchemical->slot < ALCHEMICAL_HELD_LIMIT
                    ? "%s  SELL $%d"
                    : "%s  $%d",
                alchemical_rarity_name(alchemical_info->rarity),
                description_alchemical->slot < ALCHEMICAL_HELD_LIMIT
                    ? alchemical_get_sell_value(description_alchemical->id)
                    : game_shop_discounted_price(alchemical_info->cost)
            );
            rarity_str = rarity_label;
            snprintf(
                item_desc,
                sizeof(item_desc),
                TTE_BLACK_TAG "%s",
                alchemical_info->description
            );
            int desc_height = tte_printf_justified_in_rect(
                item_desc,
                CARD_DESC_TEXT_RECT,
                JUSTIFY_CENTER,
                SCREEN_LEFT,
                false
            );
            desc_bottom_offset = max(0, CARD_DESC_MAX_TEXT_HEIGHT - desc_height);
            name = alchemical_info->name;
            rarity = alchemical_info->rarity;
        }
        else if (description_voucher != NULL)
        {
            voucher_info = voucher_get_info(description_voucher->id);
            if (voucher_info == NULL)
            {
                timer = TM_ZERO;
                state_machine_change_state(&shop_sm, GAME_SHOP_HIDE_CARD_DESC);
                return;
            }
            snprintf(rarity_label, sizeof(rarity_label), "VOUCHER  $%d", voucher_info->cost);
            rarity_str = rarity_label;
            snprintf(
                item_desc,
                sizeof(item_desc),
                TTE_BLACK_TAG "%s",
                voucher_info->description
            );
            int desc_height = tte_printf_justified_in_rect(
                item_desc,
                CARD_DESC_TEXT_RECT,
                JUSTIFY_CENTER,
                SCREEN_LEFT,
                false
            );
            desc_bottom_offset = max(0, CARD_DESC_MAX_TEXT_HEIGHT - desc_height);
            name = voucher_info->name;
            rarity = UNCOMMON_JOKER;
        }
        else
        {
            if (description_card == NULL || description_card->joker == NULL)
            {
                timer = TM_ZERO;
                state_machine_change_state(&shop_sm, GAME_SHOP_HIDE_CARD_DESC);
                return;
            }

            info = get_joker_registry_entry(description_card->joker->id);
            if (info == NULL || info->joker_print_desc == NULL)
            {
                timer = TM_ZERO;
                state_machine_change_state(&shop_sm, GAME_SHOP_HIDE_CARD_DESC);
                return;
            }

            int desc_height =
                info->joker_print_desc(description_card->joker, CARD_DESC_TEXT_RECT);
            desc_bottom_offset = max(0, CARD_DESC_MAX_TEXT_HEIGHT - desc_height);
            const char* joker_rarity = joker_get_rarity_string(info->rarity);
            if (joker_rarity == NULL)
                joker_rarity = "Unknown";
            if (description_card->joker->modifier == BASE_EDITION)
            {
                rarity_str = joker_rarity;
            }
            else
            {
                snprintf(
                    rarity_label,
                    sizeof(rarity_label),
                    "%s",
                    joker_get_edition_effect_short(description_card->joker->modifier)
                );
                rarity_str = rarity_label;
            }
            name = info->name;
            rarity = info->rarity;
        }

        if (rarity_str == NULL)
            rarity_str = "UNKNOWN";
        if (name == NULL)
            name = "Unknown Joker";

        int rarity_width = rect_width(&CARD_DESC_TEXT_RECT);
        int rarity_len = min(rarity_width, (int)strlen(rarity_str));
        int rarity_padding = max(0, (rarity_width - rarity_len) / 2);
        tte_printf(
            TTE_WHITE_TAG "#{P:%d,%d}%*s%.*s",
            CARD_DESC_TEXT_RECT.left * TILE_SIZE,
            (CARD_DESC_TEXT_RECT.bottom - desc_bottom_offset - 1) * TILE_SIZE,
            rarity_padding,
            "",
            rarity_len,
            rarity_str
        );
        pal_bg_mem[SHOP_DESC_RARITY_MAIN_COLOR_PAL_IDX] =
            joker_get_rarity_color(rarity, true);
        pal_bg_mem[SHOP_DESC_RARITY_SHADOW_COLOR_PAL_IDX] =
            joker_get_rarity_color(rarity, false);

        // Draw description panel
        Rect actual_dest_rect = CARD_DESC_9_PTCH_TO_RECT;
        actual_dest_rect.bottom -= desc_bottom_offset;
        main_bg_se_copy_expand_9_patch(actual_dest_rect, &CARD_DESC_9_PTCH_SRC);

        int name_width = rect_width(&CARD_NAME_TEXT_RECT);
        int name_len = min(name_width, (int)strlen(name));
        int name_padding = max(0, (name_width - name_len) / 2);
        tte_printf(
            TTE_WHITE_TAG "#{P:%d,%d}%*s%.*s",
            CARD_NAME_TEXT_RECT.left * TILE_SIZE,
            CARD_NAME_TEXT_RECT.top * TILE_SIZE,
            name_padding,
            "",
            name_len,
            name
        );

        if (description_planet != NULL || description_alchemical != NULL ||
            description_voucher != NULL)
        {
            tte_printf_justified_in_rect(
                item_desc,
                CARD_DESC_TEXT_RECT,
                JUSTIFY_CENTER,
                SCREEN_LEFT,
                true
            );
        }
    }

    // Actively wait for the B button to be released, but only if the described card has stopped
    // moving
    else if (
        description_sprite->vx == 0 && description_sprite->vy == 0 &&
        ((description_is_purchase && key_hit(SELECT_CARD)) ||
         (!description_is_purchase && !key_held(DESELECT_CARDS)))
    )
    {
        timer = TM_ZERO;
        state_machine_change_state(&shop_sm, GAME_SHOP_HIDE_CARD_DESC);
    }
}

static void game_shop_hide_card_desc(void)
{
    SpriteObject* description_sprite = game_shop_get_description_sprite();
    if (description_sprite == NULL)
    {
        state_machine_change_state(&shop_sm, GAME_SHOP_ACTIVE);
        return;
    }

    // just so we don't print the price of an owned Joker too many times
    static bool owned_joker_price_printed = false;

    // Anim start
    if (timer == 1)
    {
        // Erase shop text and Joker Description frame
        tte_erase_rect_wrapper(PLAYING_SCREEN_RECT);
        main_bg_se_copy_expand_3x3_rect(CARD_DESC_9_PTCH_TO_RECT, SHOP_CLEAR_3X3_SRC_POS);

        // Enable transparency window
        toggle_windows(false, true);

        // Redraw Jokers/Consumables frames
        main_bg_se_copy_expand_3x3_rect(OWNED_JOKERS_PANEL_RECT, OWNED_CARDS_PANEL_3X3_SRC_POS);
        main_bg_se_copy_expand_3x3_rect(
            OWNED_CONSUMABLES_PANEL_RECT,
            OWNED_CARDS_PANEL_3X3_SRC_POS
        );

        // Move Jokers back to their positions
        JokerObject* joker_object = NULL;

        // Owned Jokers
        ListItr itr = list_itr_create(get_jokers_list());
        while ((joker_object = list_itr_next(&itr)))
        {
            if (joker_object != description_card &&
                joker_object->sprite_object != NULL)
                joker_object->sprite_object->ty = int2fx(HELD_JOKERS_POS.y);
        }

        // Shop Jokers
        itr = list_itr_create(&s_shop_jokers_list);
        while ((joker_object = list_itr_next(&itr)))
        {
            if (joker_object != description_card &&
                joker_object->sprite_object != NULL)
                joker_object->sprite_object->ty = int2fx(ITEM_SHOP_Y);
        }

        for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
            if (s_held_alchemicals[i] != NULL && s_held_alchemicals[i] != description_alchemical)
            {
                s_held_alchemicals[i]->sprite_object->ty = int2fx(16);
                if (s_held_alchemicals[i]->sprite_object->sprite != NULL)
                    obj_unhide(
                        s_held_alchemicals[i]->sprite_object->sprite->obj,
                        ATTR0_AFF
                    );
            }
        for (int i = 0; i < game_shop_alchemical_offer_count(); i++)
            if (s_shop_alchemicals[i] != NULL &&
                s_shop_alchemicals[i] != description_alchemical)
            {
                s_shop_alchemicals[i]->sprite_object->ty = int2fx(ALCHEMICAL_SHOP_Y);
                if (s_shop_alchemicals[i]->sprite_object->sprite != NULL)
                    obj_unhide(
                        s_shop_alchemicals[i]->sprite_object->sprite->obj,
                        ATTR0_AFF
                    );
            }
        if (s_shop_planet != NULL && s_shop_planet != description_planet)
        {
            s_shop_planet->sprite_object->ty = int2fx(ALCHEMICAL_SHOP_Y);
            if (s_shop_planet->sprite_object->sprite != NULL)
                obj_unhide(s_shop_planet->sprite_object->sprite->obj, ATTR0_AFF);
        }
        if (s_shop_voucher != NULL && s_shop_voucher != description_voucher)
        {
            s_shop_voucher->sprite_object->ty = int2fx(VOUCHER_SHOP_Y);
            if (s_shop_voucher->sprite_object->sprite != NULL)
                obj_unhide(s_shop_voucher->sprite_object->sprite->obj, ATTR0_AFF);
        }

        description_sprite->tx = description_card_original_x_pos;
        description_sprite->ty = description_card_original_y_pos;
        if (description_voucher != NULL)
            voucher_object_set_description_scale(description_voucher, false);
        if (description_planet != NULL)
            planet_object_set_description_scale(description_planet, false);
        if (description_alchemical != NULL)
            alchemical_object_set_description_scale(description_alchemical, false);
    }

    // First 12 anim frames
    if (timer <= TM_SHOW_CARD_DESC_WAIT)
    {
        // Show Deck (last 5 frames only)
        if (TM_SHOW_CARD_DESC_WAIT - timer < 5)
            main_bg_se_move_rect_1_tile_vert(DECK_ANIM_RECT, SCREEN_UP);
        // Show shop panel
        main_bg_se_move_rect_1_tile_vert(POP_MENU_ANIM_RECT, SCREEN_UP);
    }

    // Last anim frame (no need to wait for the Joker to have stopped for this):
    else if (timer == TM_SHOW_CARD_DESC_WAIT + 1)
    {
        // Prices belong to fixed shop slots, not to focused/moving sprites.
        JokerObject* joker_object = NULL;
        ListItr itr = list_itr_create(&s_shop_jokers_list);
        while ((joker_object = list_itr_next(&itr)))
        {
            if (joker_object->joker != NULL && joker_object->sprite_object != NULL)
                game_shop_print_joker_price(joker_object);
        }
        for (int i = 0; i < game_shop_alchemical_offer_count(); i++)
        {
            if (s_shop_alchemicals[i] != NULL)
            {
                const AlchemicalInfo* info =
                    alchemical_get_info(s_shop_alchemicals[i]->id);
                if (info == NULL)
                    continue;
                game_shop_print_alchemical_price(
                    s_shop_alchemicals[i],
                    game_shop_discounted_price(info->cost)
                );
            }
        }
        if (s_shop_planet != NULL)
            game_shop_print_planet_price(s_shop_planet);
        if (s_shop_voucher != NULL && !description_is_purchase)
            game_shop_print_voucher_price(s_shop_voucher);

        // Print Reroll prince
        tte_printf(
            "#{P:%d,%d; cx:0x%X000}$%d",
            SHOP_REROLL_RECT.left,
            SHOP_REROLL_RECT.top,
            TTE_WHITE_PB,
            game_shop_current_reroll_cost()
        );

        // Print Deck size that was erased
        display_deck_size_max();
    }

    // Cleanup and change state
    else if (description_sprite->vx == 0 && description_sprite->vy == 0)
    {
        bool completed_purchase = description_is_purchase;
        if (!completed_purchase && description_alchemical != NULL &&
            description_alchemical->slot < ALCHEMICAL_HELD_LIMIT)
        {
            sprite_object_print_price_under(
                description_alchemical->sprite_object,
                alchemical_get_sell_value(description_alchemical->id)
            );
        }
        owned_joker_price_printed = false;
        description_card = NULL;
        description_alchemical = NULL;
        description_planet = NULL;
        description_voucher = NULL;
        description_is_purchase = false;

        if (completed_purchase)
        {
            voucher_object_destroy(&s_shop_voucher);
            game_shop_fill_joker_offers();
            game_shop_redraw_prices(true);
            display_deck_size_max();
            if (game_shop_special_offer_count() > 0)
                game_shop_set_selection((Selection){0, 2});
            else
                game_shop_set_selection(
                    (Selection){shop_top_row_get_size() - 1, 1}
                );
        }

        timer = TM_ZERO;
        state_machine_change_state(&shop_sm, GAME_SHOP_ACTIVE);
    }

    // At any point after the other prices have been printed, and while the card is still moving,
    // if we are NOT pressing A, print the price under it.
    else if (!owned_joker_price_printed && !key_held(SELECT_CARD) &&
             description_card != NULL && description_card_original_list == get_jokers_list())
    {
        owned_joker_price_printed = true;
        sprite_object_print_price_under(
            description_card->sprite_object,
            joker_get_sell_value(description_card->joker)
        );
    }
}

/**
 * @brief Outro sequence substate update.
 *         This makes the menu and shop icon go out of frame.
 */
static void game_shop_outro(void)
{
    // Shift the shop panel
    main_bg_se_move_rect_1_tile_vert(POP_MENU_ANIM_RECT, SCREEN_DOWN);

    main_bg_se_copy_rect_1_tile_vert(TOP_LEFT_PANEL_ANIM_RECT, SCREEN_UP);

    // TODO: make heads or tails of what's going on here and replace
    // magic numbers.
    if (timer == 1)
    {
        tte_erase_rect_wrapper(SHOP_PRICES_TEXT_RECT); // Erase the shop prices text

        ListItr itr = list_itr_create(&s_shop_jokers_list);
        JokerObject* joker_object;
        while ((joker_object = list_itr_next(&itr)))
        {
            if (joker_object != NULL && joker_object->sprite_object != NULL)
            {
                joker_object->sprite_object->ty = int2fx(160);
            }
        }
        for (int i = 0; i < game_shop_alchemical_offer_count(); i++)
            if (s_shop_alchemicals[i] != NULL)
            {
                s_shop_alchemicals[i]->sprite_object->ty = int2fx(160);
                s_shop_alchemicals[i]->sprite_object->y =
                    s_shop_alchemicals[i]->sprite_object->ty;
                obj_hide(s_shop_alchemicals[i]->sprite_object->sprite->obj);
            }
        if (s_shop_planet != NULL)
        {
            s_shop_planet->sprite_object->ty = int2fx(160);
            s_shop_planet->sprite_object->y = s_shop_planet->sprite_object->ty;
            obj_hide(s_shop_planet->sprite_object->sprite->obj);
        }

        /*
         * Held Alchemicals use separate shop-only objects in the upper-right
         * inventory row.  They were destroyed only after the outro completed,
         * so they remained visible while every item for sale was leaving.
         * Slide them through the nearest edge during the same outro; the
         * gameplay state recreates its own objects after the blind is chosen.
         */
        for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
        {
            if (s_held_alchemicals[i] == NULL)
                continue;
            sprite_object_erase_text_under(s_held_alchemicals[i]->sprite_object);
            s_held_alchemicals[i]->sprite_object->ty =
                int2fx(-ALCHEMICAL_OBJECT_HEIGHT);
            s_held_alchemicals[i]->sprite_object->y =
                s_held_alchemicals[i]->sprite_object->ty;
            obj_hide(s_held_alchemicals[i]->sprite_object->sprite->obj);
        }

        if (s_shop_voucher != NULL)
            s_shop_voucher->sprite_object->ty = int2fx(160);

        reset_top_left_panel_bottom_row();
    }
    else if (timer == 2)
    {
        int y = 5;
        memset16(&se_mat[MAIN_BG_SBB][y - 1][0], 0x0001, 1);
        memset16(&se_mat[MAIN_BG_SBB][y - 1][1], 0x0002, 7);
        memset16(&se_mat[MAIN_BG_SBB][y - 1][8], SE_HFLIP | 0x0001, 1);
    }

    if (timer >= MENU_POP_OUT_ANIM_FRAMES)
    {
        game_change_state(GAME_STATE_BLIND_SELECT);
    }
}

/**
 * @brief Cycling shop lights animation substate update.
 */
static inline void game_shop_lights_anim_frame(void)
{
    // Shift palette around the border of the shop icon
    COLOR shifted_palette[4];
    shifted_palette[0] = pal_bg_mem[SHOP_LIGHTS_2_PAL_IDX];
    shifted_palette[1] = pal_bg_mem[SHOP_LIGHTS_3_PAL_IDX];
    shifted_palette[2] = pal_bg_mem[SHOP_LIGHTS_4_PAL_IDX];
    shifted_palette[3] = pal_bg_mem[SHOP_LIGHTS_1_PAL_IDX];

    // Circularly shift the palette
    int last = shifted_palette[3];

    for (int i = 3; i > 0; --i)
    {
        shifted_palette[i] = shifted_palette[i - 1];
    }

    shifted_palette[0] = last;

    // Copy the shifted palette to the next 4 slots
    pal_bg_mem[SHOP_LIGHTS_2_PAL_IDX] = shifted_palette[0];
    pal_bg_mem[SHOP_LIGHTS_3_PAL_IDX] = shifted_palette[1];
    pal_bg_mem[SHOP_LIGHTS_4_PAL_IDX] = shifted_palette[2];
    pal_bg_mem[SHOP_LIGHTS_1_PAL_IDX] = shifted_palette[3];
}

void game_shop_on_update(void)
{
    timer++;

    if (s_shop_feedback_visible &&
        ++s_shop_feedback_timer >= FRAMES(20))
    {
        game_shop_clear_alchemical_feedback();
    }

    if (timer % 20 == 0)
    {
        game_shop_lights_anim_frame();
    }
}

void game_shop_on_exit(void)
{
    List* shop_jokers_list = &s_shop_jokers_list;
    ListItr itr = list_itr_create(shop_jokers_list);
    JokerObject* joker_object;

    while ((joker_object = list_itr_next(&itr)))
    {
        if (joker_object != NULL && joker_object->joker != NULL)
        {
            // Make the joker available back to shop
            game_shop_set_joker_avail(joker_object->joker->id, true);
        }
        joker_object_destroy(&joker_object); // Destroy the joker objects
    }

    list_clear(shop_jokers_list);
    game_shop_destroy_alchemical_offers();
    voucher_object_destroy(&s_shop_voucher);
    for (int i = 0; i < ALCHEMICAL_HELD_LIMIT; i++)
        game_shop_destroy_alchemical(&s_held_alchemicals[i], false);

    increment_blind(BLIND_STATE_DEFEATED); // TODO: Move to game_round_end()?

    state_machine_remove(&shop_sm);

    save_game();
}

void game_shop_debug_refresh(void)
{
    game_shop_sync_held_alchemicals();
    game_shop_fill_joker_offers();
    game_shop_redraw_prices(true);
    display_deck_size_max();
}
