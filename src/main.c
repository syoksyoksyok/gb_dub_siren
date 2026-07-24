#include <gb/gb.h>
#include <gbdk/platform.h>
#include <gbdk/console.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 20u
#define SCREEN_H 18u

#define START_LONG_FRAMES 14u
#define UI_REFRESH_FRAMES 3u

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
    PARAM_PITCH = 0,
    PARAM_DEPTH,
    PARAM_SPEED,
    PARAM_COUNT
} param_t;

static const int8_t sine_quarter[65] = {
    0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46,
    49, 51, 54, 57, 60, 62, 65, 67, 70, 72, 75, 77, 79, 81, 83, 85,
    87, 89, 91, 92, 94, 95, 97, 98, 99, 100, 102, 103, 104, 104, 105, 106,
    107, 107, 108, 108, 109, 109, 109, 110, 110, 110, 110, 110, 110, 110, 110, 110,
    110
};

static const char * const wave_names[WAVE_COUNT] = {
    "SINE   ",
    "SQUARE ",
    "SAW    ",
    "REV SAW"
};

static uint16_t base_pitch_hz = 440u;
static uint16_t lfo_depth_hz = 180u;
static uint8_t lfo_speed = 5u;
static uint8_t lfo_phase = 0u;
static int16_t lfo_value = 0;
static lfo_wave_t lfo_wave = WAVE_SINE;
static param_t selected_param = PARAM_PITCH;

static uint8_t joy = 0u;
static uint8_t prev_joy = 0u;
static uint8_t start_frames = 0u;
static bool start_was_long = false;

static bool desired_sound = false;
static bool sound_active = false;
static uint8_t volume = 0u;
static uint8_t fade_tick = 0u;
static uint8_t ui_tick = 0u;

static void put_text(uint8_t x, uint8_t y, const char *text) {
    gotoxy(x, y);
    while (*text) {
        putchar(*text++);
    }
}

static void put_u16_4(uint16_t v) {
    putchar((char)('0' + ((v / 1000u) % 10u)));
    putchar((char)('0' + ((v / 100u) % 10u)));
    putchar((char)('0' + ((v / 10u) % 10u)));
    putchar((char)('0' + (v % 10u)));
}

static void draw_bar(uint8_t x, uint8_t y, uint16_t value, uint16_t min, uint16_t max) {
    uint8_t i;
    uint8_t filled;
    uint16_t range = max - min;

    if (value < min) value = min;
    if (value > max) value = max;
    filled = (uint8_t)(((uint32_t)(value - min) * 10u) / range);

    gotoxy(x, y);
    putchar('[');
    for (i = 0u; i < 10u; ++i) {
        putchar(i < filled ? '#' : '-');
    }
    putchar(']');
}

