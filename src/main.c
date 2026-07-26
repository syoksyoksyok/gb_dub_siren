#include <gb/gb.h>
#include <gbdk/platform.h>
#include <gbdk/console.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lfo_tables.h"
#include "waveform_depth_lut.h"

#define SCREEN_W 20u
#define WAVEFORM_COLS 16u
#define WAVEFORM_ROWS 3u
#define WAVEFORM_PIXEL_W 128u
#define WAVEFORM_PIXEL_H 24u
#define WAVEFORM_CENTER_Y ((WAVEFORM_PIXEL_H - 1u) / 2u)
#define WAVEFORM_START_X 2u
#define WAVEFORM_START_Y 12u
#define WAVEFORM_FRAME_X 1u
#define WAVEFORM_FRAME_W 18u
#define WAVEFORM_FRAME_RIGHT_X (WAVEFORM_FRAME_X + WAVEFORM_FRAME_W - 1u)
#define WAVEFORM_FRAME_TOP_Y (WAVEFORM_START_Y - 1u)
#define WAVEFORM_FRAME_BOTTOM_Y (WAVEFORM_START_Y + WAVEFORM_ROWS)
#define WAVEFORM_TILE_BASE 128u
#define WAVEFORM_TILE_COUNT (WAVEFORM_COLS * WAVEFORM_ROWS)
#define SLIDER_KNOB_TILE (WAVEFORM_TILE_BASE + WAVEFORM_TILE_COUNT)
#define WAVEFORM_FRAME_TILE_BASE 248u
#define WAVEFORM_FRAME_TOP_LEFT_TILE WAVEFORM_FRAME_TILE_BASE
#define WAVEFORM_FRAME_TOP_RIGHT_TILE (WAVEFORM_FRAME_TILE_BASE + 1u)
#define WAVEFORM_FRAME_BOTTOM_LEFT_TILE (WAVEFORM_FRAME_TILE_BASE + 2u)
#define WAVEFORM_FRAME_BOTTOM_RIGHT_TILE (WAVEFORM_FRAME_TILE_BASE + 3u)
#define WAVEFORM_FRAME_TOP_HORIZONTAL_TILE (WAVEFORM_FRAME_TILE_BASE + 4u)
#define WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE (WAVEFORM_FRAME_TILE_BASE + 5u)
#define WAVEFORM_FRAME_LEFT_VERTICAL_TILE (WAVEFORM_FRAME_TILE_BASE + 6u)
#define WAVEFORM_FRAME_RIGHT_VERTICAL_TILE (WAVEFORM_FRAME_TILE_BASE + 7u)
#define WAVEFORM_UPLOAD_TILES_PER_FRAME 4u

#define SPRITE_LFO_MARKER 0u
#define SPRITE_TITLE_LEFT 1u
#define SPRITE_TITLE_RIGHT 2u
#define TILE_LFO_MARKER 0u
#define TILE_TITLE_LEFT_SPEAKER 1u
#define TILE_TITLE_RIGHT_SPEAKER 2u

#define UI_REFRESH_FRAMES 6u
#define WAVEFORM_DEPTH_REDRAW_DELAY_FRAMES 3u
#define LFO_STEP_SCALE 174u

#define PITCH_MIN_HZ 130u
#define PITCH_MAX_HZ 2100u
#define DEPTH_MIN_HZ 0u
#define DEPTH_MAX_HZ 600u
#define PITCH_STEP_HZ 25u
#define DEPTH_STEP_HZ 10u
#define SPEED_MIN 1u
#define SPEED_MAX 80u

#define VOL_MAX 15u

typedef enum {
    WAVE_SINE = 0,
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_REV_SAW,
    WAVE_COUNT
} lfo_wave_t;


static const uint8_t marker_tile[16] = {
    0x00u, 0x00u, 0x18u, 0x18u, 0x3cu, 0x3cu, 0x7eu, 0x7eu,
    0x7eu, 0x7eu, 0x3cu, 0x3cu, 0x18u, 0x18u, 0x00u, 0x00u
};

