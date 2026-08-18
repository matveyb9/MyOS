#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include <framebuffer.h>
#include <syscall.h>

#define CELL_WIDTH 8U
#define CELL_HEIGHT 8U
#define MAX_COLUMNS 160U
#define MAX_ROWS 100U
#define ANSI_ESC 0x1BU
#define GUI_WINDOW_COUNT 3U
#define GUI_WINDOW_SYSTEM 0U
#define GUI_WINDOW_NOTES 1U
#define GUI_WINDOW_MONITOR 2U
#define GUI_CONTENT_TITLE_MAX 16U
#define GUI_CONTENT_MAX 128U

typedef struct gui_window {
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
    const char *title;
    const char *line_one;
    const char *line_two;
    uint8_t visible;
} gui_window_t;

typedef struct framebuffer_console {
    volatile uint32_t *pixels;
    uint64_t width;
    uint64_t height;
    uint64_t pixels_per_row;
    uint64_t columns;
    uint64_t rows;
    uint64_t cursor_column;
    uint64_t cursor_row;
    uint64_t scroll_count;
    uint32_t background;
    uint32_t foreground;
    uint32_t accent;
    uint8_t ansi_state;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    int gui_active;
    uint8_t gui_focus;
    uint8_t gui_z_order[GUI_WINDOW_COUNT];
    gui_window_t gui_windows[GUI_WINDOW_COUNT];
    char gui_content_title[GUI_CONTENT_TITLE_MAX];
    uint8_t gui_content[GUI_CONTENT_MAX];
    uint64_t gui_content_length;
    uint64_t gui_content_flags;
    uint64_t gui_content_cursor;
    uint64_t gui_content_viewport;
    uint64_t gui_pointer_x;
    uint64_t gui_pointer_y;
    uint32_t gui_pointer_under[11U * 11U];
    int gui_pointer_under_valid;
    int active;
} framebuffer_console_t;

static framebuffer_console_t console;
static char cells[MAX_COLUMNS * MAX_ROWS];

static const uint8_t glyphs[][7] = {
    {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U}, /* A */
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU}, /* B */
    {0x0EU, 0x11U, 0x10U, 0x10U, 0x10U, 0x11U, 0x0EU}, /* C */
    {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU}, /* D */
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU}, /* E */
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U}, /* F */
    {0x0EU, 0x11U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0EU}, /* G */
    {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U}, /* H */
    {0x0EU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU}, /* I */
    {0x01U, 0x01U, 0x01U, 0x01U, 0x11U, 0x11U, 0x0EU}, /* J */
    {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U}, /* K */
    {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU}, /* L */
    {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U}, /* M */
    {0x11U, 0x19U, 0x15U, 0x13U, 0x11U, 0x11U, 0x11U}, /* N */
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU}, /* O */
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U}, /* P */
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x15U, 0x12U, 0x0DU}, /* Q */
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U}, /* R */
    {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU}, /* S */
    {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U}, /* T */
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU}, /* U */
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0AU, 0x04U}, /* V */
    {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU}, /* W */
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x0AU, 0x11U, 0x11U}, /* X */
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U}, /* Y */
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x1FU}, /* Z */
    {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU}, /* 0 */
    {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU}, /* 1 */
    {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU}, /* 2 */
    {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU}, /* 3 */
    {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U}, /* 4 */
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU}, /* 5 */
    {0x0EU, 0x10U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU}, /* 6 */
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U}, /* 7 */
    {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU}, /* 8 */
    {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x01U, 0x0EU}  /* 9 */
};

static uint32_t pack_component(uint8_t value, uint8_t mask_size, uint8_t mask_shift) {
    uint64_t scaled;

    if (mask_size == 0U || mask_size > 32U) {
        return 0U;
    }
    scaled = ((uint64_t)value * ((UINT64_C(1) << mask_size) - UINT64_C(1))) / UINT64_C(255);
    return (uint32_t)(scaled << mask_shift);
}

