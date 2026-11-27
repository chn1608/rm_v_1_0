#include "hmi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 240u
#define SCREEN_HEIGHT 280u

typedef struct {
    uint8_t pixels[SCREEN_HEIGHT][SCREEN_WIDTH * 2u];
    unsigned writes;
} Screen;

static bool blit(void *context, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 const uint8_t *pixels)
{
    Screen *screen = context;
    assert(w > 0u && h > 0u && x + w <= SCREEN_WIDTH && y + h <= SCREEN_HEIGHT);
    assert(pixels != NULL);
    for (unsigned row = 0; row < h; ++row) {
        memcpy(&screen->pixels[y + row][x * 2u], pixels + row * w * 2u, w * 2u);
    }
    ++screen->writes;
    return true;
}

static void draw(Hmi *hmi, Screen *screen, const HmiSnapshot *snapshot)
{
    const HmiDisplay display = {.context = screen, .blit = blit};
    unsigned attempts = 0;
    do {
        assert(hmi_render(hmi, &display, snapshot));
        assert(++attempts <= 50u);
    } while (hmi_has_pending(hmi));
}

static void save_frame(const char *folder, unsigned page, const Screen *screen)
{
    char path[1024];
    const int length = snprintf(path, sizeof(path), "%s/page-%u.ppm", folder, page + 1u);
    assert(length > 0 && (size_t)length < sizeof(path));
    FILE *out = fopen(path, "wb");
    assert(out != NULL);
    fprintf(out, "P6\n%u %u\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    for (unsigned row = 0; row < SCREEN_HEIGHT; ++row) {
        for (unsigned x = 0; x < SCREEN_WIDTH * 2u; x += 2u) {
            const uint16_t px = (uint16_t)(((uint16_t)screen->pixels[row][x] << 8) |
                                          screen->pixels[row][x + 1u]);
            const uint8_t rgb[] = {(uint8_t)(((px >> 11) & 31u) * 255u / 31u),
                                   (uint8_t)(((px >> 5) & 63u) * 255u / 63u),
                                   (uint8_t)((px & 31u) * 255u / 31u)};
            assert(fwrite(rgb, 1u, sizeof(rgb), out) == sizeof(rgb));
        }
    }
    assert(fclose(out) == 0);
}

int main(int argc, char **argv)
{
    static Hmi hmi, other;
    static Screen screen, other_screen;
    HmiSnapshot bench = {0};
    HmiSnapshot unavailable = {0};
    /* Intentionally leave nonzero backing values on invalid fields. */
    for (unsigned i = 0; i < HMI_FIELD_COUNT; ++i) unavailable.fields[i].value = 123456;
    bench.fields[HMI_VERSION] = (HmiValue){102, true};
    bench.fields[HMI_UPTIME_S] = (HmiValue){0, true};
    unavailable.fields[HMI_VERSION] = bench.fields[HMI_VERSION];
    unavailable.fields[HMI_UPTIME_S] = bench.fields[HMI_UPTIME_S];

    hmi_init(&hmi);
    hmi_init(&other);
    for (unsigned page = 0; page < HMI_PAGE_COUNT; ++page) {
        assert((unsigned)hmi.page == page && (unsigned)other.page == page);
        draw(&hmi, &screen, &bench);
        draw(&other, &other_screen, &unavailable);
        assert(memcmp(screen.pixels, other_screen.pixels, sizeof(screen.pixels)) == 0);

        const unsigned idle_writes = screen.writes;
        draw(&hmi, &screen, &bench);
        assert(screen.writes == idle_writes);
        if (argc == 2) save_frame(argv[1], page, &screen);
        hmi_handle_key(&hmi, KEY_RIGHT);
        hmi_handle_key(&other, KEY_RIGHT);
    }
    assert(hmi.page == HMI_PAGE_HOME);
    hmi_handle_key(&hmi, KEY_LEFT);
    assert(hmi.page == HMI_PAGE_SETTINGS);
    hmi_handle_key(&hmi, KEY_RIGHT);
    assert(hmi.page == HMI_PAGE_HOME);

    draw(&hmi, &screen, &bench);
    unavailable.fields[HMI_BUS_MV] = (HmiValue){16000, true};
    draw(&other, &other_screen, &unavailable);
    assert(memcmp(screen.pixels, other_screen.pixels, sizeof(screen.pixels)) != 0);
    hmi_handle_key(&hmi, KEY_DOWN);
    hmi_handle_key(&hmi, KEY_CENTER);
    assert(hmi.detail);
    draw(&hmi, &screen, &bench);
    puts("LCD six pages, invalid-value equivalence, known values, navigation and draw bounds passed");
    return 0;
}