static const uint8_t title_speaker_tiles[32] = {
    0x00u, 0x00u, 0x30u, 0x30u, 0x3cu, 0x3cu, 0xfeu, 0xfeu,
    0xfeu, 0xfeu, 0x3cu, 0x3cu, 0x30u, 0x30u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x0cu, 0x0cu, 0x3cu, 0x3cu, 0x7fu, 0x7fu,
    0x7fu, 0x7fu, 0x3cu, 0x3cu, 0x0cu, 0x0cu, 0x00u, 0x00u
};

static const uint8_t waveform_frame_tiles[128] = {
    0x00u, 0x00u, 0x00u, 0x7fu, 0x00u, 0x7fu, 0x00u, 0x60u,
    0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u,
    0x00u, 0x00u, 0x00u, 0xfeu, 0x00u, 0xfeu, 0x00u, 0x06u,
    0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u,
    0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u,
    0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x7fu, 0x00u, 0x7fu,
    0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u,
    0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0xfeu, 0x00u, 0xfeu,
    0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu,
    0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u,
    0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u, 0x00u, 0x60u,
    0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u,
    0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u, 0x00u, 0x06u
};

static const uint8_t slider_knob_tile[16] = {
    0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu,
    0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu, 0x3cu
};

static const uint8_t waveform_frame_top_map[WAVEFORM_FRAME_W] = {
    WAVEFORM_FRAME_TOP_LEFT_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_HORIZONTAL_TILE, WAVEFORM_FRAME_TOP_HORIZONTAL_TILE,
    WAVEFORM_FRAME_TOP_RIGHT_TILE
};

static const uint8_t waveform_frame_bottom_map[WAVEFORM_FRAME_W] = {
    WAVEFORM_FRAME_BOTTOM_LEFT_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE, WAVEFORM_FRAME_BOTTOM_HORIZONTAL_TILE,
    WAVEFORM_FRAME_BOTTOM_RIGHT_TILE
};

static const uint8_t waveform_frame_left_map[1] = { WAVEFORM_FRAME_LEFT_VERTICAL_TILE };
static const uint8_t waveform_frame_right_map[1] = { WAVEFORM_FRAME_RIGHT_VERTICAL_TILE };

static const char * const wave_names[WAVE_COUNT] = {
    "SINE   ",
    "SQUARE ",
    "SAW    ",
    "REV SAW"
};

static const uint8_t waveform_source_x[WAVEFORM_PIXEL_W] = {
    0u, 1u, 2u, 3u, 5u, 6u, 7u, 8u, 10u, 11u, 12u, 13u, 15u, 16u, 17u, 18u,
    20u, 21u, 22u, 23u, 25u, 26u, 27u, 28u, 30u, 31u, 32u, 33u, 35u, 36u, 37u, 38u,
    40u, 41u, 42u, 43u, 45u, 46u, 47u, 48u, 50u, 51u, 52u, 53u, 55u, 56u, 57u, 58u,
    60u, 61u, 62u, 63u, 65u, 66u, 67u, 68u, 70u, 71u, 72u, 73u, 75u, 76u, 77u, 78u,
    80u, 81u, 82u, 83u, 85u, 86u, 87u, 88u, 90u, 91u, 92u, 93u, 95u, 96u, 97u, 98u,
    100u, 101u, 102u, 103u, 105u, 106u, 107u, 108u, 110u, 111u, 112u, 113u, 115u, 116u, 117u, 118u,
    120u, 121u, 122u, 123u, 125u, 126u, 127u, 128u, 130u, 131u, 132u, 133u, 135u, 136u, 137u, 138u,
    140u, 141u, 142u, 143u, 145u, 146u, 147u, 148u, 150u, 151u, 152u, 153u, 155u, 156u, 157u, 159u
};