static uint32_t rgb(const struct limine_framebuffer *framebuffer,
                    uint8_t red, uint8_t green, uint8_t blue) {
    return pack_component(red, framebuffer->red_mask_size, framebuffer->red_mask_shift)
        | pack_component(green, framebuffer->green_mask_size, framebuffer->green_mask_shift)
        | pack_component(blue, framebuffer->blue_mask_size, framebuffer->blue_mask_shift);
}

static int glyph_index(char character) {
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - ('a' - 'A'));
    }
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= '0' && character <= '9') {
        return 26 + character - '0';
    }
    return -1;
}

static uint8_t punctuation_row(char character, uint8_t row) {
    switch (character) {
        case '.': return row == 6U ? 0x04U : 0U;
        case ',': return row == 6U ? 0x08U : 0U;
        case ':': return (row == 2U || row == 5U) ? 0x04U : 0U;
        case ';': return row == 2U ? 0x04U : (row == 5U ? 0x08U : 0U);
        case '!': return (row < 5U ? 0x04U : (row == 6U ? 0x04U : 0U));
        case '?': return row == 0U ? 0x0EU : (row == 1U ? 0x01U : (row == 2U ? 0x02U : (row == 3U ? 0x04U : (row == 6U ? 0x04U : 0U))));
        case '-': return row == 3U ? 0x0EU : 0U;
        case '_': return row == 6U ? 0x1FU : 0U;
        case '+': return row == 3U ? 0x1FU : (row == 2U || row == 4U ? 0x04U : 0U);
        case '=': return row == 2U || row == 4U ? 0x1FU : 0U;
        case '/': return (uint8_t)(1U << (4U - (row * 5U / 7U)));
        case '\\': return (uint8_t)(1U << (row * 5U / 7U));
        case '[': return row == 0U || row == 6U ? 0x0EU : 0x08U;
        case ']': return row == 0U || row == 6U ? 0x0EU : 0x02U;
        case '(': return row == 0U || row == 6U ? 0x06U : (row < 3U ? 0x08U : 0x02U);
        case ')': return row == 0U || row == 6U ? 0x0CU : (row < 3U ? 0x02U : 0x08U);
        case '<': return row == 2U ? 0x03U : (row == 3U ? 0x0CU : (row == 4U ? 0x03U : 0U));
        case '>': return row == 2U ? 0x18U : (row == 3U ? 0x06U : (row == 4U ? 0x18U : 0U));
        case '#': return row == 1U || row == 4U ? 0x0AU : (row == 2U || row == 3U ? 0x1FU : 0U);
        case '*': return row == 2U ? 0x15U : (row == 3U ? 0x0EU : (row == 4U ? 0x15U : 0U));
        case '"': return row == 0U || row == 1U ? 0x0AU : 0U;
        case '\'': return row == 0U || row == 1U ? 0x04U : 0U;
        default: return 0U;
    }
}

static uint8_t glyph_row(char character, uint8_t row) {
    const int index = glyph_index(character);
    if (row >= 7U) {
        return 0U;
    }
    if (index >= 0) {
        return glyphs[(uint8_t)index][row];
    }
    return punctuation_row(character, row);
}

static void put_pixel(uint64_t x, uint64_t y, uint32_t colour) {
    if (x < console.width && y < console.height) {
        console.pixels[y * console.pixels_per_row + x] = colour;
    }
}

static uint32_t console_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return pack_component(red, console.red_mask_size, console.red_mask_shift)
        | pack_component(green, console.green_mask_size, console.green_mask_shift)
        | pack_component(blue, console.blue_mask_size, console.blue_mask_shift);
}

static void fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t colour) {
    const uint64_t end_x = x + width > console.width ? console.width : x + width;
    const uint64_t end_y = y + height > console.height ? console.height : y + height;

    for (uint64_t row = y; row < end_y; row++) {
        for (uint64_t column = x; column < end_x; column++) {
            put_pixel(column, row, colour);
        }
    }
}

