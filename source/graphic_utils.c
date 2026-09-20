#include "graphic_utils.h"

#include "layout.h"

#include <stdio.h>
#include <string.h>
#include <tonc_core.h>
#include <tonc_math.h>
#include <tonc_tte.h>

static const Rect FULL_SCREENBLOCK_RECT = {0, 0, SE_ROW_LEN - 1, SE_COL_LEN - 1};
static const Rect TOP_LEFT_PANEL_BOTTOM_ROW_RESET_RECT = {0, 28, 8, 28};

static void clip_se_rect_to_screenblock(Rect* rect);
static void bg_se_copy_or_move_rect_1_tile_vert(
    u16 bg_sbb,
    Rect se_rect,
    enum ScreenVertDir direction,
    bool move
);
static void main_bg_se_copy_or_move_rect_1_tile_vert(
    Rect se_rect,
    enum ScreenVertDir direction,
    bool move
);

// Clips a rect of screenblock entries to a specified rect
// The bounding rect is not required to be within screenblock boundaries
static inline void clip_se_rect_to_bounding_rect(Rect* rect, const Rect* bounding_rect)
{
    rect->right = min(rect->right, bounding_rect->right);
    rect->bottom = min(rect->bottom, bounding_rect->bottom);
    rect->left = max(rect->left, bounding_rect->left);
    rect->top = max(rect->top, bounding_rect->top);
}

// Can be unstaticed if needed
// Clips a rect of screenblock entries to screenblock boundaries
static void clip_se_rect_to_screenblock(Rect* rect)
{
    clip_se_rect_to_bounding_rect(rect, &FULL_SCREENBLOCK_RECT);
}

// Clips a rect of screenblock entries to be within one step of
// screenblock boundaries vertically depending on direction.
static inline void clip_se_rect_within_step_of_full_screen_vert(
    Rect* se_rect,
    enum ScreenVertDir direction
)
{
    Rect bounding_rect = FULL_SCREENBLOCK_RECT;
    if (direction == SCREEN_UP)
    {
        bounding_rect.top += 1;
    }
    else if (direction == SCREEN_DOWN)
    {
        bounding_rect.bottom -= 1;
    }

    clip_se_rect_to_bounding_rect(se_rect, &bounding_rect);
}

// Internal static function to merge implementation of move/copy functions.
static void bg_se_copy_or_move_rect_1_tile_vert(
    u16 bg_sbb,
    Rect se_rect,
    enum ScreenVertDir direction,
    bool move
)
{
    if (se_rect.left > se_rect.right || (direction != SCREEN_UP && direction != SCREEN_DOWN))
        return;

    // Clip to avoid read/write overflow of the screenblock
    clip_se_rect_within_step_of_full_screen_vert(&se_rect, direction);

    int start = (direction == SCREEN_UP) ? se_rect.top : se_rect.bottom;
    int end = (direction == SCREEN_UP) ? se_rect.bottom : se_rect.top;

    for (int y = start; y != end - direction; y -= direction)
    {
        memcpy16(
            &(se_mat[bg_sbb][y + direction][se_rect.left]),
            &se_mat[bg_sbb][y][se_rect.left],
            rect_width(&se_rect)
        );
    }

    if (move)
    {
        memset16(&se_mat[bg_sbb][end][se_rect.left], 0x0000, rect_width(&se_rect));
    }
}

static void main_bg_se_copy_or_move_rect_1_tile_vert(
    Rect se_rect,
    enum ScreenVertDir direction,
    bool move
)
{
    bg_se_copy_or_move_rect_1_tile_vert(MAIN_BG_SBB, se_rect, direction, move);
}

void bg_se_copy_rect_1_tile_vert(u16 bg_sbb, Rect se_rect, enum ScreenVertDir direction)
{
    bg_se_copy_or_move_rect_1_tile_vert(bg_sbb, se_rect, direction, false);
}

