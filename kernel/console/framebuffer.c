#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include <framebuffer.h>
#include <rtc.h>
#include <scheduler.h>
#include <syscall.h>
#include <vfs.h>

#define CELL_WIDTH 8U
#define CELL_HEIGHT 8U
#define MAX_COLUMNS 160U
#define MAX_ROWS 100U
#define ANSI_ESC 0x1BU
#define GUI_WINDOW_COUNT 3U
#define GUI_WINDOW_SYSTEM 0U
#define GUI_WINDOW_NOTES 1U
#define GUI_WINDOW_MONITOR 2U
#define GUI_CONTENT_TITLE_MAX MYOS_GUI_CONTENT_TITLE_MAX
#define GUI_CONTENT_MAX MYOS_GUI_CONTENT_MAX
#define GUI_POINTER_SIZE 11U
#define GUI_LAUNCHER_TILE_COUNT 4U
#define GUI_LAUNCHER_TILE_WIDTH 136U
#define GUI_LAUNCHER_TILE_HEIGHT 80U
#define GUI_LAUNCHER_TILE_GAP 16U
#define GUI_LAUNCHER_APP_SCAN_LIMIT 64U
#define GUI_LAUNCHER_APP_NAME_MAX 16U
#define GUI_WINDOW_TITLE_HEIGHT 24U
#define GUI_WINDOW_CLOSE_LEFT_INSET 22U
#define GUI_WINDOW_CLOSE_TOP_INSET 7U
#define GUI_WINDOW_CLOSE_WIDTH 14U
#define GUI_WINDOW_CLOSE_HEIGHT 12U

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
    int gui_launcher_active;
    uint8_t gui_launcher_app_count;
    char gui_launcher_app_names[MYOS_GUI_LAUNCHER_APP_MAX][GUI_LAUNCHER_APP_NAME_MAX];
    uint64_t gui_pointer_x;
    uint64_t gui_pointer_y;
    uint32_t gui_pointer_under[GUI_POINTER_SIZE * GUI_POINTER_SIZE];
    int gui_pointer_under_valid;
    struct rtc_time gui_clock;
    uint64_t gui_task_count;
    uint64_t gui_runnable_count;
    int gui_clock_valid;
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

static void draw_gui_compact_title(uint64_t x, uint64_t y, const char *text, uint32_t colour) {
    while (*text != '\0') {
        draw_gui_character(x, y, *text, colour);
        x += 6U;
        text++;
    }
}

static uint64_t gui_launcher_total_width(void) {
    return GUI_LAUNCHER_TILE_COUNT * GUI_LAUNCHER_TILE_WIDTH
        + (GUI_LAUNCHER_TILE_COUNT - 1U) * GUI_LAUNCHER_TILE_GAP;
}

static uint64_t gui_launcher_tile_x(uint64_t index) {
    return (console.width - gui_launcher_total_width()) / 2U
        + index * (GUI_LAUNCHER_TILE_WIDTH + GUI_LAUNCHER_TILE_GAP);
}

static uint64_t gui_launcher_app_total_width(void) {
    if (console.gui_launcher_app_count == 0U) {
        return 0U;
    }
    return (uint64_t)console.gui_launcher_app_count * GUI_LAUNCHER_TILE_WIDTH
        + ((uint64_t)console.gui_launcher_app_count - 1U) * GUI_LAUNCHER_TILE_GAP;
}

static uint64_t gui_launcher_tile_y(void) {
    return (console.height - GUI_LAUNCHER_TILE_HEIGHT) / 2U;
}

static uint64_t gui_launcher_app_tile_x(uint64_t index) {
    return (console.width - gui_launcher_app_total_width()) / 2U
        + index * (GUI_LAUNCHER_TILE_WIDTH + GUI_LAUNCHER_TILE_GAP);
}

static uint64_t gui_launcher_app_tile_y(void) {
    return gui_launcher_tile_y() + GUI_LAUNCHER_TILE_HEIGHT + GUI_LAUNCHER_TILE_GAP;
}

static int gui_point_in_rect(uint64_t x, uint64_t y, uint64_t left, uint64_t top,
                             uint64_t width, uint64_t height) {
    return x >= left && y >= top && x - left < width && y - top < height;
}

static int gui_pointer_hits_exit(void) {
    return gui_point_in_rect(console.gui_pointer_x, console.gui_pointer_y,
                             console.width - 40U, 7U, 20U, 18U);
}