static const uint8_t phase_to_marker_x[256] = {
    0u, 0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 6u, 6u, 7u,
    7u, 8u, 8u, 9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u, 14u, 14u, 15u,
    15u, 16u, 16u, 17u, 17u, 18u, 18u, 19u, 19u, 20u, 20u, 21u, 21u, 22u, 22u, 23u,
    23u, 24u, 24u, 25u, 25u, 26u, 26u, 27u, 27u, 28u, 28u, 29u, 29u, 30u, 30u, 31u,
    31u, 32u, 32u, 33u, 33u, 34u, 34u, 35u, 35u, 36u, 36u, 37u, 37u, 38u, 38u, 39u,
    39u, 40u, 40u, 41u, 41u, 42u, 42u, 43u, 43u, 44u, 44u, 45u, 45u, 46u, 46u, 47u,
    47u, 48u, 48u, 49u, 49u, 50u, 50u, 51u, 51u, 52u, 52u, 53u, 53u, 54u, 54u, 55u,
    55u, 56u, 56u, 57u, 57u, 58u, 58u, 59u, 59u, 60u, 60u, 61u, 61u, 62u, 62u, 63u,
    63u, 63u, 64u, 64u, 65u, 65u, 66u, 66u, 67u, 67u, 68u, 68u, 69u, 69u, 70u, 70u,
    71u, 71u, 72u, 72u, 73u, 73u, 74u, 74u, 75u, 75u, 76u, 76u, 77u, 77u, 78u, 78u,
    79u, 79u, 80u, 80u, 81u, 81u, 82u, 82u, 83u, 83u, 84u, 84u, 85u, 85u, 86u, 86u,
    87u, 87u, 88u, 88u, 89u, 89u, 90u, 90u, 91u, 91u, 92u, 92u, 93u, 93u, 94u, 94u,
    95u, 95u, 96u, 96u, 97u, 97u, 98u, 98u, 99u, 99u, 100u, 100u, 101u, 101u, 102u, 102u,
    103u, 103u, 104u, 104u, 105u, 105u, 106u, 106u, 107u, 107u, 108u, 108u, 109u, 109u, 110u, 110u,
    111u, 111u, 112u, 112u, 113u, 113u, 114u, 114u, 115u, 115u, 116u, 116u, 117u, 117u, 118u, 118u,
    119u, 119u, 120u, 120u, 121u, 121u, 122u, 122u, 123u, 123u, 124u, 124u, 125u, 125u, 126u, 126u
};

static uint16_t base_pitch_hz = 440u;
static uint16_t lfo_depth_hz = 400u;
static uint8_t lfo_depth_index = 80u;
static uint8_t lfo_speed = 19u;
static uint16_t lfo_phase = 0u;
static int16_t lfo_value = 0;
static lfo_wave_t lfo_wave = WAVE_SQUARE;

static uint8_t joy = 0u;
static uint8_t prev_joy = 0u;

static bool desired_sound = false;
static bool sound_active = false;
static bool channel_triggered = false;
static uint8_t volume = 0u;
static uint8_t applied_volume = 0xffu;
static uint8_t ui_tick = 0u;
static bool ui_dirty = true;
static bool help_visible = false;
static bool waveform_dirty = true;
static uint8_t waveform_redraw_delay = 0u;
static uint8_t waveform_tile_data[WAVEFORM_TILE_COUNT * 16u];
static uint8_t waveform_tile_map[WAVEFORM_TILE_COUNT];
static uint8_t waveform_y_pixels[WAVEFORM_PIXEL_W];
static uint8_t waveform_upload_index = WAVEFORM_TILE_COUNT;

static void draw_help_ui(void);
static void draw_static_ui(void);
static void draw_ui(void);
static void hide_title_sprites(void);
static void show_title_sprites(void);
static void upload_all_lfo_waveform_tiles(void);

static void put_text(uint8_t x, uint8_t y, const char *text) {
    gotoxy(x, y);
    while (*text) {
        putchar(*text++);
    }
}

static void draw_slider(uint8_t x, uint8_t y, uint16_t value, uint16_t min, uint16_t max) {
    uint8_t i;
    uint8_t knob;
    uint16_t range = max - min;

    if (value < min) value = min;
    if (value > max) value = max;
    knob = (uint8_t)(((uint32_t)(value - min) * 9u) / range);

    gotoxy(x, y);
    putchar('[');
    for (i = 0u; i < 10u; ++i) {
        if (i == knob) {
            putchar(' ');
            set_bkg_tile_xy((uint8_t)(x + 1u + i), y, SLIDER_KNOB_TILE);
        } else {
            putchar('.');
        }
    }
    putchar(']');
}

