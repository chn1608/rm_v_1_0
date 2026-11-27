#include "hmi.h"
#include "hmi_text.h"
#include <stdio.h>
#include <string.h>

#define ROW_FIRST 2u
#define FOOTER_FIRST (ROW_FIRST + HMI_MAX_ROWS)
#define BAND_COUNT (FOOTER_FIRST + 2u)
#define ALL_BANDS ((uint16_t)((1u << BAND_COUNT) - 1u))

enum {
    COLOR_TOP = 0x1083, COLOR_HEAD = 0x19a8, COLOR_BACKGROUND = 0x08e3,
    COLOR_ROW = 0x1126, COLOR_SELECTED = 0x1a0b, COLOR_RULE = 0x2167,
    COLOR_TEXT = 0xf7df, COLOR_LABEL = 0xb69b, COLOR_ACCENT = 0x379e,
    COLOR_UNKNOWN = 0xf5ac
};

typedef enum {
    FMT_STATE, FMT_CONTROL, FMT_LINK, FMT_ENABLE, FMT_ESTOP, FMT_DIRECTION,
    FMT_RELAY, FMT_COMMAND, FMT_HEALTH, FMT_AVAILABLE, FMT_VOLT, FMT_AMP,
    FMT_PERMILLE, FMT_DEGREES, FMT_PULSE, FMT_RPM, FMT_COUNT, FMT_MS, FMT_SECONDS,
    FMT_VERSION, FMT_FAULT
} Format;

typedef struct {
    HmiField field;
    const char *label;
    Format format;
} Row;

typedef struct {
    const char *title;
    const Row *rows;
    uint8_t count;
} Page;

static const Row home[] = {
    {HMI_BUS_MV, T_BUS_MV, FMT_VOLT},
    {HMI_BLE_CONNECTED, T_BLE_CONNECTED, FMT_LINK},
    {HMI_SYSTEM_STATE, T_SYSTEM_STATE, FMT_STATE},
    {HMI_CONTROL_MODE, T_CONTROL_MODE, FMT_CONTROL},
    {HMI_POWER_ENABLED, T_POWER_ENABLED, FMT_ENABLE},
    {HMI_ESTOP, T_ESTOP, FMT_ESTOP},
    {HMI_FAULT_CODE, T_FAULT_CODE, FMT_FAULT}
};

static const Row motor[] = {
    {HMI_MOTOR_CMD_L, T_MOTOR_CMD_L, FMT_PERMILLE},
    {HMI_MOTOR_CMD_R, T_MOTOR_CMD_R, FMT_PERMILLE},
    {HMI_MOTOR_PWM_L, T_MOTOR_PWM_L, FMT_PERMILLE},
    {HMI_MOTOR_PWM_R, T_MOTOR_PWM_R, FMT_PERMILLE},
    {HMI_MOTOR_DIRECTION_L, T_MOTOR_DIRECTION_L, FMT_DIRECTION},
    {HMI_MOTOR_DIRECTION_R, T_MOTOR_DIRECTION_R, FMT_DIRECTION},
    {HMI_MOTOR_ENABLED, T_MOTOR_ENABLED, FMT_ENABLE},
    {HMI_MOTOR_RPM_L, T_MOTOR_RPM_L, FMT_RPM},
    {HMI_MOTOR_RPM_R, T_MOTOR_RPM_R, FMT_RPM}
};

static const Row servo[] = {
    {HMI_SERVO_TARGET_MDEG, T_SERVO_TARGET, FMT_DEGREES},
    {HMI_SERVO_PULSE_US, T_SERVO_PULSE, FMT_PULSE},
    {HMI_SERVO_CENTER_US, T_SERVO_CENTER, FMT_PULSE},
    {HMI_SERVO_ANGLE_MIN_MDEG, T_SERVO_ANGLE_MIN, FMT_DEGREES},
    {HMI_SERVO_ANGLE_MAX_MDEG, T_SERVO_ANGLE_MAX, FMT_DEGREES},
    {HMI_SERVO_PULSE_MIN_US, T_SERVO_PULSE_MIN, FMT_PULSE},
    {HMI_SERVO_PULSE_MAX_US, T_SERVO_PULSE_MAX, FMT_PULSE},
    {HMI_SERVO_POSITION_MDEG, T_SERVO_POSITION, FMT_DEGREES}
};