void bg_se_move_rect_1_tile_vert(u16 bg_sbb, Rect se_rect, enum ScreenVertDir direction)
{
    bg_se_copy_or_move_rect_1_tile_vert(bg_sbb, se_rect, direction, true);
}

void main_bg_se_copy_rect_1_tile_vert(Rect se_rect, enum ScreenVertDir direction)
{
    main_bg_se_copy_or_move_rect_1_tile_vert(se_rect, direction, false);
}

void main_bg_se_move_rect_1_tile_vert(Rect se_rect, enum ScreenVertDir direction)
{
    main_bg_se_copy_or_move_rect_1_tile_vert(se_rect, direction, true);
}

void main_bg_se_copy_rect(Rect se_rect, BG_POINT dest_pos)
{
    if (se_rect.left > se_rect.right || se_rect.top > se_rect.bottom)
        return;

    // Clip the source first, then crop it again to the visible destination.
    clip_se_rect_to_screenblock(&se_rect);

    int src_x_offset = max(0, -dest_pos.x);
    int src_y_offset = max(0, -dest_pos.y);
    int dest_x = max(0, dest_pos.x);
    int dest_y = max(0, dest_pos.y);
    int width = min(
        rect_width(&se_rect) - src_x_offset,
        SE_ROW_LEN - dest_x
    );
    int height = min(
        rect_height(&se_rect) - src_y_offset,
        SE_COL_LEN - dest_y
    );
    if (width <= 0 || height <= 0)
        return;

    /*
     * Do not put a width*height VLA on the GBA stack.  A full screenblock
     * copy used to reserve 2048 bytes here, while this ROM has only a few KiB
     * of IWRAM left for the complete call stack.  That made otherwise harmless
     * menu/description transitions capable of corrupting return addresses.
     *
     * A single fixed row is enough.  Select the vertical copy direction like
     * memmove so an overlapping destination cannot overwrite a source row
     * that has not been read yet.  memcpy16 is retained because byte writes
     * are not safe for GBA VRAM.
     */
    SE row_buffer[SE_ROW_LEN];
    int src_y = se_rect.top + src_y_offset;
    int first_row = 0;
    int last_row = height;
    int step = 1;
    if (dest_y > src_y && dest_y < src_y + height)
    {
        first_row = height - 1;
        last_row = -1;
        step = -1;
    }

    for (int sy = first_row; sy != last_row; sy += step)
    {
        memcpy16(
            row_buffer,
            &se_mat[MAIN_BG_SBB][src_y + sy][se_rect.left + src_x_offset],
            width
        );
        memcpy16(
            &se_mat[MAIN_BG_SBB][dest_y + sy][dest_x],
            row_buffer,
            width
        );
    }
}

static inline void main_bg_se_fill_rect_with_se(SE se, Rect se_rect)
{
    if (se_rect.left > se_rect.right || se_rect.top > se_rect.bottom)
        return;

    // Clip to avoid screenblock overflow
    clip_se_rect_to_screenblock(&se_rect);

    int width = rect_width(&se_rect);
    int height = rect_height(&se_rect);

    for (int sy = 0; sy < height; sy++)
    {
        memset16(&se_mat[MAIN_BG_SBB][se_rect.top + sy][se_rect.left], se, width);
    }
}

void main_bg_se_copy_expand_tile(Rect se_rect_dest, BG_POINT se_tile_src)
{
    main_bg_se_fill_rect_with_se(se_mat[MAIN_BG_SBB][se_tile_src.y][se_tile_src.x], se_rect_dest);
}

void main_bg_se_copy_expand_3x3_rect(Rect se_dest_rect, BG_POINT src_top_left_pnt)
{
    // New implementation: uses a 9-patch to factorize as much code as we can
    // clang-format off
    NinePatchRect src_9_ptch = {
        .patch_rect =
            {
                src_top_left_pnt.x,
                src_top_left_pnt.y,
                src_top_left_pnt.x + 2,
                src_top_left_pnt.y + 2
            },
        .margins = {1, 1, 1, 1}
    };
    // clang-format on
    main_bg_se_copy_expand_9_patch(se_dest_rect, &src_9_ptch);
}