static int8_t lfo_wave_sample(uint8_t phase) {
    return lfo_tables[lfo_wave][phase];
}

static int16_t lfo_wave_value(uint16_t phase) {
    int16_t a;
    int16_t b;
    int16_t delta;
    uint8_t whole = (uint8_t)(phase >> 8);
    uint8_t frac = (uint8_t)(phase & 0xffu);

    if (lfo_wave == WAVE_SQUARE) return lfo_wave_sample(whole);

    a = lfo_wave_sample(whole);
    b = lfo_wave_sample((uint8_t)(whole + 1u));
    delta = b - a;
    return (int16_t)(a + ((delta * frac) >> 8));
}

static uint8_t waveform_y_for_x(uint8_t x) {
    uint8_t source_y = waveform_y_tables[lfo_wave][waveform_source_x[x]];
    uint8_t half_y = source_y >> 1;

    if (half_y >= WAVEFORM_CENTER_Y) {
        uint8_t delta = waveform_depth_delta_lut[lfo_depth_index][half_y - WAVEFORM_CENTER_Y];
        uint8_t scaled_y = (uint8_t)(WAVEFORM_CENTER_Y + delta);
        if (scaled_y >= WAVEFORM_PIXEL_H) return (uint8_t)(WAVEFORM_PIXEL_H - 1u);
        return scaled_y;
    }

    return (uint8_t)(WAVEFORM_CENTER_Y - waveform_depth_delta_lut[lfo_depth_index][WAVEFORM_CENTER_Y - half_y]);
}

static void request_waveform_rebuild(uint8_t delay) {
    waveform_dirty = true;
    waveform_redraw_delay = delay;
    waveform_upload_index = WAVEFORM_TILE_COUNT;
}

static void waveform_set_pixel(uint8_t x, uint8_t y) {
    uint8_t tile_col = x >> 3;
    uint8_t tile_row = y >> 3;
    uint16_t tile_index = (uint16_t)tile_row * WAVEFORM_COLS + tile_col;
    uint16_t offset = (tile_index * 16u) + ((uint16_t)(y & 0x07u) * 2u);
    uint8_t mask = (uint8_t)(0x80u >> (x & 0x07u));

    waveform_tile_data[offset] |= mask;
    waveform_tile_data[offset + 1u] |= mask;
}