static void draw_gui_character(uint64_t x, uint64_t y, char character, uint32_t colour) {
    for (uint64_t row = 0U; row < 7U; row++) {
        const uint8_t bits = glyph_row(character, (uint8_t)row);
        for (uint64_t column = 0U; column < 5U; column++) {
            if ((bits & (uint8_t)(UINT8_C(1) << (4U - column))) != 0U) {
                put_pixel(x + column, y + row, colour);
            }
        }
    }
}

static void draw_gui_text(uint64_t x, uint64_t y, const char *text, uint32_t colour) {
    while (*text != '\0') {
        draw_gui_character(x, y, *text, colour);
        x += 7U;
        text++;
    }
}

static void gui_reset_content(void) {
    static const char default_title[] = "VIEWER";
    static const char default_content[] = "PRESS M FOR MOTD OR D FOR DISK NOTE";
    uint64_t index;

    for (index = 0U; index < GUI_CONTENT_TITLE_MAX; index++) {
        console.gui_content_title[index] = '\0';
    }
    for (index = 0U; default_title[index] != '\0' && index + 1U < GUI_CONTENT_TITLE_MAX; index++) {
        console.gui_content_title[index] = default_title[index];
    }
    for (index = 0U; index < GUI_CONTENT_MAX; index++) {
        console.gui_content[index] = 0U;
    }
    for (index = 0U; default_content[index] != '\0' && index < GUI_CONTENT_MAX; index++) {
        console.gui_content[index] = (uint8_t)default_content[index];
    }
    console.gui_content_length = index;
    console.gui_content_flags = 0U;
    console.gui_content_cursor = 0U;
    console.gui_content_viewport = 0U;
}

static uint64_t gui_content_line_start(uint64_t position) {
    uint64_t start = position > console.gui_content_length ? console.gui_content_length : position;

    while (start != 0U && console.gui_content[start - 1U] != (uint8_t)'\n') {
        start--;
    }
    return start;
}

static void draw_gui_content(const gui_window_t *window, uint32_t colour) {
    uint64_t x = window->x + 16U;
    uint64_t y = window->y + 48U;
    const uint64_t first_x = x;
    const uint64_t limit_x = window->x + window->width - 16U;
    const uint64_t limit_y = window->y + window->height - 12U;
    uint64_t index = console.gui_content_viewport;

    if (index > console.gui_content_length) {
        index = console.gui_content_length;
    }
    index = gui_content_line_start(index);
    while (y < limit_y) {
        const int caret_here = (console.gui_content_flags & MYOS_GUI_CONTENT_FLAG_EDITABLE) != 0U
            && index == console.gui_content_cursor;

        if (caret_here != 0) {
            fill_rect(x, y, 2U, 8U, console_rgb(36U, 170U, 224U));
        }
        if (index == console.gui_content_length) {
            break;
        }
        if (console.gui_content[index] == (uint8_t)'\n') {
            index++;
            x = first_x;
            y += 12U;
            continue;
        }
        if (x + 7U > limit_x) {
            x = first_x;
            y += 12U;
            if (y >= limit_y) {
                break;
            }
            continue;
        }
        {
            char character = (char)console.gui_content[index];

            if ((uint8_t)character < 32U || (uint8_t)character > 126U) {
                character = '?';
            }
            draw_gui_character(x, y, character, colour);
        }
        index++;
        x += 7U;
    }
}