// Helper: Copy the left and right sides of a 3x3 tile block
static inline void main_bg_se_3w_copy_expand_left_right_sides(
    const Rect* se_dest_rect,
    const BG_POINT* src_left_pnt
)
{
    SE middle_left_se = se_mat[MAIN_BG_SBB][src_left_pnt->y][src_left_pnt->x];
    // Assuming width 3 so the right side is + 2
    SE middle_right_se = se_mat[MAIN_BG_SBB][src_left_pnt->y][src_left_pnt->x + 2];
    int dest_rect_width = rect_width(se_dest_rect);
    int dest_rect_height = rect_height(se_dest_rect);
    for (int y = 0; y < dest_rect_height; y++)
    {
        se_mat[MAIN_BG_SBB][se_dest_rect->top + y][se_dest_rect->left] = middle_left_se;
        se_mat[MAIN_BG_SBB][se_dest_rect->top + y][se_dest_rect->left + dest_rect_width - 1] =
            middle_right_se;
    }
}

void main_bg_se_copy_expand_3w_row(Rect se_dest_rect, BG_POINT src_row_left_pnt)
{
    clip_se_rect_to_screenblock(&se_dest_rect);
    int dest_rect_width = rect_width(&se_dest_rect);
    if (dest_rect_width < 2)
        return;

    // Copy left and right sides
    main_bg_se_3w_copy_expand_left_right_sides(&se_dest_rect, &src_row_left_pnt);

    if (dest_rect_width > 2)
    {
        SE middle_fill_se = se_mat[MAIN_BG_SBB][src_row_left_pnt.y][src_row_left_pnt.x + 1];
        Rect dest_inner_fill_rect = se_dest_rect;

        // Avoid copying the sides when filling the rect.
        dest_inner_fill_rect.left += 1;
        dest_inner_fill_rect.right -= 1;
        main_bg_se_fill_rect_with_se(middle_fill_se, dest_inner_fill_rect);
    }
}

// Helper: Copy the four corners of a 9-patch
static inline void main_bg_se_expand_9_patch_copy_corners(
    Rect se_dest_rect,
    const NinePatchRect* src_9_ptch
)
{
    BG_POINT top_left_dest_pos = {se_dest_rect.left, se_dest_rect.top};
    Rect top_left_src = {
        src_9_ptch->patch_rect.left,
        src_9_ptch->patch_rect.top,
        src_9_ptch->patch_rect.left + src_9_ptch->margins.left - 1,
        src_9_ptch->patch_rect.top + src_9_ptch->margins.top - 1
    };

    BG_POINT top_right_dest_pos = {
        se_dest_rect.right - src_9_ptch->margins.right + 1,
        se_dest_rect.top
    };
    Rect top_right_src = {
        src_9_ptch->patch_rect.right - src_9_ptch->margins.right + 1,
        src_9_ptch->patch_rect.top,
        src_9_ptch->patch_rect.right,
        src_9_ptch->patch_rect.top + src_9_ptch->margins.top - 1
    };

    BG_POINT bottom_right_dest_pos = {
        se_dest_rect.right - src_9_ptch->margins.right + 1,
        se_dest_rect.bottom - src_9_ptch->margins.bottom + 1
    };
    Rect bottom_right_src = {
        src_9_ptch->patch_rect.right - src_9_ptch->margins.right + 1,
        src_9_ptch->patch_rect.bottom - src_9_ptch->margins.bottom + 1,
        src_9_ptch->patch_rect.right,
        src_9_ptch->patch_rect.bottom
    };

    BG_POINT bottom_left_dest_pos = {
        se_dest_rect.left,
        se_dest_rect.bottom - src_9_ptch->margins.bottom + 1
    };
    Rect bottom_left_src = {
        src_9_ptch->patch_rect.left,
        src_9_ptch->patch_rect.bottom - src_9_ptch->margins.bottom + 1,
        src_9_ptch->patch_rect.left + src_9_ptch->margins.left - 1,
        src_9_ptch->patch_rect.bottom
    };

    main_bg_se_copy_rect(top_left_src, top_left_dest_pos);
    main_bg_se_copy_rect(top_right_src, top_right_dest_pos);
    main_bg_se_copy_rect(bottom_right_src, bottom_right_dest_pos);
    main_bg_se_copy_rect(bottom_left_src, bottom_left_dest_pos);
}