static int8_t lfo_wave_value(uint8_t phase) {
    uint8_t q = phase & 0x3fu;

    switch (lfo_wave) {
    case WAVE_SINE:
        if (phase < 64u) return sine_quarter[q];
        if (phase < 128u) return sine_quarter[64u - q];
        if (phase < 192u) return (int8_t)-sine_quarter[q];
        return (int8_t)-sine_quarter[64u - q];

    case WAVE_SQUARE:
        return (phase < 128u) ? 110 : -110;

    case WAVE_SAW:
        return (int8_t)((int16_t)(((uint16_t)phase * 220u) >> 8) - 110);

    case WAVE_REV_SAW:
    default:
        return (int8_t)(110 - (int16_t)(((uint16_t)phase * 220u) >> 8));
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
    NR14_REG = 0x80u;
}

static void apu_set_frequency(uint16_t hz) {
    uint16_t gb_freq = hz_to_gb_freq(hz);
    NR13_REG = (uint8_t)(gb_freq & 0xffu);
    NR14_REG = (uint8_t)(0x80u | ((gb_freq >> 8) & 0x07u));
}

static void update_lfo(void) {
    int8_t raw;

    if (!(joy & J_SELECT)) {
        lfo_phase = (uint8_t)(lfo_phase + lfo_speed);
    }

    raw = lfo_wave_value(lfo_phase);
    lfo_value = ((int16_t)raw * (int16_t)lfo_depth_hz) / 110;
}

static void update_input(void) {
    bool start_down = (joy & J_START) != 0u;
    bool start_pressed = ((joy & J_START) && !(prev_joy & J_START));
    bool start_released = (!(joy & J_START) && (prev_joy & J_START));

    if (start_pressed) {
        start_frames = 0u;
        start_was_long = false;
    }

    if (start_down) {
        if (start_frames < 255u) ++start_frames;
        if (start_frames >= START_LONG_FRAMES) start_was_long = true;
    }

    if (start_released && !start_was_long) {
        lfo_wave = (lfo_wave_t)((lfo_wave + 1u) % WAVE_COUNT);
    }

    if ((joy & J_UP) && !(prev_joy & J_UP)) {
        if (selected_param == PARAM_PITCH) selected_param = (param_t)(PARAM_COUNT - 1u);
        else selected_param = (param_t)(selected_param - 1u);
    }

    if ((joy & J_DOWN) && !(prev_joy & J_DOWN)) {
        selected_param = (param_t)((selected_param + 1u) % PARAM_COUNT);
    }

    if (joy & J_RIGHT) {
        switch (selected_param) {
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
    } else if (joy & J_LEFT) {
        switch (selected_param) {
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
}

static void update_sound(void) {
    int16_t hz = (int16_t)base_pitch_hz + lfo_value;

    desired_sound = ((joy & J_A) != 0u) || start_was_long;

    if (hz < (int16_t)PITCH_MIN_HZ) hz = PITCH_MIN_HZ;
    if (hz > (int16_t)PITCH_MAX_HZ + (int16_t)DEPTH_MAX_HZ) hz = PITCH_MAX_HZ + DEPTH_MAX_HZ;
    apu_set_frequency((uint16_t)hz);

    if (++fade_tick >= 2u) {
        fade_tick = 0u;
        if (desired_sound) {
            if (volume < VOL_MAX) ++volume;
        } else if (volume > 0u) {
            --volume;
        }
        apu_set_volume(volume);
    }

    sound_active = volume > 0u;
}

static void draw_static_ui(void) {
    cls();
    put_text(2u, 0u, "GB DUB SIREN");
    put_text(0u, 2u, "WAVE:");
    put_text(0u, 4u, "PITCH");
    put_text(0u, 7u, "DEPTH");
    put_text(0u, 10u, "SPEED");
    put_text(0u, 12u, "LFO WAVE");
    put_text(11u, 12u, "OFF  ");
    put_text(0u, 17u, "UD SEL LR EDIT");
}

static void draw_lfo_waveform(void) {
    uint8_t row;
    uint8_t col;
    uint8_t current_col = (uint8_t)(((uint16_t)lfo_phase * SCREEN_W) >> 8);

    for (row = 0u; row < 4u; ++row) {
        gotoxy(0u, (uint8_t)(13u + row));
        for (col = 0u; col < SCREEN_W; ++col) {
            uint8_t phase = (uint8_t)(((uint16_t)col * 255u) / (SCREEN_W - 1u));
            int16_t value = (int16_t)lfo_wave_value(phase) + 110;
            uint8_t plot_row = (uint8_t)(3u - ((uint16_t)value * 3u / 220u));

            if (col == current_col && row == plot_row) putchar('@');
            else putchar(row == plot_row ? '*' : ' ');
        }
    }
}

static void draw_ui(void) {
    put_text(6u, 2u, wave_names[lfo_wave]);

    put_text(6u, 4u, selected_param == PARAM_PITCH ? ">" : " ");
    draw_bar(7u, 4u, base_pitch_hz, PITCH_MIN_HZ, PITCH_MAX_HZ);
    gotoxy(7u, 5u);
    put_u16_4(base_pitch_hz);
    put_text(11u, 5u, " Hz ");

    put_text(6u, 7u, selected_param == PARAM_DEPTH ? ">" : " ");
    draw_bar(7u, 7u, lfo_depth_hz, DEPTH_MIN_HZ, DEPTH_MAX_HZ);
    gotoxy(7u, 8u);
    put_u16_4(lfo_depth_hz);
    put_text(11u, 8u, " Hz ");

    put_text(6u, 10u, selected_param == PARAM_SPEED ? ">" : " ");
    draw_bar(7u, 10u, lfo_speed, SPEED_MIN, SPEED_MAX);
    gotoxy(7u, 11u);
    put_u16_4(lfo_speed);
    put_text(11u, 11u, " step");

    put_text(11u, 12u, sound_active ? "ON   " : "OFF  ");
    draw_lfo_waveform();
}

void main(void) {
    DISPLAY_ON;
    SHOW_BKG;
    apu_init();
    draw_static_ui();

    while (1) {
        prev_joy = joy;
        joy = joypad();

        update_input();
        update_lfo();
        update_sound();

        if (++ui_tick >= UI_REFRESH_FRAMES) {
            ui_tick = 0u;
            draw_ui();
        }

        wait_vbl_done();
    }
}