static void waveform_draw_vertical(uint8_t x, uint8_t y0, uint8_t y1) {
    uint8_t y;

    if (y0 > y1) {
        uint8_t tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    for (y = y0; y <= y1; ++y) {
        waveform_set_pixel(x, y);
    }
}

static uint16_t hz_to_gb_freq(uint16_t hz) {
    uint16_t period;

    if (hz < 64u) hz = 64u;
    if (hz > 8192u) hz = 8192u;

    period = (uint16_t)(131072UL / hz);
    if (period > 2047u) return 1u;
    if (period == 0u) return 2047u;
    return (uint16_t)(2048u - period);
}

static void apu_init(void) {
    NR52_REG = 0x80u;
    NR50_REG = 0x77u;
    NR51_REG = 0x11u;
    NR10_REG = 0x00u;
    NR11_REG = 0x80u;
    NR12_REG = 0x00u;
}

static void apu_set_volume(uint8_t vol) {
    if (vol > 15u) vol = 15u;
    if (vol == applied_volume) return;
    NR12_REG = (uint8_t)(vol << 4);
    applied_volume = vol;
}

static void apu_set_frequency(uint16_t hz, bool trigger) {
    uint16_t gb_freq = hz_to_gb_freq(hz);
    NR13_REG = (uint8_t)(gb_freq & 0xffu);
    NR14_REG = (uint8_t)((trigger ? 0x80u : 0x00u) | ((gb_freq >> 8) & 0x07u));
}

static void update_lfo(void) {
    lfo_phase = (uint16_t)(lfo_phase + ((uint16_t)lfo_speed * LFO_STEP_SCALE));
    lfo_value = (lfo_wave_value(lfo_phase) * (int16_t)lfo_depth_hz) / 110;
}

static void update_input(void) {
    uint16_t old_base_pitch_hz = base_pitch_hz;
    uint16_t old_lfo_depth_hz = lfo_depth_hz;
    uint8_t old_lfo_speed = lfo_speed;

    if ((joy & J_START) && (joy & J_SELECT) && (joy & J_A) && (joy & J_B) &&
        !((prev_joy & J_START) && (prev_joy & J_SELECT) && (prev_joy & J_A) && (prev_joy & J_B))) {
        help_visible = !help_visible;
        if (help_visible) {
            draw_help_ui();
        } else {
            draw_static_ui();
            upload_all_lfo_waveform_tiles();
            draw_ui();
        }
        return;
    }

    if (help_visible) return;

    if ((joy & J_UP) && !(joy & J_DOWN)) {
        if (lfo_depth_hz < DEPTH_MAX_HZ - DEPTH_STEP_HZ) lfo_depth_hz += DEPTH_STEP_HZ;
        else lfo_depth_hz = DEPTH_MAX_HZ;
        if (old_lfo_depth_hz != lfo_depth_hz) {
            if (lfo_depth_index < (uint8_t)(WAVEFORM_DEPTH_STEP_COUNT - 2u)) lfo_depth_index = (uint8_t)(lfo_depth_index + 2u);
            else lfo_depth_index = (uint8_t)(WAVEFORM_DEPTH_STEP_COUNT - 1u);
            ui_dirty = true;
            request_waveform_rebuild(WAVEFORM_DEPTH_REDRAW_DELAY_FRAMES);
        }
        return;
    } else if ((joy & J_DOWN) && !(joy & J_UP)) {
        if (lfo_depth_hz > DEPTH_MIN_HZ + DEPTH_STEP_HZ) lfo_depth_hz -= DEPTH_STEP_HZ;
        else lfo_depth_hz = DEPTH_MIN_HZ;
        if (old_lfo_depth_hz != lfo_depth_hz) {
            if (lfo_depth_index > 1u) lfo_depth_index = (uint8_t)(lfo_depth_index - 2u);
            else lfo_depth_index = 0u;
            ui_dirty = true;
            request_waveform_rebuild(WAVEFORM_DEPTH_REDRAW_DELAY_FRAMES);
        }
        return;
    }

    if ((joy & J_START) && !(prev_joy & J_START) && !(joy & J_SELECT)) {
        if (lfo_wave == WAVE_SINE) lfo_wave = WAVE_REV_SAW;
        else lfo_wave = (lfo_wave_t)(lfo_wave - 1u);
        request_waveform_rebuild(0u);
        ui_dirty = true;
    }

    if ((joy & J_SELECT) && !(prev_joy & J_SELECT) && !(joy & J_START)) {
        lfo_wave = (lfo_wave_t)((lfo_wave + 1u) % WAVE_COUNT);
        request_waveform_rebuild(0u);
        ui_dirty = true;
    }

    if (joy & J_RIGHT) {
        if ((joy & J_A) && (joy & J_B)) {
            if (lfo_speed < SPEED_MAX) ++lfo_speed;
        } else if (joy & J_A) {
            if (base_pitch_hz < PITCH_MAX_HZ - PITCH_STEP_HZ) base_pitch_hz += PITCH_STEP_HZ;
            else base_pitch_hz = PITCH_MAX_HZ;
        } else if (joy & J_B) {
            if (lfo_speed < SPEED_MAX) ++lfo_speed;
        }
    } else if (joy & J_LEFT) {
        if ((joy & J_A) && (joy & J_B)) {
            if (lfo_speed > SPEED_MIN) --lfo_speed;
        } else if (joy & J_A) {
            if (base_pitch_hz > PITCH_MIN_HZ + PITCH_STEP_HZ) base_pitch_hz -= PITCH_STEP_HZ;
            else base_pitch_hz = PITCH_MIN_HZ;
        } else if (joy & J_B) {
            if (lfo_speed > SPEED_MIN) --lfo_speed;
        }
    }

    if ((old_base_pitch_hz != base_pitch_hz) || (old_lfo_speed != lfo_speed)) {
        ui_dirty = true;
    }
}
static void update_sound(void) {
    bool old_sound_active = sound_active;
    int16_t hz;

    desired_sound = ((joy & J_A) != 0u);
    volume = desired_sound ? VOL_MAX : 0u;
    apu_set_volume(volume);

    if (!desired_sound) {
        channel_triggered = false;
        sound_active = false;
        if (sound_active != old_sound_active) ui_dirty = true;
        return;
    }
    hz = (int16_t)base_pitch_hz + lfo_value;

    if (hz < (int16_t)PITCH_MIN_HZ) hz = PITCH_MIN_HZ;
    if (hz > (int16_t)PITCH_MAX_HZ + (int16_t)DEPTH_MAX_HZ) hz = PITCH_MAX_HZ + DEPTH_MAX_HZ;

    if (!channel_triggered) {
        apu_set_frequency((uint16_t)hz, true);
        channel_triggered = true;
    } else {
        apu_set_frequency((uint16_t)hz, false);
    }

    sound_active = true;
    if (sound_active != old_sound_active) ui_dirty = true;
}

static void waveform_init_map(void) {
    uint8_t i;

    for (i = 0u; i < WAVEFORM_TILE_COUNT; ++i) {
        waveform_tile_map[i] = (uint8_t)(WAVEFORM_TILE_BASE + i);
    }

    set_bkg_tiles(WAVEFORM_START_X, WAVEFORM_START_Y, WAVEFORM_COLS, WAVEFORM_ROWS, waveform_tile_map);
    set_sprite_data(TILE_LFO_MARKER, 1u, marker_tile);
    set_sprite_data(TILE_TITLE_LEFT_SPEAKER, 2u, title_speaker_tiles);
    set_sprite_tile(SPRITE_LFO_MARKER, TILE_LFO_MARKER);
    set_sprite_tile(SPRITE_TITLE_LEFT, TILE_TITLE_LEFT_SPEAKER);
    set_sprite_tile(SPRITE_TITLE_RIGHT, TILE_TITLE_RIGHT_SPEAKER);
}

static void draw_waveform_frame(void) {
    uint8_t y;

    set_bkg_data(WAVEFORM_FRAME_TILE_BASE, 8u, waveform_frame_tiles);
    set_bkg_tiles(WAVEFORM_FRAME_X, WAVEFORM_FRAME_TOP_Y, WAVEFORM_FRAME_W, 1u, waveform_frame_top_map);
    set_bkg_tiles(WAVEFORM_FRAME_X, WAVEFORM_FRAME_BOTTOM_Y, WAVEFORM_FRAME_W, 1u, waveform_frame_bottom_map);
    for (y = WAVEFORM_START_Y; y < WAVEFORM_FRAME_BOTTOM_Y; ++y) {
        set_bkg_tiles(WAVEFORM_FRAME_X, y, 1u, 1u, waveform_frame_left_map);
        set_bkg_tiles(WAVEFORM_FRAME_RIGHT_X, y, 1u, 1u, waveform_frame_right_map);
    }
}

static void hide_title_sprites(void) {
    move_sprite(SPRITE_TITLE_LEFT, 0u, 0u);
    move_sprite(SPRITE_TITLE_RIGHT, 0u, 0u);
}

static void show_title_sprites(void) {
    move_sprite(SPRITE_TITLE_LEFT, 24u, 24u);
    move_sprite(SPRITE_TITLE_RIGHT, 144u, 24u);
}

static void draw_help_ui(void) {
    cls();
    move_sprite(SPRITE_LFO_MARKER, 0u, 0u);
    hide_title_sprites();
    put_text(8u, 0u, "HELP");
    put_text(0u, 2u, "A HOLD SOUND");
    put_text(0u, 4u, "ST/SE CHANGE WAVE");
    put_text(0u, 6u, "< > WITH A OR B");
    put_text(0u, 8u, "^ / v DEPTH");
    put_text(0u, 10u, "A + < > PITCH");
    put_text(0u, 12u, "B + < > RATE");
    put_text(0u, 14u, "ST+SE+A+B HELP");
    put_text(0u, 17u, "ST+SE+A+B CLOSE");
}
static void draw_static_ui(void) {
    cls();
    set_bkg_data(SLIDER_KNOB_TILE, 1u, slider_knob_tile);
    put_text(4u, 1u, "GB DUB SIREN");
    put_text(1u, 3u, "LFO WAVE :");
    put_text(1u, 5u, "PITCH");
    put_text(1u, 7u, "DEPTH");
    put_text(1u, 9u, "RATE ");
    draw_waveform_frame();
    waveform_init_map();
    show_title_sprites();
}

static void upload_all_lfo_waveform_tiles(void) {
    set_bkg_data(WAVEFORM_TILE_BASE, WAVEFORM_TILE_COUNT, waveform_tile_data);
    waveform_upload_index = WAVEFORM_TILE_COUNT;
}

static void upload_lfo_waveform_tiles_step(void) {
    uint8_t count;

    if (waveform_upload_index >= WAVEFORM_TILE_COUNT) return;

    count = WAVEFORM_UPLOAD_TILES_PER_FRAME;
    if ((uint8_t)(WAVEFORM_TILE_COUNT - waveform_upload_index) < count) {
        count = (uint8_t)(WAVEFORM_TILE_COUNT - waveform_upload_index);
    }

    set_bkg_data((uint8_t)(WAVEFORM_TILE_BASE + waveform_upload_index), count, &waveform_tile_data[(uint16_t)waveform_upload_index * 16u]);
    waveform_upload_index = (uint8_t)(waveform_upload_index + count);
}

static void rebuild_lfo_waveform_cache(void) {
    uint8_t x;
    uint8_t prev_y = waveform_y_for_x(0u);

    memset(waveform_tile_data, 0, sizeof(waveform_tile_data));
    waveform_y_pixels[0] = prev_y;
    waveform_set_pixel(0u, prev_y);

    for (x = 1u; x < WAVEFORM_PIXEL_W; ++x) {
        uint8_t y = waveform_y_for_x(x);
        waveform_y_pixels[x] = y;
        waveform_draw_vertical(x, prev_y, y);
        prev_y = y;
    }

    waveform_upload_index = 0u;
    waveform_dirty = false;
}

static void draw_lfo_marker(void) {
    uint8_t x = phase_to_marker_x[(uint8_t)(lfo_phase >> 8)];
    uint8_t y = waveform_y_for_x(x);

    move_sprite(SPRITE_LFO_MARKER, (uint8_t)(5u + (WAVEFORM_START_X * 8u) + x), (uint8_t)(13u + (WAVEFORM_START_Y * 8u) + y));
}

static void draw_ui(void) {
    put_text(12u, 3u, wave_names[lfo_wave]);

    draw_slider(7u, 5u, base_pitch_hz, PITCH_MIN_HZ, PITCH_MAX_HZ);
    draw_slider(7u, 7u, lfo_depth_hz, DEPTH_MIN_HZ, DEPTH_MAX_HZ);
    draw_slider(7u, 9u, lfo_speed, SPEED_MIN, SPEED_MAX);

    ui_dirty = false;
}
void main(void) {
    DISPLAY_OFF;
    apu_init();
    draw_static_ui();
    rebuild_lfo_waveform_cache();
    upload_all_lfo_waveform_tiles();
    draw_ui();
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    while (1) {
        wait_vbl_done();
        upload_lfo_waveform_tiles_step();

        prev_joy = joy;
        joy = joypad();

        update_input();
        update_lfo();
        update_sound();

        if (help_visible) continue;

        draw_lfo_marker();

        if (waveform_dirty) {
            if (waveform_redraw_delay > 0u) --waveform_redraw_delay;
            else rebuild_lfo_waveform_cache();
        }

        if (++ui_tick >= UI_REFRESH_FRAMES) {
            ui_tick = 0u;
            if (ui_dirty) draw_ui();
        }

    }
}