static const Row comm[] = {
    {HMI_BLE_CONNECTED, T_BLE_CONNECTED, FMT_LINK},
    {HMI_PC_CONNECTED, T_PC_CONNECTED, FMT_LINK},
    {HMI_VALID_FRAMES, T_VALID_FRAMES, FMT_COUNT},
    {HMI_CRC_ERRORS, T_CRC_ERRORS, FMT_COUNT},
    {HMI_COMM_TIMEOUTS, T_COMM_TIMEOUTS, FMT_COUNT},
    {HMI_LAST_COMMAND_MS, T_LAST_COMMAND, FMT_MS}
};

static const Row faults[] = {
    {HMI_ESTOP, T_ESTOP, FMT_ESTOP},
    {HMI_POWER_COMMAND, T_POWER_COMMAND, FMT_COMMAND},
    {HMI_BUS_MV, T_BUS_MV, FMT_VOLT},
    {HMI_CURRENT_MA, T_CURRENT, FMT_AMP},
    {HMI_RELAY_FEEDBACK, T_RELAY_FEEDBACK, FMT_RELAY},
    {HMI_FAULT_CODE, T_FAULT_CODE, FMT_FAULT},
    {HMI_SYSTEM_ERROR, T_SYSTEM_ERROR, FMT_FAULT}
};

static const Row settings[] = {
    {HMI_PARAM_VIEW, T_PARAM_VIEW, FMT_AVAILABLE},
    {HMI_PARAM_EDIT, T_PARAM_EDIT, FMT_AVAILABLE},
    {HMI_PARAM_RESET, T_PARAM_RESET, FMT_AVAILABLE},
    {HMI_PARAM_SAVE, T_PARAM_SAVE, FMT_AVAILABLE},
    {HMI_VERSION, T_VERSION, FMT_VERSION},
    {HMI_UPTIME_S, T_UPTIME, FMT_SECONDS},
    {HMI_TASK_HEALTH, T_TASK_HEALTH, FMT_HEALTH}
};

#define PAGE(title, rows) {title, rows, (uint8_t)(sizeof(rows) / sizeof((rows)[0]))}
static const Page pages[HMI_PAGE_COUNT] = {
    PAGE(T_HOME, home), PAGE(T_MOTOR, motor), PAGE(T_SERVO, servo),
    PAGE(T_COMM, comm), PAGE(T_FAULTS, faults), PAGE(T_SETTINGS, settings)
};
#undef PAGE

typedef struct {
    uint16_t code;
    uint16_t rows[14];
} Glyph;
#include "hmi_font.h"

typedef struct {
    uint8_t *pixels;
    uint16_t top;
    uint16_t height;
} Strip;

static unsigned glyph_advance(uint16_t code)
{
    return code < 128u ? 8u : GLYPH_SIZE;
}

/* All strings come from an internal UTF-8 catalog or ASCII value formatters. */
static uint16_t next_code(const char **text)
{
    const unsigned char *p = (const unsigned char *)*text;
    uint16_t result = *p++;
    if (result >= 0xe0u) {
        const uint16_t middle = *p++;
        result = (uint16_t)(((result & 15u) << 12) | ((middle & 63u) << 6) | (*p++ & 63u));
    } else if (result >= 0xc0u) {
        result = (uint16_t)(((result & 31u) << 6) | (*p++ & 63u));
    }
    *text = (const char *)p;
    return result;
}

static const Glyph *find_glyph(uint16_t code)
{
    unsigned low = 0u;
    unsigned high = sizeof(font) / sizeof(font[0]);
    while (low < high) {
        const unsigned middle = low + (high - low) / 2u;
        if (font[middle].code < code) low = middle + 1u;
        else if (font[middle].code > code) high = middle;
        else return &font[middle];
    }
    return NULL;
}

static void pixel(Strip *strip, unsigned x, unsigned y, uint16_t color)
{
    if (x >= HMI_WIDTH || y < strip->top || y - strip->top >= strip->height) return;
    uint8_t *out = strip->pixels + ((y - strip->top) * HMI_WIDTH + x) * 2u;
    out[0] = (uint8_t)(color >> 8);
    out[1] = (uint8_t)color;
}

