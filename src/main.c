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
#define WAVEFORM_START_Y 11u
#define WAVEFORM_TILE_BASE 128u
#define WAVEFORM_TILE_COUNT (SCREEN_W * WAVEFORM_ROWS)

#define UI_REFRESH_FRAMES 6u
#define LFO_STEP_SCALE 96u

#define PITCH_MIN_HZ 130u
#define PITCH_MAX_HZ 2100u
#define DEPTH_MIN_HZ 0u
#define DEPTH_MAX_HZ 600u
#define SPEED_MIN 1u
#define SPEED_MAX 32u

#define VOL_MAX 12u

typedef enum {
    WAVE_SINE = 0,
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_REV_SAW,
    WAVE_COUNT
} lfo_wave_t;

typedef enum {
    PARAM_WAVE = 0,
    PARAM_PITCH,
    PARAM_DEPTH,
    PARAM_SPEED,
    PARAM_COUNT
} param_t;


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

static uint16_t base_pitch_hz = 440u;
static uint16_t lfo_depth_hz = 180u;
static uint8_t lfo_speed = 19u;
static uint16_t lfo_phase = 0u;
static int16_t lfo_value = 0;
static lfo_wave_t lfo_wave = WAVE_SINE;
static param_t selected_param = PARAM_PITCH;

static uint8_t joy = 0u;
static uint8_t prev_joy = 0u;

static bool desired_sound = false;
static bool sound_active = false;
static bool channel_triggered = false;
static uint8_t volume = 0u;
static uint8_t fade_tick = 0u;
static uint8_t ui_tick = 0u;
static bool ui_dirty = true;
static bool waveform_dirty = true;
static uint8_t waveform_tile_data[WAVEFORM_TILE_COUNT * 16u];
static uint8_t waveform_tile_map[WAVEFORM_TILE_COUNT];
static uint8_t waveform_y_pixels[WAVEFORM_PIXEL_W];

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
    return waveform_y_tables[lfo_wave][x];
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
    NR12_REG = (uint8_t)(vol << 4);
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
    bool start_pressed = ((joy & J_START) && !(prev_joy & J_START));

    if (start_pressed) {
        lfo_wave = (lfo_wave_t)((lfo_wave + 1u) % WAVE_COUNT);
        waveform_dirty = true;
        ui_dirty = true;
    }

    if ((joy & J_UP) && !(prev_joy & J_UP)) {
        if (selected_param == PARAM_WAVE) selected_param = (param_t)(PARAM_COUNT - 1u);
        else selected_param = (param_t)(selected_param - 1u);
        ui_dirty = true;
    }

    if ((joy & J_DOWN) && !(prev_joy & J_DOWN)) {
        selected_param = (param_t)((selected_param + 1u) % PARAM_COUNT);
        ui_dirty = true;
    }

    if (joy & J_RIGHT) {
        if (joy & J_B) {
            if (lfo_speed < SPEED_MAX) ++lfo_speed;
        } else if (joy & J_SELECT) {
            if (lfo_depth_hz < DEPTH_MAX_HZ - 5u) lfo_depth_hz += 5u;
            else lfo_depth_hz = DEPTH_MAX_HZ;
        } else {
            switch (selected_param) {
            case PARAM_WAVE:
                if (!(prev_joy & J_RIGHT)) {
                    lfo_wave = (lfo_wave_t)((lfo_wave + 1u) % WAVE_COUNT);
                    waveform_dirty = true;
                }
                break;
            case PARAM_PITCH:
                if (base_pitch_hz < PITCH_MAX_HZ - 5u) base_pitch_hz += 5u;
                else base_pitch_hz = PITCH_MAX_HZ;
                break;
            case PARAM_DEPTH:
                if (lfo_depth_hz < DEPTH_MAX_HZ - 5u) lfo_depth_hz += 5u;
                else lfo_depth_hz = DEPTH_MAX_HZ;
                break;
            case PARAM_SPEED:
            default:
                if (lfo_speed < SPEED_MAX) ++lfo_speed;
                break;
            }
        }
        ui_dirty = true;
    } else if (joy & J_LEFT) {
        if (joy & J_B) {
            if (lfo_speed > SPEED_MIN) --lfo_speed;
        } else if (joy & J_SELECT) {
            if (lfo_depth_hz > DEPTH_MIN_HZ + 5u) lfo_depth_hz -= 5u;
            else lfo_depth_hz = DEPTH_MIN_HZ;
        } else {
            switch (selected_param) {
            case PARAM_WAVE:
                if (!(prev_joy & J_LEFT)) {
                    if (lfo_wave == WAVE_SINE) lfo_wave = WAVE_REV_SAW;
                    else lfo_wave = (lfo_wave_t)(lfo_wave - 1u);
                    waveform_dirty = true;
                }
                break;
            case PARAM_PITCH:
                if (base_pitch_hz > PITCH_MIN_HZ + 5u) base_pitch_hz -= 5u;
                else base_pitch_hz = PITCH_MIN_HZ;
                break;
            case PARAM_DEPTH:
                if (lfo_depth_hz > DEPTH_MIN_HZ + 5u) lfo_depth_hz -= 5u;
                else lfo_depth_hz = DEPTH_MIN_HZ;
                break;
            case PARAM_SPEED:
            default:
                if (lfo_speed > SPEED_MIN) --lfo_speed;
                break;
            }
        }
        ui_dirty = true;
    }
}