static void gui_layout_windows(void) {
    gui_window_t *system_window = &console.gui_windows[GUI_WINDOW_SYSTEM];
    gui_window_t *notes_window = &console.gui_windows[GUI_WINDOW_NOTES];
    gui_window_t *monitor_window = &console.gui_windows[GUI_WINDOW_MONITOR];

    system_window->x = 40U;
    system_window->y = 64U;
    system_window->width = (console.width * 3U) / 5U;
    system_window->height = (console.height * 3U) / 5U;
    system_window->title = "SYSTEM";
    system_window->line_one = "MYOS GUI BRINGUP";
    system_window->line_two = "WINDOW MANAGER READY";
    system_window->visible = 1U;

    notes_window->x = console.width / 4U;
    notes_window->y = console.height / 4U;
    notes_window->width = (console.width * 3U) / 5U;
    notes_window->height = console.height / 2U;
    notes_window->title = console.gui_content_title;
    notes_window->line_one = "";
    notes_window->line_two = "";
    notes_window->visible = 1U;

    monitor_window->x = console.width / 2U;
    monitor_window->y = 96U;
    monitor_window->width = console.width / 3U;
    monitor_window->height = console.height / 3U;
    monitor_window->title = "MONITOR";
    monitor_window->line_one = "TASKS AND INPUT";
    monitor_window->line_two = "BOUNDED EVENT LOOP";
    monitor_window->visible = 1U;

    console.gui_z_order[0] = GUI_WINDOW_NOTES;
    console.gui_z_order[1] = GUI_WINDOW_MONITOR;
    console.gui_z_order[2] = GUI_WINDOW_SYSTEM;
    console.gui_focus = GUI_WINDOW_SYSTEM;
}

static uint64_t gui_visible_window_count(void) {
    uint64_t count = 0U;

    for (uint64_t index = 0U; index < GUI_WINDOW_COUNT; index++) {
        if (console.gui_windows[index].visible != 0U) {
            count++;
        }
    }
    return count;
}

static void gui_raise_window(uint8_t window_id) {
    uint64_t slot;

    if (window_id >= GUI_WINDOW_COUNT || console.gui_windows[window_id].visible == 0U) {
        return;
    }
    for (slot = 0U; slot < GUI_WINDOW_COUNT; slot++) {
        if (console.gui_z_order[slot] == window_id) {
            break;
        }
    }
    if (slot == GUI_WINDOW_COUNT) {
        return;
    }
    while (slot + 1U < GUI_WINDOW_COUNT) {
        console.gui_z_order[slot] = console.gui_z_order[slot + 1U];
        slot++;
    }
    console.gui_z_order[GUI_WINDOW_COUNT - 1U] = window_id;
    console.gui_focus = window_id;
}

static void gui_focus_next_window(void) {
    for (uint64_t offset = 1U; offset <= GUI_WINDOW_COUNT; offset++) {
        const uint8_t candidate = (uint8_t)((console.gui_focus + offset) % GUI_WINDOW_COUNT);

        if (console.gui_windows[candidate].visible != 0U) {
            gui_raise_window(candidate);
            return;
        }
    }
}

static int gui_window_contains(const gui_window_t *window, uint64_t x, uint64_t y) {
    return window->visible != 0U && x >= window->x && y >= window->y
        && x - window->x < window->width && y - window->y < window->height;
}

static void gui_focus_pointer_window(void) {
    for (uint64_t slot = GUI_WINDOW_COUNT; slot > 0U; slot--) {
        const uint8_t candidate = console.gui_z_order[slot - 1U];

        if (gui_window_contains(&console.gui_windows[candidate], console.gui_pointer_x,
                                console.gui_pointer_y) != 0) {
            gui_raise_window(candidate);
            return;
        }
    }
    gui_focus_next_window();
}

static void gui_toggle_window(uint8_t window_id) {
    if (window_id >= GUI_WINDOW_COUNT) {
        return;
    }
    if (console.gui_windows[window_id].visible == 0U) {
        console.gui_windows[window_id].visible = 1U;
        gui_raise_window(window_id);
        return;
    }
    if (gui_visible_window_count() <= 1U) {
        return;
    }
    console.gui_windows[window_id].visible = 0U;
    if (console.gui_focus == window_id) {
        gui_focus_next_window();
    }
}

