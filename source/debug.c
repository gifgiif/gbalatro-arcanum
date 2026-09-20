#include "debug.h"

#include "alchemy.h"
#include "card.h"
#include "game.h"
#include "game/shop.h"
#include "game_variables.h"
#include "graphic_utils.h"
#include "voucher.h"

#include <stdio.h>
#include <tonc.h>

#if GBALATRO_DEBUG_MENU

enum DebugPage
{
    DEBUG_PAGE_ALCHEMY,
    DEBUG_PAGE_VOUCHER,
    DEBUG_PAGE_ENHANCEMENT,
    DEBUG_PAGE_CARD_EDITION,
    DEBUG_PAGE_SEAL,
    DEBUG_PAGE_COUNT
};

static const Rect DEBUG_RECT = {72, 40, 239, 103};
static bool s_open = false;
static enum DebugPage s_page = DEBUG_PAGE_ALCHEMY;
static int s_selected[DEBUG_PAGE_COUNT] = {0};
static char s_status[24] = "";

static bool debug_available_in_state(void)
{
    enum GameState state = game_get_state();
    return state == GAME_STATE_PLAYING || state == GAME_STATE_SHOP ||
           state == GAME_STATE_BLIND_SELECT;
}

static void debug_draw(void)
{
    tte_erase_rect_wrapper(DEBUG_RECT);

    static const char* page_names[DEBUG_PAGE_COUNT] = {
        "ALCHEMY", "VOUCHER", "ENHANCE", "EDITION", "SEAL"
    };
    const char* page_name = page_names[s_page];
    int selected = s_selected[s_page];
    int count = s_page == DEBUG_PAGE_ALCHEMY
                  ? ALCHEMICAL_ID_COUNT
                  : s_page == DEBUG_PAGE_VOUCHER
                      ? VOUCHER_COUNT
                      : s_page == DEBUG_PAGE_ENHANCEMENT
                          ? CARD_ENHANCEMENT_COUNT
                          : s_page == DEBUG_PAGE_CARD_EDITION
                              ? CARD_EDITION_COUNT
                              : CARD_SEAL_COUNT;
    const char* item_name = "";
    if (s_page == DEBUG_PAGE_ALCHEMY)
    {
        const AlchemicalInfo* info = alchemical_get_info(selected);
        item_name = info != NULL ? info->name : "?";
    }
    else if (s_page == DEBUG_PAGE_VOUCHER)
    {
        const VoucherInfo* info = voucher_get_info((enum VoucherId)selected);
        item_name = info != NULL ? info->name : "?";
    }
    else if (s_page == DEBUG_PAGE_ENHANCEMENT)
        item_name = card_get_enhancement_name(selected);
    else if (s_page == DEBUG_PAGE_CARD_EDITION)
        item_name = card_get_edition_name(selected);
    else
        item_name = card_get_seal_name(selected);

    tte_printf(
        "#{P:76,42; cx:0x%X000}DEBUG  L/R PAGE\n"
        "#{P:76,58; cx:0x%X000}%s %02d/%02d\n"
        "#{P:76,74; cx:0x%X000}%-20.20s\n"
        "#{P:76,90; cx:0x%X000}%s",
        TTE_YELLOW_PB,
        TTE_WHITE_PB,
        page_name,
        selected + 1,
        count,
        TTE_WHITE_PB,
        item_name,
        TTE_YELLOW_PB,
        s_status[0] != '\0' ? s_status : "A ADD  B CLOSE"
    );
}

static void debug_close(void)
{
    s_open = false;
    s_status[0] = '\0';
    tte_erase_rect_wrapper(DEBUG_RECT);
}

static void debug_grant_selected(void)
{
    bool granted;
    if (s_page == DEBUG_PAGE_ALCHEMY)
    {
        granted = game_debug_add_alchemical(
            (enum AlchemicalId)s_selected[DEBUG_PAGE_ALCHEMY]
        );
    }
    else if (s_page == DEBUG_PAGE_VOUCHER)
    {
        granted = voucher_grant(
            &g_game_vars.vouchers,
            (enum VoucherId)s_selected[DEBUG_PAGE_VOUCHER]
        );
        if (granted && game_get_state() == GAME_STATE_SHOP)
            game_shop_debug_refresh();
    }
    else
    {
        granted = game_debug_apply_card_modifier(
            s_page - DEBUG_PAGE_ENHANCEMENT,
            s_selected[s_page]
        );
    }
    snprintf(s_status, sizeof(s_status), "%s", granted ? "ADDED" : "FULL / OWNED");
}

bool debug_update(void)
{
    if (!s_open)
    {
        if (!debug_available_in_state() || !key_is_down(KEY_SELECT))
            return false;

        if (key_hit(KEY_B))
        {
            s_open = true;
            s_status[0] = '\0';
            debug_draw();
            return true;
        }
        if (key_hit(KEY_A))
        {
            g_game_vars.money = min(9999, g_game_vars.money + 100);
            display_money();
            return true;
        }
        if (key_hit(KEY_DOWN))
        {
            game_debug_win_blind();
            return true;
        }
        if (key_hit(KEY_L))
        {
            g_game_vars.hands = min(99, g_game_vars.hands + 1);
            g_game_vars.discards = min(99, g_game_vars.discards + 1);
            display_hands();
            display_discards();
            return true;
        }
        return false;
    }

    if (key_hit(KEY_B) || (key_is_down(KEY_SELECT) && key_hit(KEY_B)))
    {
        debug_close();
        return true;
    }
    if (key_hit(KEY_L))
    {
        s_page = (s_page + DEBUG_PAGE_COUNT - 1) % DEBUG_PAGE_COUNT;
        s_status[0] = '\0';
    }
    else if (key_hit(KEY_R))
    {
        s_page = (s_page + 1) % DEBUG_PAGE_COUNT;
        s_status[0] = '\0';
    }
    else if (key_hit(KEY_UP) || key_hit(KEY_DOWN))
    {
        int count = s_page == DEBUG_PAGE_ALCHEMY
                      ? ALCHEMICAL_ID_COUNT
                      : s_page == DEBUG_PAGE_VOUCHER
                          ? VOUCHER_COUNT
                          : s_page == DEBUG_PAGE_ENHANCEMENT
                              ? CARD_ENHANCEMENT_COUNT
                              : s_page == DEBUG_PAGE_CARD_EDITION
                                  ? CARD_EDITION_COUNT
                                  : CARD_SEAL_COUNT;
        int delta = key_hit(KEY_UP) ? -1 : 1;
        s_selected[s_page] = (s_selected[s_page] + count + delta) % count;
        s_status[0] = '\0';
    }
    else if (key_hit(KEY_A))
    {
        debug_grant_selected();
    }
    debug_draw();
    return true;
}

void debug_reset(void)
{
    s_open = false;
    s_page = DEBUG_PAGE_ALCHEMY;
    for (int i = 0; i < DEBUG_PAGE_COUNT; i++)
        s_selected[i] = 0;
    s_status[0] = '\0';
}

#else

bool debug_update(void)
{
    return false;
}

void debug_reset(void)
{
}

#endif