static void update_sound(void) {
    bool old_sound_active = sound_active;
    int16_t hz = (int16_t)base_pitch_hz + lfo_value;

    desired_sound = ((joy & J_A) != 0u);

    if (hz < (int16_t)PITCH_MIN_HZ) hz = PITCH_MIN_HZ;
    if (hz > (int16_t)PITCH_MAX_HZ + (int16_t)DEPTH_MAX_HZ) hz = PITCH_MAX_HZ + DEPTH_MAX_HZ;

    if (++fade_tick >= 2u) {
        fade_tick = 0u;
        if (desired_sound) {
            if (volume < VOL_MAX) ++volume;
        } else if (volume > 0u) {
            --volume;
        }
        apu_set_volume(volume);
    }

    if (desired_sound && !channel_triggered && volume > 0u) {
        apu_set_frequency((uint16_t)hz, true);
        channel_triggered = true;
    } else {
        apu_set_frequency((uint16_t)hz, false);
    }

    if (!desired_sound && volume == 0u) channel_triggered = false;

    sound_active = volume > 0u;
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

static void draw_static_ui(void) {
    cls();
    put_text(2u, 0u, "GB DUB SIREN");
    put_text(0u, 2u, "WAVE:");
    put_text(0u, 4u, "PITCH");
    put_text(0u, 6u, "DEPTH");
    put_text(0u, 8u, "SPEED");
    put_text(0u, 10u, "LFO WAVE");
    put_text(11u, 10u, "SND OFF");
    put_text(0u, 17u, "UD SEL LR EDIT");
    waveform_init_map();
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

    set_bkg_data(WAVEFORM_TILE_BASE, WAVEFORM_TILE_COUNT, waveform_tile_data);
    waveform_dirty = false;
}

static void draw_lfo_marker(void) {
    uint8_t x = (uint8_t)(((uint32_t)lfo_phase * (WAVEFORM_PIXEL_W - 1u)) >> 16);
    uint8_t y = waveform_y_pixels[x];

    move_sprite(0u, (uint8_t)(8u + x), (uint8_t)(16u + (WAVEFORM_START_Y * 8u) + y));
}

static void draw_ui(void) {
    put_text(5u, 2u, selected_param == PARAM_WAVE ? ">" : " ");
    put_text(6u, 2u, wave_names[lfo_wave]);

    put_text(6u, 4u, selected_param == PARAM_PITCH ? ">" : " ");
    draw_slider(7u, 4u, base_pitch_hz, PITCH_MIN_HZ, PITCH_MAX_HZ);
    put_text(6u, 6u, selected_param == PARAM_DEPTH ? ">" : " ");
    draw_slider(7u, 6u, lfo_depth_hz, DEPTH_MIN_HZ, DEPTH_MAX_HZ);

    put_text(6u, 8u, selected_param == PARAM_SPEED ? ">" : " ");
    draw_slider(7u, 8u, lfo_speed, SPEED_MIN, SPEED_MAX);

    put_text(11u, 10u, sound_active ? "SND ON " : "SND OFF");
    ui_dirty = false;
}

void main(void) {
    DISPLAY_ON;
    SHOW_BKG;
    SHOW_SPRITES;
    apu_init();
    draw_static_ui();
    rebuild_lfo_waveform_cache();
    draw_ui();

    while (1) {
        prev_joy = joy;
        joy = joypad();

        update_input();
        update_lfo();
        update_sound();

        if (waveform_dirty) rebuild_lfo_waveform_cache();
        draw_lfo_marker();

        if (++ui_tick >= UI_REFRESH_FRAMES) {
            ui_tick = 0u;
            if (ui_dirty) draw_ui();
        }

        wait_vbl_done();
    }
}