static void draw_gui_window(const gui_window_t *window, int focused) {
    const uint32_t border = focused != 0 ? console_rgb(36U, 170U, 224U) : console_rgb(92U, 112U, 138U);
    const uint32_t header = focused != 0 ? console_rgb(36U, 170U, 224U) : console_rgb(54U, 76U, 106U);
    const uint32_t surface = console_rgb(229U, 235U, 243U);
    const uint32_t text = console_rgb(13U, 20U, 34U);

    fill_rect(window->x, window->y, window->width, window->height, border);
    fill_rect(window->x + 2U, window->y + 2U, window->width - 4U, window->height - 4U, surface);
    fill_rect(window->x + 2U, window->y + 2U, window->width - 4U, 24U, header);
    draw_gui_text(window->x + 14U, window->y + 10U, window->title, surface);
    fill_rect(window->x + window->width - 22U, window->y + 7U, 10U, 10U, surface);
    if (window == &console.gui_windows[GUI_WINDOW_NOTES]) {
        draw_gui_content(window, text);
    } else {
        draw_gui_text(window->x + 16U, window->y + 48U, window->line_one, text);
        draw_gui_text(window->x + 16U, window->y + 72U, window->line_two, text);
    }
}

static void erase_gui_pointer(void) {
    if (console.gui_pointer_under_valid == 0) {
        return;
    }
    for (uint64_t row = 0U; row < 11U; row++) {
        for (uint64_t column = 0U; column < 11U; column++) {
            console.pixels[(console.gui_pointer_y + row) * console.pixels_per_row
                           + console.gui_pointer_x + column] = console.gui_pointer_under[row * 11U + column];
        }
    }
    console.gui_pointer_under_valid = 0;
}

static void draw_gui_pointer(void) {
    const uint32_t outline = console_rgb(13U, 20U, 34U);
    const uint32_t fill = console_rgb(255U, 255U, 255U);
    const uint32_t accent = console_rgb(36U, 170U, 224U);

    for (uint64_t row = 0U; row < 11U; row++) {
        for (uint64_t column = 0U; column < 11U; column++) {
            uint32_t colour = fill;

            console.gui_pointer_under[row * 11U + column] =
                console.pixels[(console.gui_pointer_y + row) * console.pixels_per_row
                               + console.gui_pointer_x + column];
            if (row == 0U || column == 0U || row == 10U || column == 10U) {
                colour = outline;
            } else if (row == 5U || column == 5U) {
                colour = accent;
            }
            put_pixel(console.gui_pointer_x + column, console.gui_pointer_y + row, colour);
        }
    }
    console.gui_pointer_under_valid = 1;
}

static void redraw_gui_desktop(void) {
    const uint32_t desktop = console_rgb(18U, 31U, 56U);
    const uint32_t top_bar = console_rgb(25U, 54U, 92U);
    const uint32_t text = console_rgb(229U, 235U, 243U);

    console.gui_pointer_under_valid = 0;
    fill_rect(0U, 0U, console.width, console.height, desktop);
    fill_rect(0U, 0U, console.width, 32U, top_bar);
    draw_gui_text(16U, 12U, "MYOS DESKTOP", text);
    for (uint64_t slot = 0U; slot < GUI_WINDOW_COUNT; slot++) {
        const uint8_t window_id = console.gui_z_order[slot];
        const gui_window_t *window = &console.gui_windows[window_id];

        if (window->visible != 0U) {
            draw_gui_window(window, window_id == console.gui_focus);
        }
    }
    fill_rect(20U, console.height - 44U, console.width - 40U, 24U, top_bar);
    draw_gui_text(30U, console.height - 36U, "MOUSE MOVE/CLICK WASD FALLBACK TAB FOCUS 1-3 TOGGLE F POINTER X HIDE N NEXT DISK E EDIT ARROWS NAV HOME END DEL CTRL-S SAVE ESC CANCEL Q EXIT", text);
    draw_gui_pointer();
}

