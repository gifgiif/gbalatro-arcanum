#include "button.h"

#include "audio_utils.h"
#include "soundbank.h"

#include <tonc.h>

void button_set_highlight(Button* button, bool highlight)
{
    if (button == NULL)
        return;

    u16 set_color = highlight ? BTN_HIGHLIGHT_COLOR : pal_bg_mem[button->button_pal_idx];

    /* Selection code may defensively reassert a highlight every frame.  OBJ
     * and BG palette memory is shared hardware state, so avoid an identical
     * write when the requested border is already displayed. */
    if (pal_bg_mem[button->border_pal_idx] == set_color)
        return;

    memset16(&pal_bg_mem[button->border_pal_idx], set_color, 1);
}

void button_press(Button* button)
{
    if (button == NULL || button->on_pressed == NULL ||
        (button->can_be_pressed != NULL && !button->can_be_pressed()))
    {
        return;
    }

    play_ui_sfx(SFX_BUTTON, MM_BASE_PITCH_RATE, BUTTON_SFX_VOLUME);

    button->on_pressed();
}