// Helper: Copy the four sides of a 9-patch
static inline void main_bg_se_expand_9_patch_stretch_sides(
    Rect se_dest_rect,
    const NinePatchRect* src_9_ptch,
    int top_bottom_width,
    int left_right_height
)
{
    // We can take the Top and Bottom sides' tiles 1 by 1 and stretch them horizontally

    // Top side
    for (int i = 0; i < src_9_ptch->margins.top; i++)
    {
        SE top_side_tile = se_mat[MAIN_BG_SBB][src_9_ptch->patch_rect.top + i]
                                 [src_9_ptch->patch_rect.left + src_9_ptch->margins.left];
        memset16(
            &se_mat[MAIN_BG_SBB][se_dest_rect.top + i]
                   [se_dest_rect.left + src_9_ptch->margins.left],
            top_side_tile,
            top_bottom_width
        );
    }

    // Bottom side side
    for (int i = 0; i < src_9_ptch->margins.bottom; i++)
    {
        SE bottom_side_tile = se_mat[MAIN_BG_SBB][src_9_ptch->patch_rect.bottom - i]
                                    [src_9_ptch->patch_rect.left + src_9_ptch->margins.left];
        memset16(
            &se_mat[MAIN_BG_SBB][se_dest_rect.bottom - i]
                   [se_dest_rect.left + src_9_ptch->margins.left],
            bottom_side_tile,
            top_bottom_width
        );
    }

    // Can't stretch the Left and Right sides with a wide memset16, and
    // main_bg_se_fill_rect_with_se` does it rows-wise too. We'll have to copy rects by hand.

    Rect left_side_tiles_src = {
        src_9_ptch->patch_rect.left,
        src_9_ptch->patch_rect.top + src_9_ptch->margins.top,
        src_9_ptch->patch_rect.left + src_9_ptch->margins.left - 1,
        src_9_ptch->patch_rect.top + src_9_ptch->margins.top
    };
    BG_POINT left_side_tiles_dest_pos = {
        se_dest_rect.left,
        se_dest_rect.top + src_9_ptch->margins.top
    };

    Rect right_side_tiles_src = {
        src_9_ptch->patch_rect.right - src_9_ptch->margins.right + 1,
        src_9_ptch->patch_rect.top + src_9_ptch->margins.top,
        src_9_ptch->patch_rect.right,
        src_9_ptch->patch_rect.top + src_9_ptch->margins.top
    };
    BG_POINT right_side_tiles_dest_pos = {
        se_dest_rect.right - src_9_ptch->margins.right + 1,
        se_dest_rect.top + src_9_ptch->margins.top
    };

    for (int i = 0; i < left_right_height; i++)
    {
        main_bg_se_copy_rect(left_side_tiles_src, left_side_tiles_dest_pos);
        left_side_tiles_dest_pos.y++;

        main_bg_se_copy_rect(right_side_tiles_src, right_side_tiles_dest_pos);
        right_side_tiles_dest_pos.y++;
    }
}