static void draw_cell(uint64_t column, uint64_t row) {
    const char character = cells[row * console.columns + column];
    const uint64_t origin_x = column * CELL_WIDTH;
    const uint64_t origin_y = row * CELL_HEIGHT;

    for (uint64_t y = 0U; y < CELL_HEIGHT; y++) {
        const uint8_t bits = glyph_row(character, (uint8_t)y);
        for (uint64_t x = 0U; x < CELL_WIDTH; x++) {
            const int glyph_pixel = x >= 1U && x <= 5U && (bits & (uint8_t)(1U << (5U - x))) != 0U;
            put_pixel(origin_x + x, origin_y + y, glyph_pixel != 0 ? console.foreground : console.background);
        }
    }
}

static void draw_cursor(void) {
    const uint64_t x = console.cursor_column * CELL_WIDTH;
    const uint64_t y = console.cursor_row * CELL_HEIGHT + (CELL_HEIGHT - 1U);
    for (uint64_t offset = 1U; offset < CELL_WIDTH - 1U; offset++) {
        put_pixel(x + offset, y, console.accent);
    }
}

static void redraw_all(void) {
    for (uint64_t row = 0U; row < console.rows; row++) {
        for (uint64_t column = 0U; column < console.columns; column++) {
            draw_cell(column, row);
        }
    }
    draw_cursor();
}

static void scroll_one_line(void) {
    for (uint64_t row = 1U; row < console.rows; row++) {
        for (uint64_t column = 0U; column < console.columns; column++) {
            cells[(row - 1U) * console.columns + column] = cells[row * console.columns + column];
        }
    }
    for (uint64_t column = 0U; column < console.columns; column++) {
        cells[(console.rows - 1U) * console.columns + column] = ' ';
    }
    console.cursor_row = console.rows - 1U;
    console.scroll_count++;
    redraw_all();
}

static void advance_line(void) {
    console.cursor_column = 0U;
    console.cursor_row++;
    if (console.cursor_row >= console.rows) {
        scroll_one_line();
    }
}

int framebuffer_console_init(const struct limine_framebuffer *framebuffer) {
    if (framebuffer == (const struct limine_framebuffer *)0
        || framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->bpp != 32U
        || framebuffer->address == (void *)0 || framebuffer->pitch < framebuffer->width * 4U
        || framebuffer->pitch % 4U != 0U) {
        return 0;
    }

    console.pixels = (volatile uint32_t *)framebuffer->address;
    console.width = framebuffer->width;
    console.height = framebuffer->height;
    console.pixels_per_row = framebuffer->pitch / 4U;
    console.columns = framebuffer->width / CELL_WIDTH;
    console.rows = framebuffer->height / CELL_HEIGHT;
    if (console.columns == 0U || console.rows == 0U || console.columns > MAX_COLUMNS || console.rows > MAX_ROWS) {
        return 0;
    }
    console.cursor_column = 0U;
    console.cursor_row = 0U;
    console.scroll_count = 0U;
    console.ansi_state = 0U;
    console.red_mask_size = framebuffer->red_mask_size;
    console.red_mask_shift = framebuffer->red_mask_shift;
    console.green_mask_size = framebuffer->green_mask_size;
    console.green_mask_shift = framebuffer->green_mask_shift;
    console.blue_mask_size = framebuffer->blue_mask_size;
    console.blue_mask_shift = framebuffer->blue_mask_shift;
    console.gui_active = 0;
    console.gui_focus = 0U;
    for (uint64_t index = 0U; index < GUI_WINDOW_COUNT; index++) {
        console.gui_z_order[index] = (uint8_t)index;
        console.gui_windows[index].visible = 0U;
    }
    gui_reset_content();
    console.gui_pointer_x = 0U;
    console.gui_pointer_y = 0U;
    console.gui_pointer_under_valid = 0;
    console.background = rgb(framebuffer, 13U, 20U, 34U);
    console.foreground = rgb(framebuffer, 229U, 235U, 243U);
    console.accent = rgb(framebuffer, 36U, 170U, 224U);
    console.active = 1;
    framebuffer_console_clear();
    return 1;
}