static void rect(Strip *strip, unsigned x, unsigned y, unsigned width, unsigned height, uint16_t color)
{
    const unsigned start = y < strip->top ? strip->top : y;
    const unsigned end = y + height < strip->top + strip->height ? y + height : strip->top + strip->height;
    for (unsigned row = start; row < end; ++row) {
        uint8_t *out = strip->pixels + ((row - strip->top) * HMI_WIDTH + x) * 2u;
        for (unsigned col = 0; col < width; ++col) {
            *out++ = (uint8_t)(color >> 8);
            *out++ = (uint8_t)color;
        }
    }
}

static unsigned text_width(const char *text)
{
    unsigned width = 0u;
    while (*text) width += glyph_advance(next_code(&text));
    return width;
}

static void draw_text(Strip *strip, unsigned x, unsigned y, const char *text, uint16_t color)
{
    while (*text) {
        const uint16_t code = next_code(&text);
        const Glyph *glyph = find_glyph(code);
        if (glyph == NULL) glyph = find_glyph('?');
        const unsigned advance = glyph_advance(code);
        for (unsigned iy = 0; iy < GLYPH_SIZE; ++iy) {
            for (unsigned ix = 0; ix < advance; ++ix) {
                if ((glyph->rows[iy] & (1u << (GLYPH_SIZE - 1u - ix))) != 0u)
                    pixel(strip, x + ix, y + iy, color);
            }
        }
        x += advance;
    }
}

static const char *boolean(int32_t value, const char *no, const char *yes)
{
    if (value == 0) return no;
    if (value == 1) return yes;
    return NULL;
}

static void fixed_value(char *buffer, unsigned size, int32_t number, unsigned divisor, const char *unit)
{
    const uint32_t magnitude = number < 0 ? 0u - (uint32_t)number : (uint32_t)number;
    (void)snprintf(buffer, size, "%s%lu.%lu %s", number < 0 ? "-" : "",
                   (unsigned long)(magnitude / divisor),
                   (unsigned long)((magnitude % divisor) / (divisor / 10u)), unit);
}

static const char *format_value(Format format, HmiValue reading, char *buffer, unsigned size)
{
    if (!reading.valid) return NULL;
    const int32_t v = reading.value;

    static const char *const states[] = {
        T_BOOT, T_SELF_TEST, T_STANDBY, T_LOCAL_CONTROL, T_REMOTE_CONTROL, T_FAULT_LOCKED, T_ESTOP_ACTIVE
    };
    static const char *const modes[] = {T_NO_OWNER, T_LOCAL, T_BLE, T_PC};
    static const char *const directions[] = {T_REVERSE, T_STOP, T_FORWARD};
    switch (format) {
    case FMT_STATE: return v >= 0 && v < 7 ? states[v] : NULL;
    case FMT_CONTROL: return v >= 0 && v < 4 ? modes[v] : NULL;
    case FMT_DIRECTION: return v >= -1 && v <= 1 ? directions[v + 1] : NULL;
    case FMT_LINK: return boolean(v, T_DISCONNECTED, T_CONNECTED);
    case FMT_ENABLE: return boolean(v, T_DISABLED, T_ENABLED);
    case FMT_ESTOP: return boolean(v, T_CLEAR, T_TRIGGERED);
    case FMT_RELAY: return boolean(v, T_RELAY_OPEN, T_RELAY_CLOSED);
    case FMT_COMMAND: return boolean(v, T_STOP, T_COMMAND_ON);
    case FMT_HEALTH: return boolean(v, T_TASK_ABNORMAL, T_NO_FAULT);
    case FMT_AVAILABLE: return v == 1 ? T_AVAILABLE : NULL;
    case FMT_VOLT: fixed_value(buffer, size, v, 1000u, "V"); break;
    case FMT_AMP: fixed_value(buffer, size, v, 1000u, "A"); break;
    case FMT_PERMILLE: fixed_value(buffer, size, v, 10u, "%"); break;
    case FMT_DEGREES: fixed_value(buffer, size, v, 1000u, "deg"); break;
    case FMT_PULSE: (void)snprintf(buffer, size, "%ld us", (long)v); break;
    case FMT_RPM: (void)snprintf(buffer, size, "%ld rpm", (long)v); break;
    case FMT_MS: (void)snprintf(buffer, size, "%lu ms", (unsigned long)(uint32_t)v); break;
    case FMT_SECONDS: (void)snprintf(buffer, size, "%lu s", (unsigned long)(uint32_t)v); break;
    case FMT_COUNT: (void)snprintf(buffer, size, "%lu", (unsigned long)(uint32_t)v); break;
    case FMT_VERSION:
        if (v < 0 || v > 99999) return NULL;
        (void)snprintf(buffer, size, "V%ld.%ld", (long)(v / 100), (long)(v % 100));
        break;
    case FMT_FAULT:
        if (v == 0) return T_NO_FAULT;
        (void)snprintf(buffer, size, "0x%08lX", (unsigned long)(uint32_t)v);
        break;
    }
    return buffer;
}