void main_bg_se_copy_expand_9_patch(Rect se_dest_rect, const NinePatchRect* src_9_ptch)
{
    clip_se_rect_to_screenblock(&se_dest_rect);

    int dest_rect_width = rect_width(&se_dest_rect);
    int dest_rect_height = rect_height(&se_dest_rect);

    int src_9ptch_min_width = src_9_ptch->margins.left + src_9_ptch->margins.right;
    int src_9ptch_min_height = src_9_ptch->margins.top + src_9_ptch->margins.bottom;

    // Verify the dest rect is big enough to fit at least the 9-patch's corners
    if (dest_rect_width < src_9ptch_min_width || dest_rect_height < src_9ptch_min_height)
        return;

    // Copy the corners
    main_bg_se_expand_9_patch_copy_corners(se_dest_rect, src_9_ptch);

    // Fill the sides and center if needed
    if (dest_rect_width > src_9ptch_min_width && dest_rect_height > src_9ptch_min_height)
    {
        // Stretch sides
        main_bg_se_expand_9_patch_stretch_sides(
            se_dest_rect,
            src_9_ptch,
            dest_rect_width - src_9ptch_min_width,
            dest_rect_height - src_9ptch_min_height
        );

        // Fill center
        SE middle_fill_se =
            se_mat[MAIN_BG_SBB][src_9_ptch->patch_rect.top + src_9_ptch->margins.top]
                  [src_9_ptch->patch_rect.left + src_9_ptch->margins.left];
        Rect dest_inner_fill_rect = {
            se_dest_rect.left + src_9_ptch->margins.left,
            se_dest_rect.top + src_9_ptch->margins.top,
            se_dest_rect.right - src_9_ptch->margins.right,
            se_dest_rect.bottom - src_9_ptch->margins.bottom
        };
        main_bg_se_fill_rect_with_se(middle_fill_se, dest_inner_fill_rect);
    }
}

void tte_erase_rect_wrapper(Rect rect)
{
    tte_erase_rect(rect.left, rect.top, rect.right, rect.bottom);
}

void update_text_rect_to_right_align_str(
    Rect* rect,
    const char* str,
    enum OverflowDir overflow_direction
)
{
    // TODO: Allow passing string length to avoid calling strlen()?
    int str_len = strlen(str);
    if (overflow_direction == OVERFLOW_LEFT)
    {
        rect->left = max(0, rect->right - str_len * TTE_CHAR_SIZE);
    }
    else if (overflow_direction == OVERFLOW_RIGHT)
    {
        int num_fitting_chars = rect_width(rect) / TTE_CHAR_SIZE;
        if (str_len < num_fitting_chars)
            rect->left += (num_fitting_chars - str_len) * TTE_CHAR_SIZE;
        // else nothing is to be updated, entire rect is filled and may overflow
    }
}

void update_text_rect_to_center_str(Rect* rect, const char* str, enum ScreenHorzDir bias_direction)
{
    if (rect == NULL || str == NULL)
        return;

    int text_width_chars = strlen(str);
    int rect_width_chars = rect_width(rect) / TTE_CHAR_SIZE;

    bool bias_right = (bias_direction == SCREEN_RIGHT);

    /* Adding bias_right makes sure that we round up when biased right
     * but round down when biased left.
     */
    rect->left += max(0, (rect_width_chars - text_width_chars + bias_right) / 2) * TTE_CHAR_SIZE;
}

void memcpy16_tile8_with_palette_offset(u16* dst, const u16* src, uint hwcount, u8 palette_offset)
{
    const u16 offset = (((palette_offset) << 8) | (palette_offset));
    for (int i = 0; i < hwcount; i++)
    {
        // Copying u8 data twice across u16 data
        dst[i] = src[i] + offset;
    }
}

void memcpy32_tile8_with_palette_offset(u32* dst, const u32* src, uint wcount, u8 palette_offset)
{
    const u32 offset =
        (palette_offset << 24) | (palette_offset << 16) | (palette_offset << 8) | palette_offset;
    for (int i = 0; i < wcount; i++)
    {
        // Copying u8 data 4 times across u32 data
        dst[i] = src[i] + offset;
    }
}