int framebuffer_console_available(void) {
    return console.active;
}

void framebuffer_console_clear(void) {
    if (console.active == 0) {
        return;
    }
    for (uint64_t row = 0U; row < console.rows; row++) {
        for (uint64_t column = 0U; column < console.columns; column++) {
            cells[row * console.columns + column] = ' ';
        }
    }
    console.cursor_column = 0U;
    console.cursor_row = 0U;
    for (uint64_t y = 0U; y < console.height; y++) {
        for (uint64_t x = 0U; x < console.width; x++) {
            put_pixel(x, y, console.background);
        }
    }
    for (uint64_t x = 0U; x < console.width; x++) {
        put_pixel(x, 0U, console.accent);
    }
    draw_cursor();
}

void framebuffer_console_putc(char character) {
    if (console.active == 0 || console.gui_active != 0) {
        return;
    }
    if (console.ansi_state != 0U) {
        if (console.ansi_state == 1U && character == '[') {
            console.ansi_state = 2U;
        } else if (console.ansi_state == 2U && character == '2') {
            console.ansi_state = 3U;
        } else if (console.ansi_state == 3U && character == 'J') {
            console.ansi_state = 0U;
            framebuffer_console_clear();
        } else if (character >= '@' && character <= '~') {
            console.ansi_state = 0U;
        }
        return;
    }
    if ((uint8_t)character == ANSI_ESC) {
        console.ansi_state = 1U;
        return;
    }
    if (character == '\r') {
        console.cursor_column = 0U;
        draw_cursor();
        return;
    }
    if (character == '\n') {
        advance_line();
        draw_cursor();
        return;
    }
    if (character == '\b') {
        if (console.cursor_column > 0U) {
            console.cursor_column--;
            cells[console.cursor_row * console.columns + console.cursor_column] = ' ';
            draw_cell(console.cursor_column, console.cursor_row);
        }
        draw_cursor();
        return;
    }
    if ((uint8_t)character < 32U || (uint8_t)character > 126U) {
        return;
    }

    cells[console.cursor_row * console.columns + console.cursor_column] = character;
    draw_cell(console.cursor_column, console.cursor_row);
    console.cursor_column++;
    if (console.cursor_column >= console.columns) {
        advance_line();
    }
    draw_cursor();
}

int framebuffer_gui_begin(void) {
    if (console.active == 0 || console.gui_active != 0 || console.width < 640U || console.height < 480U) {
        return 0;
    }
    console.gui_active = 1;
    gui_reset_content();
    gui_layout_windows();
    console.gui_pointer_x = console.width / 2U;
    console.gui_pointer_y = console.height / 2U;
    console.gui_pointer_under_valid = 0;
    redraw_gui_desktop();
    return 1;
}

void framebuffer_gui_end(void) {
    if (console.gui_active == 0) {
        return;
    }
    console.gui_active = 0;
    console.gui_pointer_under_valid = 0;
    framebuffer_console_clear();
}

int framebuffer_gui_active(void) {
    return console.gui_active;
}

int framebuffer_gui_set_content(const char *title, const uint8_t *data, uint64_t length, uint64_t flags,
                                uint64_t cursor, uint64_t viewport) {
    uint64_t index;

    if (console.gui_active == 0 || title == (const char *)0 || data == (const uint8_t *)0
        || length > GUI_CONTENT_MAX || (flags & ~MYOS_GUI_CONTENT_FLAG_EDITABLE) != 0U || cursor > length
        || viewport > length || title[0] == '\0') {
        return 0;
    }
    for (index = 0U; index < GUI_CONTENT_TITLE_MAX; index++) {
        console.gui_content_title[index] = '\0';
    }
    for (index = 0U; index + 1U < GUI_CONTENT_TITLE_MAX && title[index] != '\0'; index++) {
        console.gui_content_title[index] = title[index];
    }
    for (index = 0U; index < GUI_CONTENT_MAX; index++) {
        console.gui_content[index] = 0U;
    }
    for (index = 0U; index < length; index++) {
        console.gui_content[index] = data[index];
    }
    console.gui_content_length = length;
    console.gui_content_flags = flags;
    console.gui_content_cursor = cursor;
    console.gui_content_viewport = gui_content_line_start(viewport);
    redraw_gui_desktop();
    return 1;
}