static void right_text(Strip *strip, unsigned edge, unsigned y, const char *text, uint16_t color)
{
    const unsigned width = text_width(text);
    if (width <= edge) draw_text(strip, edge - width, y, text, color);
}

static bool row_has_value(const Hmi *hmi, const Page *page, unsigned row)
{
    return hmi->detail ? row == 1u : row < page->count;
}

static const Row *row_at(const Hmi *hmi, const Page *page, unsigned row)
{
    return &page->rows[hmi->detail ? hmi->selected : row];
}

static void draw_band(Strip *strip, const Hmi *hmi, const Page *page, unsigned band,
                      uint16_t top, uint16_t height, HmiValue value)
{
    if (band < ROW_FIRST) {
        rect(strip, 0u, top, HMI_WIDTH, height, band == 0u ? COLOR_TOP : COLOR_HEAD);
        rect(strip, 0u, top, 4u, height, COLOR_ACCENT);
        if (band == 0u) {
            char position[10];
            (void)snprintf(position, sizeof(position), "%02u/%02u", (unsigned)hmi->page + 1u,
                           (unsigned)HMI_PAGE_COUNT);
            draw_text(strip, 13u, top + 5u, T_BRAND, COLOR_TEXT);
            right_text(strip, 225u, top + 5u, position, COLOR_ACCENT);
        } else {
            draw_text(strip, 13u, top + 5u, page->title, COLOR_TEXT);
            if (hmi->detail) right_text(strip, 225u, top + 5u, T_DETAIL, COLOR_ACCENT);
        }
    } else if (band >= FOOTER_FIRST) {
        rect(strip, 0u, top, HMI_WIDTH, height, COLOR_TOP);
        if (band == FOOTER_FIRST)
            draw_text(strip, 13u, top + 2u, hmi->detail ? T_BACK_DETAIL : T_NAVIGATION, COLOR_LABEL);
        else if (!hmi->detail)
            draw_text(strip, 13u, top + 1u, T_OPEN_DETAIL, COLOR_ACCENT);
    } else {
        const unsigned row = band - ROW_FIRST;
        const bool selected = !hmi->detail && row == hmi->selected;
        const bool show_value = row_has_value(hmi, page, row);
        rect(strip, 0u, top, HMI_WIDTH, height,
             selected ? COLOR_SELECTED : show_value ? COLOR_ROW : COLOR_BACKGROUND);
        if (selected) rect(strip, 0u, top, 4u, height, COLOR_ACCENT);
        if (show_value) {
            const Row *item = row_at(hmi, page, row);
            char buffer[32] = {0};
            const char *formatted = format_value(item->format, value, buffer, sizeof(buffer));
            draw_text(strip, 13u, top + 4u, hmi->detail ? T_DETAILS_VALUE : item->label, COLOR_LABEL);
            right_text(strip, 225u, top + 4u, formatted ? formatted : T_UNKNOWN,
                       formatted ? COLOR_TEXT : COLOR_UNKNOWN);
            rect(strip, 13u, top + height - 1u, 212u, 1u, COLOR_RULE);
        } else if (hmi->detail && row == 0u) {
            draw_text(strip, 13u, top + 4u, page->rows[hmi->selected].label, COLOR_ACCENT);
        }
    }
}

static bool same_value(HmiValue a, HmiValue b)
{
    return a.valid == b.valid && (!a.valid || a.value == b.value);
}

