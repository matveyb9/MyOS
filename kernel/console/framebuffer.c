#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include <framebuffer.h>

#define CELL_WIDTH 8U
#define CELL_HEIGHT 8U
#define MAX_COLUMNS 160U
#define MAX_ROWS 100U
#define ANSI_ESC 0x1BU

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

static void draw_gui_text(uint64_t x, uint64_t y, const char *text, uint32_t colour) {
    while (*text != '\0') {
        for (uint64_t row = 0U; row < 7U; row++) {
            const uint8_t bits = glyph_row(*text, (uint8_t)row);
            for (uint64_t column = 0U; column < 5U; column++) {
                if ((bits & (uint8_t)(UINT8_C(1) << (4U - column))) != 0U) {
                    put_pixel(x + column, y + row, colour);
                }
            }
        }
        x += 7U;
        text++;
    }
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
    const uint32_t desktop = console_rgb(18U, 31U, 56U);
    const uint32_t top_bar = console_rgb(25U, 54U, 92U);
    const uint32_t window = console_rgb(229U, 235U, 243U);
    const uint32_t header = console_rgb(36U, 170U, 224U);
    const uint32_t text = console_rgb(13U, 20U, 34U);
    const uint64_t window_x = console.width / 10U;
    const uint64_t window_y = console.height / 5U;
    const uint64_t window_width = console.width - (window_x * 2U);
    const uint64_t window_height = console.height - window_y - (console.height / 8U);

    if (console.active == 0 || console.gui_active != 0 || console.width < 160U || console.height < 120U) {
        return 0;
    }
    console.gui_active = 1;
    fill_rect(0U, 0U, console.width, console.height, desktop);
    fill_rect(0U, 0U, console.width, 32U, top_bar);
    draw_gui_text(16U, 12U, "MYOS DESKTOP", window);
    fill_rect(window_x, window_y, window_width, window_height, window);
    fill_rect(window_x, window_y, window_width, 24U, header);
    draw_gui_text(window_x + 12U, window_y + 9U, "WELCOME", window);
    draw_gui_text(window_x + 20U, window_y + 48U, "MYOS GUI BRINGUP", text);
    draw_gui_text(window_x + 20U, window_y + 72U, "FRAMEBUFFER DESKTOP ACTIVE", text);
    draw_gui_text(window_x + 20U, window_y + 104U, "ESC OR Q RETURNS TO SHELL", text);
    fill_rect(20U, console.height - 36U, console.width - 40U, 20U, top_bar);
    draw_gui_text(30U, console.height - 29U, "STARTGUI  CONSOLE SESSION", window);
    return 1;
}

void framebuffer_gui_end(void) {
    if (console.gui_active == 0) {
        return;
    }
    console.gui_active = 0;
    framebuffer_console_clear();
}

int framebuffer_gui_active(void) {
    return console.gui_active;
}

void framebuffer_gui_handle_input(char character) {
    const uint32_t accent = console_rgb(36U, 170U, 224U);
    const uint64_t marker_x = console.width > 24U ? console.width - 24U : 0U;
    const uint64_t marker_y = console.height > 24U ? console.height - 24U : 0U;

    if (console.gui_active == 0) {
        return;
    }
    if (character >= ' ' && character <= '~') {
        fill_rect(marker_x, marker_y, 8U, 8U, accent);
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