void framebuffer_gui_handle_input(char character) {
    const uint64_t step = 16U;
    int pointer_only = 0;

    if (console.gui_active == 0) {
        return;
    }
    if (character == 'w' || character == 'W') {
        erase_gui_pointer();
        console.gui_pointer_y = console.gui_pointer_y > step ? console.gui_pointer_y - step : 0U;
        pointer_only = 1;
    } else if (character == 's' || character == 'S') {
        const uint64_t limit = console.height - 11U;

        erase_gui_pointer();
        console.gui_pointer_y = console.gui_pointer_y + step < limit ? console.gui_pointer_y + step : limit;
        pointer_only = 1;
    } else if (character == 'a' || character == 'A') {
        erase_gui_pointer();
        console.gui_pointer_x = console.gui_pointer_x > step ? console.gui_pointer_x - step : 0U;
        pointer_only = 1;
    } else if (character == 'd' || character == 'D') {
        const uint64_t limit = console.width - 11U;

        erase_gui_pointer();
        console.gui_pointer_x = console.gui_pointer_x + step < limit ? console.gui_pointer_x + step : limit;
        pointer_only = 1;
    } else if (character == '\t' || character == '\n' || character == ' ') {
        gui_focus_next_window();
    } else if (character == 'f' || character == 'F') {
        gui_focus_pointer_window();
    } else if (character >= '1' && character <= '3') {
        gui_toggle_window((uint8_t)(character - '1'));
    } else if (character == 'x' || character == 'X') {
        gui_toggle_window(console.gui_focus);
    } else if (character == 'r' || character == 'R') {
        gui_layout_windows();
    } else {
        return;
    }
    if (pointer_only != 0) {
        draw_gui_pointer();
    } else {
        redraw_gui_desktop();
    }
}

static uint64_t gui_pointer_offset(uint64_t position, int64_t delta, uint64_t limit) {
    if (delta < 0) {
        const uint64_t distance = (uint64_t)(-(delta + 1)) + 1U;

        return position > distance ? position - distance : 0U;
    }
    if ((uint64_t)delta > limit - position) {
        return limit;
    }
    return position + (uint64_t)delta;
}

void framebuffer_gui_handle_mouse(int64_t delta_x, int64_t delta_y, int left_pressed, int left_was_pressed) {
    const uint8_t previous_focus = console.gui_focus;

    if (console.gui_active == 0) {
        return;
    }
    if (delta_x == 0 && delta_y == 0 && (left_pressed == 0 || left_was_pressed != 0)) {
        return;
    }
    erase_gui_pointer();
    console.gui_pointer_x = gui_pointer_offset(console.gui_pointer_x, delta_x, console.width - 11U);
    console.gui_pointer_y = gui_pointer_offset(console.gui_pointer_y, delta_y, console.height - 11U);
    if (left_pressed != 0 && left_was_pressed == 0) {
        gui_focus_pointer_window();
    }
    if (console.gui_focus != previous_focus) {
        redraw_gui_desktop();
    } else {
        draw_gui_pointer();
    }
}

uint64_t framebuffer_console_columns(void) {
    return console.columns;
}

uint64_t framebuffer_console_rows(void) {
    return console.rows;
}

uint64_t framebuffer_console_scroll_count(void) {
    return console.scroll_count;
}