void toggle_windows(bool win0, bool win1)
{
    if (win0)
    {
        REG_DISPCNT |= DCNT_WIN0;
    }
    else
    {
        REG_DISPCNT &= ~DCNT_WIN0;
    }

    if (win1)
    {
        REG_DISPCNT |= DCNT_WIN1;
    }
    else
    {
        REG_DISPCNT &= ~DCNT_WIN1;
    }

    if (win0 || win1)
    {
        REG_BLDCNT = BLD_BUILD(BLD_BG1, BLD_BG2, 1);
    }
    else
    {
        REG_BLDCNT = 0;
    }
}

void main_bg_se_clear_rect(Rect se_rect)
{
    if (se_rect.left > se_rect.right)
        return;
    // Clip to avoid screenblock overflow
    clip_se_rect_to_screenblock(&se_rect);

    for (int y = se_rect.top; y < se_rect.bottom; y++)
    {
        memset16(&(se_mat[MAIN_BG_SBB][y][se_rect.left]), 0x0000, rect_width(&se_rect));
    }
}

void reset_top_left_panel_bottom_row(void)
{
    BG_POINT top_left_panel_bottom_row_pos = TOP_LEFT_PANEL_POINT;
    // Use the source rect height to offset to the bottom row point
    top_left_panel_bottom_row_pos.y += rect_height(&TOP_LEFT_ITEM_SRC_RECT) - 1;
    main_bg_se_copy_rect(TOP_LEFT_PANEL_BOTTOM_ROW_RESET_RECT, top_left_panel_bottom_row_pos);
}

// Width of the screen, in nb of tiles
#define MAX_LINE_TEXT_LENGTH 30