static int gui_launcher_copy_app_name(char *destination, const char *source) {
    uint64_t index = 0U;

    while (index + 1U < GUI_LAUNCHER_APP_NAME_MAX && source[index] != '\0') {
        if (source[index] == '/' || source[index] < ' ' || source[index] > '~') {
            return 0;
        }
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
    return index != 0U && source[index] == '\0';
}

static int gui_launcher_has_main_elf(const char *name) {
    static const char prefix[] = "/apps/";
    static const char suffix[] = "/main.elf";
    char path[VFS_PATH_MAX] = { 0 };
    struct vfs_file file;
    uint64_t offset = 0U;

    for (uint64_t index = 0U; index + 1U < sizeof(prefix); index++) { path[offset++] = prefix[index]; }
    for (uint64_t index = 0U; name[index] != '\0' && offset + sizeof(suffix) < sizeof(path); index++) {
        path[offset++] = name[index];
    }
    for (uint64_t index = 0U; index + 1U < sizeof(suffix) && offset + 1U < sizeof(path); index++) {
        path[offset++] = suffix[index];
    }
    path[offset] = '\0';
    return vfs_open(path, &file) != 0 && file.type == VFS_OBJECT_REGULAR && file.size != 0U;
}

static void gui_launcher_refresh_apps(void) {
    console.gui_launcher_app_count = 0U;
    for (uint64_t index = 0U; index < MYOS_GUI_LAUNCHER_APP_MAX; index++) {
        console.gui_launcher_app_names[index][0] = '\0';
    }
    for (uint64_t index = 0U; index < GUI_LAUNCHER_APP_SCAN_LIMIT
         && console.gui_launcher_app_count < MYOS_GUI_LAUNCHER_APP_MAX; index++) {
        struct vfs_directory_entry entry;
        char name[GUI_LAUNCHER_APP_NAME_MAX] = { 0 };

        if (vfs_list("/apps", index, &entry) == 0) {
            break;
        }
        if (entry.type != VFS_OBJECT_DIRECTORY || gui_launcher_copy_app_name(name, entry.name) == 0
            || gui_launcher_has_main_elf(name) == 0) {
            continue;
        }
        for (uint64_t character = 0U; character < GUI_LAUNCHER_APP_NAME_MAX; character++) {
            console.gui_launcher_app_names[console.gui_launcher_app_count][character] = name[character];
            if (name[character] == '\0') {
                break;
            }
        }
        console.gui_launcher_app_count++;
    }
}

static char gui_launcher_action_at_pointer(void) {
    static const char actions[GUI_LAUNCHER_TILE_COUNT] = {
        (char)MYOS_INPUT_GUI_ACTION_SYSTEM,
        (char)MYOS_INPUT_GUI_ACTION_NOTES,
        (char)MYOS_INPUT_GUI_ACTION_EDITOR,
        (char)MYOS_INPUT_GUI_ACTION_FILES
    };
    const uint64_t top = gui_launcher_tile_y();

    if (console.gui_launcher_active == 0) {
        return '\0';
    }
    for (uint64_t index = 0U; index < GUI_LAUNCHER_TILE_COUNT; index++) {
        if (gui_point_in_rect(console.gui_pointer_x, console.gui_pointer_y, gui_launcher_tile_x(index), top,
                              GUI_LAUNCHER_TILE_WIDTH, GUI_LAUNCHER_TILE_HEIGHT) != 0) {
            return actions[index];
        }
    }
    for (uint64_t index = 0U; index < console.gui_launcher_app_count; index++) {
        if (gui_point_in_rect(console.gui_pointer_x, console.gui_pointer_y, gui_launcher_app_tile_x(index),
                              gui_launcher_app_tile_y(), GUI_LAUNCHER_TILE_WIDTH, GUI_LAUNCHER_TILE_HEIGHT) != 0) {
            return (char)(MYOS_INPUT_GUI_ACTION_APP_BASE + index);
        }
    }
    return '\0';
}

static void gui_reset_content(void) {
    static const char default_title[] = "VIEWER";
    static const char default_content[] = "USE DESKTOP TILES OR WINDOW CONTROLS";
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
    console.gui_launcher_active = 0;
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
static int gui_window_title_contains(const gui_window_t *window, uint64_t x, uint64_t y) {
    return gui_point_in_rect(x, y, window->x + 2U, window->y + 2U,
                             window->width - 4U, GUI_WINDOW_TITLE_HEIGHT);
}
static int gui_window_close_contains(const gui_window_t *window, uint64_t x, uint64_t y) {
    return gui_point_in_rect(x, y, window->x + window->width - GUI_WINDOW_CLOSE_LEFT_INSET,
                             window->y + GUI_WINDOW_CLOSE_TOP_INSET,
                             GUI_WINDOW_CLOSE_WIDTH, GUI_WINDOW_CLOSE_HEIGHT);
}
static char gui_browser_action_at_pointer(const gui_window_t *window) {
    const uint64_t content_x = window->x + 16U;
    const uint64_t content_y = window->y + 48U;
    uint64_t row;

    if ((console.gui_content_flags & MYOS_GUI_CONTENT_FLAG_BROWSER) == 0U
        || gui_point_in_rect(console.gui_pointer_x, console.gui_pointer_y, content_x, content_y,
                             window->width - 32U, window->height - 60U) == 0) {
        return '\0';
    }
    row = (console.gui_pointer_y - content_y) / 12U;
    if (row == 1U) { return (char)MYOS_INPUT_GUI_ACTION_BROWSER_PARENT; }
    if (row == 2U) { return (char)MYOS_INPUT_GUI_ACTION_BROWSER_PREVIOUS; }
    if (row >= 3U && row < 3U + MYOS_GUI_BROWSER_ENTRY_MAX) {
        return (char)(MYOS_INPUT_GUI_ACTION_BROWSER_ENTRY_BASE + row - 3U);
    }
    if (row == 3U + MYOS_GUI_BROWSER_ENTRY_MAX) { return (char)MYOS_INPUT_GUI_ACTION_BROWSER_NEXT; }
    if (row == 4U + MYOS_GUI_BROWSER_ENTRY_MAX) { return (char)MYOS_INPUT_GUI_ACTION_BROWSER_CREATE; }
    if (row == 5U + MYOS_GUI_BROWSER_ENTRY_MAX) { return (char)MYOS_INPUT_GUI_ACTION_BROWSER_CREATE_DIRECTORY; }
    if (row == 6U + MYOS_GUI_BROWSER_ENTRY_MAX) { return (char)MYOS_INPUT_GUI_ACTION_BROWSER_REMOVE; }
    if (row == 7U + MYOS_GUI_BROWSER_ENTRY_MAX) { return (char)MYOS_INPUT_GUI_ACTION_BROWSER_COPY; }
    return '\0';
}

static char gui_window_action_at_pointer(int *handled) {
    if (handled == (int *)0) {
        return '\0';
    }
    *handled = 0;
    for (uint64_t slot = GUI_WINDOW_COUNT; slot > 0U; slot--) {
        const uint8_t candidate = console.gui_z_order[slot - 1U];
        const gui_window_t *window = &console.gui_windows[candidate];

        if (window->visible == 0U) {
            continue;
        }
        if (gui_window_close_contains(window, console.gui_pointer_x, console.gui_pointer_y) != 0) {
            *handled = 1;
            if (candidate == GUI_WINDOW_NOTES) {
                return (console.gui_content_flags & MYOS_GUI_CONTENT_FLAG_EDITABLE) != 0U
                    ? '\x1b' : (char)MYOS_INPUT_GUI_ACTION_HOME;
            }
            gui_toggle_window(candidate);
            return '\0';
        }
        if (gui_window_title_contains(window, console.gui_pointer_x, console.gui_pointer_y) != 0) {
            *handled = 1;
            gui_raise_window(candidate);
            return '\0';
        }
        if (candidate == GUI_WINDOW_NOTES) {
            const char browser_action = gui_browser_action_at_pointer(window);

            if (browser_action != '\0') {
                *handled = 1;
                return browser_action;
            }
        }
    }
    return '\0';
}

static void draw_gui_window(const gui_window_t *window, int focused) {
    const uint32_t border = focused != 0 ? console_rgb(36U, 170U, 224U) : console_rgb(92U, 112U, 138U);
    const uint32_t header = focused != 0 ? console_rgb(36U, 170U, 224U) : console_rgb(54U, 76U, 106U);
    const uint32_t surface = console_rgb(229U, 235U, 243U);
    const uint32_t text = console_rgb(13U, 20U, 34U);

    fill_rect(window->x, window->y, window->width, window->height, border);
    fill_rect(window->x + 2U, window->y + 2U, window->width - 4U, window->height - 4U, surface);
    fill_rect(window->x + 2U, window->y + 2U, window->width - 4U, GUI_WINDOW_TITLE_HEIGHT, header);
    draw_gui_compact_title(window->x + 14U, window->y + 10U, window->title, surface);
    fill_rect(window->x + window->width - GUI_WINDOW_CLOSE_LEFT_INSET,
              window->y + GUI_WINDOW_CLOSE_TOP_INSET,
              GUI_WINDOW_CLOSE_WIDTH, GUI_WINDOW_CLOSE_HEIGHT, console_rgb(170U, 70U, 80U));
    draw_gui_text(window->x + window->width - GUI_WINDOW_CLOSE_LEFT_INSET + 4U,
                  window->y + GUI_WINDOW_CLOSE_TOP_INSET + 2U, "X", surface);
    if (window == &console.gui_windows[GUI_WINDOW_NOTES]) {
        draw_gui_content(window, text);
    } else {
        draw_gui_text(window->x + 16U, window->y + 48U, window->line_one, text);
        draw_gui_text(window->x + 16U, window->y + 72U, window->line_two, text);
    }
}

static void draw_gui_launcher_tile(uint64_t x, uint64_t y, const char *title, const char *subtitle,
                                   uint32_t border, uint32_t surface, uint32_t text) {
    fill_rect(x, y, GUI_LAUNCHER_TILE_WIDTH, GUI_LAUNCHER_TILE_HEIGHT, border);
    fill_rect(x + 3U, y + 3U, GUI_LAUNCHER_TILE_WIDTH - 6U, GUI_LAUNCHER_TILE_HEIGHT - 6U, surface);
    draw_gui_text(x + 12U, y + 20U, title, text);
    draw_gui_text(x + 12U, y + 48U, subtitle, text);
}

static void draw_gui_launcher(uint32_t text) {
    const uint32_t border = console_rgb(36U, 170U, 224U);
    const uint32_t notes_border = console_rgb(72U, 196U, 139U);
    const uint32_t surface = console_rgb(229U, 235U, 243U);
    const uint64_t top = gui_launcher_tile_y();

    draw_gui_text(16U, 56U, "CLICK A TILE TO OPEN", text);
    draw_gui_launcher_tile(gui_launcher_tile_x(0U), top, "SYSTEM", "CLICK", border, surface,
                           console_rgb(13U, 20U, 34U));
    draw_gui_launcher_tile(gui_launcher_tile_x(1U), top, "NOTES", "CLICK", notes_border, surface,
                           console_rgb(13U, 20U, 34U));
    draw_gui_launcher_tile(gui_launcher_tile_x(2U), top, "EDIT NOTE", "CLICK", border, surface,
                           console_rgb(13U, 20U, 34U));
    draw_gui_launcher_tile(gui_launcher_tile_x(3U), top, "FILES", "BROWSE", notes_border, surface,
                           console_rgb(13U, 20U, 34U));
    if (console.gui_launcher_app_count != 0U) {
        const uint64_t app_top = gui_launcher_app_tile_y();

        draw_gui_text(gui_launcher_app_tile_x(0U), app_top - 16U, "INSTALLED APPS", text);
        for (uint64_t index = 0U; index < console.gui_launcher_app_count; index++) {
            draw_gui_launcher_tile(gui_launcher_app_tile_x(index), app_top,
                                   console.gui_launcher_app_names[index], "OPEN APP", border, surface,
                                   console_rgb(13U, 20U, 34U));
        }
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

static uint64_t gui_format_decimal(char *destination, uint64_t capacity, uint64_t value) {
    static const char digits[] = "0123456789";
    char reversed[20];
    uint64_t count = 0U;
    uint64_t offset = 0U;

    if (destination == (char *)0 || capacity == 0U) {
        return 0U;
    }
    do {
        reversed[count++] = digits[value % UINT64_C(10)];
        value /= UINT64_C(10);
    } while (value != 0U && count < sizeof(reversed));
    while (count != 0U && offset + 1U < capacity) {
        destination[offset++] = reversed[--count];
    }
    destination[offset] = '\0';
    return offset;
}

static void gui_refresh_status_snapshot(void) {
    console.gui_task_count = scheduler_task_count();
    console.gui_runnable_count = scheduler_runnable_task_count();
    console.gui_clock_valid = rtc_read_time(&console.gui_clock);
}

static void draw_gui_clock(uint32_t text) {
    char clock_text[9] = "--:--:--";

    if (console.gui_clock_valid != 0) {
        clock_text[0] = (char)('0' + console.gui_clock.hour / 10U);
        clock_text[1] = (char)('0' + console.gui_clock.hour % 10U);
        clock_text[3] = (char)('0' + console.gui_clock.minute / 10U);
        clock_text[4] = (char)('0' + console.gui_clock.minute % 10U);
        clock_text[6] = (char)('0' + console.gui_clock.second / 10U);
        clock_text[7] = (char)('0' + console.gui_clock.second % 10U);
    }
    draw_gui_text(console.width - 112U, 12U, clock_text, text);
}

static const char *gui_focus_status_label(void) {
    static const char *const labels[GUI_WINDOW_COUNT] = { "SYSTEM", "NOTES", "MONITOR" };

    if (console.gui_launcher_active != 0) { return "HOME"; }
    if (console.gui_focus >= GUI_WINDOW_COUNT) { return "UNKNOWN"; }
    return labels[console.gui_focus];
}

static void draw_gui_status_bar(uint32_t top_bar, uint32_t text, const char *help) {
    char focus[16] = "FOCUS ";
    char status[32] = "TASKS ";
    const char *focus_label = gui_focus_status_label();
    uint64_t focus_offset = 6U;
    uint64_t offset = 6U;

    while (focus_label[focus_offset - 6U] != '\0' && focus_offset + 1U < sizeof(focus)) {
        focus[focus_offset] = focus_label[focus_offset - 6U];
        focus_offset++;
    }
    focus[focus_offset] = '\0';
    offset += gui_format_decimal(status + offset, sizeof(status) - offset, console.gui_task_count);
    if (offset + 5U < sizeof(status)) {
        status[offset++] = ' ';
        status[offset++] = 'R';
        status[offset++] = 'U';
        status[offset++] = 'N';
        status[offset++] = ' ';
        offset += gui_format_decimal(status + offset, sizeof(status) - offset, console.gui_runnable_count);
    }
    fill_rect(20U, console.height - 44U, console.width - 40U, 24U, top_bar);
    draw_gui_text(30U, console.height - 36U, help, text);
    draw_gui_text(console.width - 224U, console.height - 36U, focus, text);
    draw_gui_text(console.width - 124U, console.height - 36U, status, text);
}

static void redraw_gui_desktop(void) {
    const uint32_t desktop = console_rgb(18U, 31U, 56U);
    const uint32_t top_bar = console_rgb(25U, 54U, 92U);
    const uint32_t text = console_rgb(229U, 235U, 243U);

    console.gui_pointer_under_valid = 0;
    fill_rect(0U, 0U, console.width, console.height, desktop);
    fill_rect(0U, 0U, console.width, 32U, top_bar);
    draw_gui_text(16U, 12U, "MYOS DESKTOP", text);
    draw_gui_clock(text);
    fill_rect(console.width - 40U, 7U, 20U, 18U, console_rgb(170U, 70U, 80U));
    draw_gui_text(console.width - 34U, 12U, "X", text);
    if (console.gui_launcher_active != 0) {
        draw_gui_launcher(text);
        draw_gui_status_bar(top_bar, text, "CLICK TILE TO OPEN  TOP X OR CTRL-Q EXITS");
    } else {
        for (uint64_t slot = 0U; slot < GUI_WINDOW_COUNT; slot++) {
            const uint8_t window_id = console.gui_z_order[slot];
            const gui_window_t *window = &console.gui_windows[window_id];

            if (window->visible != 0U) {
                draw_gui_window(window, window_id == console.gui_focus);
            }
        }
        draw_gui_status_bar(top_bar, text, "TITLE RAISES  X OR ALT-F4 CLOSES  ESC BACK  CTRL-Q EXITS");
    }
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
    gui_refresh_status_snapshot();
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

void framebuffer_gui_on_timer_tick(void) {
    const uint32_t top_bar = console_rgb(25U, 54U, 92U);
    const uint32_t text = console_rgb(229U, 235U, 243U);

    if (console.gui_active == 0) {
        return;
    }
    erase_gui_pointer();
    console.gui_clock_valid = rtc_read_time(&console.gui_clock);
    fill_rect(console.width - 120U, 0U, 72U, 32U, top_bar);
    draw_gui_clock(text);
    draw_gui_pointer();
}

int framebuffer_gui_set_content(const char *title, const uint8_t *data, uint64_t length, uint64_t flags,
                                uint64_t cursor, uint64_t viewport) {
    uint64_t index;

    if (console.gui_active == 0 || title == (const char *)0 || data == (const uint8_t *)0
        || length > GUI_CONTENT_MAX
        || (flags & ~(MYOS_GUI_CONTENT_FLAG_EDITABLE | MYOS_GUI_CONTENT_FLAG_LAUNCHER
                      | MYOS_GUI_CONTENT_FLAG_BROWSER)) != 0U
        || ((flags & MYOS_GUI_CONTENT_FLAG_LAUNCHER) != 0U
            && (flags & (MYOS_GUI_CONTENT_FLAG_EDITABLE | MYOS_GUI_CONTENT_FLAG_BROWSER)) != 0U)
        || ((flags & MYOS_GUI_CONTENT_FLAG_EDITABLE) != 0U
            && (flags & MYOS_GUI_CONTENT_FLAG_BROWSER) != 0U)
        || cursor > length || viewport > length || title[0] == '\0') {
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
    console.gui_launcher_active = (flags & MYOS_GUI_CONTENT_FLAG_LAUNCHER) != 0U;
    if (console.gui_launcher_active != 0) {
        gui_launcher_refresh_apps();
    } else {
        console.gui_launcher_app_count = 0U;
    }
    console.gui_content_cursor = cursor;
    console.gui_content_viewport = gui_content_line_start(viewport);
    gui_refresh_status_snapshot();
    if (console.gui_launcher_active == 0) {
        gui_raise_window(GUI_WINDOW_NOTES);
    }
    redraw_gui_desktop();
    return 1;
}

char framebuffer_gui_handle_input(char character) {
    if (console.gui_active == 0) {
        return '\0';
    }
    if ((uint8_t)character == MYOS_INPUT_KEY_ALT_TAB) {
        gui_focus_next_window();
        redraw_gui_desktop();
        return '\0';
    }
    if ((uint8_t)character != MYOS_INPUT_KEY_ALT_F4 || console.gui_launcher_active != 0) {
        return '\0';
    }
    if (console.gui_focus == GUI_WINDOW_NOTES) {
        return (console.gui_content_flags & MYOS_GUI_CONTENT_FLAG_EDITABLE) != 0U
            ? '\x1b' : (char)MYOS_INPUT_GUI_ACTION_HOME;
    }
    gui_toggle_window(console.gui_focus);
    redraw_gui_desktop();
    return '\0';
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

char framebuffer_gui_handle_mouse(int64_t delta_x, int64_t delta_y, int left_pressed, int left_was_pressed) {
    const uint8_t previous_focus = console.gui_focus;
    char action = '\0';
    int window_chrome_handled = 0;

    if (console.gui_active == 0) {
        return '\0';
    }
    if (delta_x == 0 && delta_y == 0 && (left_pressed == 0 || left_was_pressed != 0)) {
        return '\0';
    }
    erase_gui_pointer();
    console.gui_pointer_x = gui_pointer_offset(console.gui_pointer_x, delta_x, console.width - GUI_POINTER_SIZE);
    console.gui_pointer_y = gui_pointer_offset(console.gui_pointer_y, delta_y, console.height - GUI_POINTER_SIZE);
    if (left_pressed != 0 && left_was_pressed == 0) {
        action = gui_launcher_action_at_pointer();
        if (action == '\0' && gui_pointer_hits_exit() != 0) {
            action = (char)MYOS_INPUT_GUI_ACTION_EXIT;
        }
        if (action == '\0' && console.gui_launcher_active == 0) {
            action = gui_window_action_at_pointer(&window_chrome_handled);
            if (action == '\0' && window_chrome_handled == 0) {
                gui_focus_pointer_window();
            }
        }
    }
    if (console.gui_focus != previous_focus || window_chrome_handled != 0) {
        redraw_gui_desktop();
    } else {
        draw_gui_pointer();
    }
    return action;
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