void hmi_init(Hmi *hmi)
{
    memset(hmi, 0, sizeof(*hmi));
    hmi->drawn_page = HMI_PAGE_COUNT;
    hmi->pending = ALL_BANDS;
}

void hmi_handle_key(Hmi *hmi, Key key)
{
    if (hmi->detail) {
        if (key == KEY_CENTER || key == KEY_LEFT) hmi->detail = false;
        return;
    }

    switch (key) {
    case KEY_LEFT:
        hmi->page = (HmiPage)((hmi->page + HMI_PAGE_COUNT - 1u) % HMI_PAGE_COUNT);
        hmi->selected = 0u;
        break;
    case KEY_RIGHT:
        hmi->page = (HmiPage)((hmi->page + 1u) % HMI_PAGE_COUNT);
        hmi->selected = 0u;
        break;
    case KEY_UP:
        if (hmi->selected != 0u) --hmi->selected;
        break;
    case KEY_DOWN:
        if (hmi->selected + 1u < pages[hmi->page].count) ++hmi->selected;
        break;
    case KEY_CENTER:
        hmi->detail = true;
        break;
    default:
        break;
    }
}

bool hmi_has_pending(const Hmi *hmi)
{
    return hmi->pending != 0u || hmi->drawn_page != hmi->page ||
           hmi->drawn_detail != hmi->detail || hmi->drawn_selection != hmi->selected;
}

bool hmi_render(Hmi *hmi, const HmiDisplay *display, const HmiSnapshot *snapshot)
{
    const Page *page = &pages[hmi->page];
    if (hmi->drawn_page != hmi->page || hmi->drawn_detail != hmi->detail) {
        hmi->drawn_page = hmi->page;
        hmi->drawn_detail = hmi->detail;
        hmi->pending = ALL_BANDS;
        hmi->next_band = 0u;
    } else if (hmi->drawn_selection != hmi->selected) {
        hmi->pending |= (uint16_t)((1u << (ROW_FIRST + hmi->drawn_selection)) |
                                   (1u << (ROW_FIRST + hmi->selected)));
    }
    hmi->drawn_selection = hmi->selected;

    for (unsigned row = 0; row < HMI_MAX_ROWS; ++row) {
        if (row_has_value(hmi, page, row) &&
            !same_value(snapshot->fields[row_at(hmi, page, row)->field], hmi->drawn_values[row]))
            hmi->pending |= (uint16_t)(1u << (row + ROW_FIRST));
    }
    if (hmi->pending == 0u) return true;

    unsigned band = hmi->next_band;
    while ((hmi->pending & (1u << band)) == 0u) band = (band + 1u) % BAND_COUNT;

    const unsigned row = band >= ROW_FIRST ? band - ROW_FIRST : HMI_MAX_ROWS;
    const bool body = band >= ROW_FIRST && band < FOOTER_FIRST;
    const bool has_reading = body && row_has_value(hmi, page, row);
    const HmiValue reading = has_reading ? snapshot->fields[row_at(hmi, page, row)->field] :
                                          (HmiValue){0, false};
    uint16_t top, height;
    if (band < ROW_FIRST) {
        top = (uint16_t)(24u * band);
        height = 24u;
    } else if (body) {
        top = (uint16_t)(48u + 22u * row);
        height = 22u;
    } else {
        top = (uint16_t)(246u + 17u * (band - FOOTER_FIRST));
        height = 17u;
    }

    for (uint16_t offset = 0u; offset < height; offset += HMI_STRIP_HEIGHT) {
        const unsigned remaining = height - offset;
        const uint16_t lines = remaining < HMI_STRIP_HEIGHT ? (uint16_t)remaining : HMI_STRIP_HEIGHT;
        Strip strip = {hmi->strip, (uint16_t)(top + offset), lines};
        draw_band(&strip, hmi, page, band, top, height, reading);
        if (!display->blit(display->context, 0u, strip.top, HMI_WIDTH, strip.height, hmi->strip))
            return false;
    }
    if (body) hmi->drawn_values[row] = reading;
    hmi->pending &= (uint16_t)~(1u << band);
    hmi->next_band = (uint8_t)((band + 1u) % BAND_COUNT);
    return true;
}
