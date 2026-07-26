#include <gb/gb.h>
#include <gbdk/platform.h>
#include <gbdk/console.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lfo_tables.h"

#define SCREEN_W 20u
#define WAVEFORM_ROWS 6u
#define WAVEFORM_PIXEL_W 160u
#define WAVEFORM_PIXEL_H 48u
#define WAVEFORM_CENTER_Y ((WAVEFORM_PIXEL_H - 1u) / 2u)
#define WAVEFORM_START_Y 11u
#define WAVEFORM_TILE_BASE 128u
#define WAVEFORM_TILE_COUNT (SCREEN_W * WAVEFORM_ROWS)
#define WAVEFORM_UPLOAD_TILES_PER_FRAME 4u

#define UI_REFRESH_FRAMES 6u
#define WAVEFORM_DEPTH_REDRAW_DELAY_FRAMES 10u
#define LFO_STEP_SCALE 174u

#define PITCH_MIN_HZ 130u
#define PITCH_MAX_HZ 2100u
#define DEPTH_MIN_HZ 0u
#define DEPTH_MAX_HZ 600u
#define PITCH_STEP_HZ 25u
#define DEPTH_STEP_HZ 5u
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
    0x18u, 0x18u, 0x3cu, 0x3cu, 0x7eu, 0x7eu, 0xffu, 0xffu,
    0xffu, 0xffu, 0x7eu, 0x7eu, 0x3cu, 0x3cu, 0x18u, 0x18u
};

static const char * const wave_names[WAVE_COUNT] = {
    "SINE   ",
    "SQUARE ",
    "SAW    ",
    "REV SAW"
};

static const uint8_t phase_to_marker_x[256] = {
    0u, 0u, 1u, 1u, 2u, 3u, 3u, 4u, 4u, 5u, 6u, 6u, 7u, 8u, 8u, 9u,
    9u, 10u, 11u, 11u, 12u, 13u, 13u, 14u, 14u, 15u, 16u, 16u, 17u, 18u, 18u, 19u,
    19u, 20u, 21u, 21u, 22u, 22u, 23u, 24u, 24u, 25u, 26u, 26u, 27u, 27u, 28u, 29u,
    29u, 30u, 31u, 31u, 32u, 32u, 33u, 34u, 34u, 35u, 36u, 36u, 37u, 37u, 38u, 39u,
    39u, 40u, 40u, 41u, 42u, 42u, 43u, 44u, 44u, 45u, 45u, 46u, 47u, 47u, 48u, 49u,
    49u, 50u, 50u, 51u, 52u, 52u, 53u, 54u, 54u, 55u, 55u, 56u, 57u, 57u, 58u, 59u,
    59u, 60u, 60u, 61u, 62u, 62u, 63u, 63u, 64u, 65u, 65u, 66u, 67u, 67u, 68u, 68u,
    69u, 70u, 70u, 71u, 72u, 72u, 73u, 73u, 74u, 75u, 75u, 76u, 77u, 77u, 78u, 78u,
    79u, 80u, 80u, 81u, 81u, 82u, 83u, 83u, 84u, 85u, 85u, 86u, 86u, 87u, 88u, 88u,
    89u, 90u, 90u, 91u, 91u, 92u, 93u, 93u, 94u, 95u, 95u, 96u, 96u, 97u, 98u, 98u,
    99u, 99u, 100u, 101u, 101u, 102u, 103u, 103u, 104u, 104u, 105u, 106u, 106u, 107u, 108u, 108u,
    109u, 109u, 110u, 111u, 111u, 112u, 113u, 113u, 114u, 114u, 115u, 116u, 116u, 117u, 118u, 118u,
    119u, 119u, 120u, 121u, 121u, 122u, 122u, 123u, 124u, 124u, 125u, 126u, 126u, 127u, 127u, 128u,
    129u, 129u, 130u, 131u, 131u, 132u, 132u, 133u, 134u, 134u, 135u, 136u, 136u, 137u, 137u, 138u,
    139u, 139u, 140u, 140u, 141u, 142u, 142u, 143u, 144u, 144u, 145u, 145u, 146u, 147u, 147u, 148u,
    149u, 149u, 150u, 150u, 151u, 152u, 152u, 153u, 154u, 154u, 155u, 155u, 156u, 157u, 157u, 158u
};