int tte_printf_justified_in_rect(
    const char* raw_text,
    Rect dst_rect,
    enum TextJustifyFlag justify_direction,
    enum ScreenHorzDir bias_direction,
    bool do_print
)
{
    if (raw_text == NULL)
        return 0;

    // These are the actual lengths of the line/token, which take the {TAGS} into account

    int raw_text_len = strlen(raw_text);

    int line_start = 0;
    int token_start = 0;
    int token_len = 0;

    // These lengths correspond to what will be visible on screen

    int max_line_text_len = rect_width(&dst_rect);
    int max_line_count = rect_height(&dst_rect);
    if (max_line_text_len <= 0 || max_line_count <= 0)
        return 0;

    int line_text_len = 0;
    int token_text_len = 0;

    int line_x = 0;
    int line_y = 0;

    /*
     * A valid pass advances by at least one source byte. Keep an additional
     * hard limit so malformed formatting can never lock the game loop.
     */
    int iterations_remaining = raw_text_len + 1;
    while (line_start < raw_text_len && iterations_remaining-- > 0)
    {
        int previous_line_start = line_start;

        // Need to do everything by hand, as it seems tte_printf does NOT stop at \0
        token_len = 0;
        token_text_len = 0;

        // Parse the `raw_text` and
        while (line_text_len <= max_line_text_len && token_start < raw_text_len)
        {
            int current_char = token_start + token_len;

            if (current_char >= raw_text_len)
            {
                token_start = raw_text_len;
                token_len = 0;
                token_text_len = -1;
                break;
            }

            // Handle tags
            if (raw_text[current_char] == '#' && (current_char + 1) < raw_text_len &&
                raw_text[current_char + 1] == '{')
            {
                while (current_char < raw_text_len && raw_text[current_char] != '}')
                {
                    token_len++;
                    current_char++;
                }

                if (current_char < raw_text_len)
                {
                    token_len++;
                    current_char++;
                }
            }

            // Handle special cases

            // End of word/text detected
            if (raw_text[current_char] == ' ' || raw_text[current_char] == '\0')
            {
                token_start = current_char + 1;
                // Set these to -1 so that it becomes 0 when incremented for the
                // next iteration since we need to continue.
                token_len = -1;
                token_text_len = -1;
            }
            // Early end of line detected, interrupt parsing immediately
            else if (raw_text[current_char] == '\n')
            {
                token_start = current_char + 1;
                // Set to 0, no need to compensate for continuing the loops since
                // we're breaking early from it.
                token_len = 0;
                token_text_len = 0;
                line_text_len++;
                break;
            }

            token_len++;
            token_text_len++;
            line_text_len++;
        };

        /*
         * A single visible token wider than the destination previously left
         * token_start unchanged. The outer loop would then process the same
         * byte forever and lock the game while opening a description.
         * Split such a token at the visible line width while copying any TTE
         * formatting tag as an indivisible zero-width sequence.
         */
        if (token_start <= line_start)
        {
            int split = line_start;
            int visible = 0;
            while (split < raw_text_len && visible < max_line_text_len)
            {
                if (raw_text[split] == '#' && split + 1 < raw_text_len &&
                    raw_text[split + 1] == '{')
                {
                    do
                    {
                        split++;
                    } while (split < raw_text_len && raw_text[split - 1] != '}');
                    continue;
                }
                if (raw_text[split] == '\n')
                    break;
                split++;
                visible++;
            }
            if (split <= line_start)
                split = line_start + 1;
            token_start = split;
            line_text_len = visible;
            /* The normal overflow subtraction below must keep this segment. */
            token_text_len = -1;
        }

        // Do not print anything if we only need to compute the paragraph's total height
        if (do_print && line_y < max_line_count)
        {
            // Length of the raw slice for this line (includes formatting tags)
            int line_len = token_start - line_start;

            // Trim trailing whitespace/newlines from the raw slice
            while (line_len > 0)
            {
                char c = raw_text[line_start + line_len - 1];
                if (c == ' ' || c == '\n' || c == '\0')
                    line_len--;
                else
                    break;
            }

            // Remove the overflowing token's visible length from this line
            line_text_len = line_text_len - token_text_len - 1;
            if (line_text_len < 0)
                line_text_len = 0;

            // useful DEBUG
            // tte_printf("#{P:0,%d}line %d - %d", 40 + (dst_rect.top + line_y) * TILE_SIZE, line_y,
            // line_text_len);

            // Now, we can print the chars from line_start to token_start
            line_x = 0;
            if (justify_direction == JUSTIFY_CENTER)
            {
                line_x = (max_line_text_len - line_text_len) / 2;
            }

            tte_printf(
                "#{P:%d,%d}%.*s",
                (dst_rect.left + line_x) * TILE_SIZE,
                (dst_rect.top + line_y) * TILE_SIZE,
                line_len,
                raw_text + line_start
            );
        }

        line_start = token_start;
        if (line_start <= previous_line_start)
            line_start = previous_line_start + 1;
        if (line_start > raw_text_len)
            line_start = raw_text_len;
        line_text_len = 0;
        line_y++;
    }

    return line_y;
}

u8 obj_palette_brightest_color_index(int palette_bank)
{
    if (palette_bank < 0 || palette_bank >= NUM_PALETTES)
        return 1;

    u8 brightest = 1;
    int brightest_luma = -1;
    for (int i = 1; i < PAL_ROW_LEN; i++)
    {
        COLOR color = pal_obj_mem[palette_bank * PAL_ROW_LEN + i];
        int luma = (color & 31) + ((color >> 5) & 31) + ((color >> 10) & 31);
        if (luma > brightest_luma)
        {
            brightest_luma = luma;
            brightest = i;
        }
    }
    return brightest;
}

u8 obj_palette_darkest_color_index(int palette_bank)
{
    if (palette_bank < 0 || palette_bank >= NUM_PALETTES)
        return 1;

    u8 darkest = 1;
    int darkest_luma = 32 * 3;
    for (int i = 1; i < PAL_ROW_LEN; i++)
    {
        COLOR color = pal_obj_mem[palette_bank * PAL_ROW_LEN + i];
        int luma = (color & 31) + ((color >> 5) & 31) + ((color >> 10) & 31);
        if (luma < darkest_luma)
        {
            darkest_luma = luma;
            darkest = i;
        }
    }
    return darkest;
}