static uint16_t base_pitch_hz = 440u;
static uint16_t lfo_depth_hz = 400u;
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
        putchar(i == knob ? 'O' : '-');
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
    int16_t source_y = (int16_t)waveform_y_tables[lfo_wave][x];
    int16_t centered_y = source_y - (int16_t)WAVEFORM_CENTER_Y;
    int16_t scaled_y = (int16_t)WAVEFORM_CENTER_Y + ((centered_y * (int16_t)lfo_depth_hz) / (int16_t)DEPTH_MAX_HZ);

    if (scaled_y < 0) return 0u;
    if (scaled_y >= (int16_t)WAVEFORM_PIXEL_H) return (uint8_t)(WAVEFORM_PIXEL_H - 1u);
    return (uint8_t)scaled_y;
}

static void request_waveform_rebuild(uint8_t delay) {
    waveform_dirty = true;
    waveform_redraw_delay = delay;
    waveform_upload_index = WAVEFORM_TILE_COUNT;
}

static void waveform_set_pixel(uint8_t x, uint8_t y) {
    uint8_t tile_col = x >> 3;
    uint8_t tile_row = y >> 3;
    uint16_t tile_index = (uint16_t)tile_row * SCREEN_W + tile_col;
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

    if ((joy & J_START) && (joy & J_SELECT) && !((prev_joy & J_START) && (prev_joy & J_SELECT))) {
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

    if ((joy & J_START) && !(joy & J_SELECT)) {
        if (lfo_depth_hz < DEPTH_MAX_HZ - DEPTH_STEP_HZ) lfo_depth_hz += DEPTH_STEP_HZ;
        else lfo_depth_hz = DEPTH_MAX_HZ;
        if (old_lfo_depth_hz != lfo_depth_hz) {
            ui_dirty = true;
            request_waveform_rebuild(WAVEFORM_DEPTH_REDRAW_DELAY_FRAMES);
        }
        return;
    } else if ((joy & J_SELECT) && !(joy & J_START)) {
        if (lfo_depth_hz > DEPTH_MIN_HZ + DEPTH_STEP_HZ) lfo_depth_hz -= DEPTH_STEP_HZ;
        else lfo_depth_hz = DEPTH_MIN_HZ;
        if (old_lfo_depth_hz != lfo_depth_hz) {
            ui_dirty = true;
            request_waveform_rebuild(WAVEFORM_DEPTH_REDRAW_DELAY_FRAMES);
        }
        return;
    }

    if ((joy & J_UP) && !(prev_joy & J_UP)) {
        if (lfo_wave == WAVE_SINE) lfo_wave = WAVE_REV_SAW;
        else lfo_wave = (lfo_wave_t)(lfo_wave - 1u);
        request_waveform_rebuild(0u);
        ui_dirty = true;
    }

    if ((joy & J_DOWN) && !(prev_joy & J_DOWN)) {
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

    set_bkg_tiles(0u, WAVEFORM_START_Y, SCREEN_W, WAVEFORM_ROWS, waveform_tile_map);
    set_sprite_data(0u, 1u, marker_tile);
    set_sprite_tile(0u, 0u);
}

static void draw_help_ui(void) {
    cls();
    move_sprite(0u, 0u, 0u);
    put_text(8u, 0u, "HELP");
    put_text(0u, 2u, "A HOLD SOUND");
    put_text(0u, 4u, "^ v CHANGE WAVE");
    put_text(0u, 6u, "< > WITH A OR B");
    put_text(0u, 8u, "ST + SE HELP");
    put_text(0u, 10u, "A + < > PITCH");
    put_text(0u, 12u, "B + < > RATE");
    put_text(0u, 14u, "SEL / ST DEPTH");
    put_text(0u, 17u, "ST + SE CLOSE");
}
static void draw_static_ui(void) {
    cls();
    put_text(2u, 0u, "> GB DUB SIREN <");
    put_text(0u, 2u, "WAVE:");
    put_text(0u, 4u, "PITCH");
    put_text(0u, 6u, "DEPTH");
    put_text(0u, 8u, "RATE ");
    put_text(0u, 10u, "LFO WAVE");
    put_text(0u, 17u, "^ v WAVE ST/SE DP");
    waveform_init_map();
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
    uint8_t y = waveform_y_pixels[x];

    move_sprite(0u, (uint8_t)(8u + x), (uint8_t)(16u + (WAVEFORM_START_Y * 8u) + y));
}

static void draw_ui(void) {
    put_text(6u, 2u, wave_names[lfo_wave]);

    draw_slider(7u, 4u, base_pitch_hz, PITCH_MIN_HZ, PITCH_MAX_HZ);
    draw_slider(7u, 6u, lfo_depth_hz, DEPTH_MIN_HZ, DEPTH_MAX_HZ);
    draw_slider(7u, 8u, lfo_speed, SPEED_MIN, SPEED_MAX);

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