static inline u8 obj_tiles_get_pixel_4bpp(
    const u8* tiles,
    int tile_columns,
    int x,
    int y
)
{
    int tile = (y / TILE_SIZE) * tile_columns + x / TILE_SIZE;
    int offset = tile * 32 + (y % TILE_SIZE) * 4 + (x % TILE_SIZE) / 2;
    return (x & 1) ? tiles[offset] >> 4 : tiles[offset] & 0x0F;
}

static inline void obj_tiles_set_pixel_4bpp(
    u8* tiles,
    int tile_columns,
    int x,
    int y,
    u8 color
)
{
    int tile = (y / TILE_SIZE) * tile_columns + x / TILE_SIZE;
    int offset = tile * 32 + (y % TILE_SIZE) * 4 + (x % TILE_SIZE) / 2;
    if (x & 1)
        tiles[offset] = (tiles[offset] & 0x0F) | (color << 4);
    else
        tiles[offset] = (tiles[offset] & 0xF0) | color;
}

void obj_tiles_add_outline_4bpp(u8* tiles, int width, int height, u8 color_index)
{
    if (tiles == NULL || width <= 0 || height <= 0 || width > 32 || height > 32 ||
        width % TILE_SIZE != 0 || height % TILE_SIZE != 0 || color_index == 0 ||
        color_index >= PAL_ROW_LEN)
    {
        return;
    }

    /* One bit per destination pixel: 32 rows cost only 128 bytes temporarily. */
    u32 outline_rows[32] = {0};
    int tile_columns = width / TILE_SIZE;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (obj_tiles_get_pixel_4bpp(tiles, tile_columns, x, y) == 0)
                continue;

            for (int dy = -1; dy <= 1; dy++)
            {
                int outline_y = y + dy;
                if (outline_y < 0 || outline_y >= height)
                    continue;
                for (int dx = -1; dx <= 1; dx++)
                {
                    int outline_x = x + dx;
                    if ((dx == 0 && dy == 0) || outline_x < 0 || outline_x >= width)
                        continue;
                    if (obj_tiles_get_pixel_4bpp(
                            tiles,
                            tile_columns,
                            outline_x,
                            outline_y
                        ) == 0)
                    {
                        outline_rows[outline_y] |= 1U << outline_x;
                    }
                }
            }
        }
    }

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            if (outline_rows[y] & (1U << x))
                obj_tiles_set_pixel_4bpp(tiles, tile_columns, x, y, color_index);
}

void obj_tiles_add_card_cursor_frame_4bpp(
    u8* tiles,
    u8 outer_color_index,
    u8 separator_color_index
)
{
    if (tiles == NULL || outer_color_index == 0 ||
        outer_color_index >= PAL_ROW_LEN || separator_color_index == 0 ||
        separator_color_index >= PAL_ROW_LEN)
        return;

    const int card_sprite_size = 32;
    const int tile_columns = card_sprite_size / TILE_SIZE;

    /* Rounded two-step top/bottom edge, matching the existing card shape. */
    for (int x = 5; x <= 26; x++)
    {
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, x, 0, outer_color_index);
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, x, 31, outer_color_index);
    }
    for (int x = 4; x <= 27; x++)
    {
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, x, 1, outer_color_index);
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, x, 30, outer_color_index);
    }

    /* One outside pixel plus the existing edge makes the cursor unmistakable. */
    for (int y = 2; y <= 29; y++)
    {
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, 3, y, outer_color_index);
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, 4, y, outer_color_index);
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, 27, y, outer_color_index);
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, 28, y, outer_color_index);
    }

    /* Playing cards already have a pale edge.  This inset dark keyline keeps
     * the white cursor frame distinct instead of letting both edges merge. */
    for (int x = 5; x <= 26; x++)
    {
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, x, 2, separator_color_index);
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, x, 29, separator_color_index);
    }
    for (int y = 3; y <= 28; y++)
    {
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, 5, y, separator_color_index);
        obj_tiles_set_pixel_4bpp(tiles, tile_columns, 26, y, separator_color_index);
    }
}
