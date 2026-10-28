#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_timer.h"

#define FAULT_OVERLOAD  (1U << 0)
#define FAULT_STALL     (1U << 1)
#define FAULT_LOW_VOLT  (1U << 2)
#define FAULT_HIGH_TEMP (1U << 3)
#define FAULT_SENSOR    (1U << 4)

#define FRAME_TIMEOUT_MS 1500

static i2c_master_dev_handle_t oled_device = NULL;
static bool oled_ready = false;

static const uint8_t digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static const uint8_t letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}  /* Z */
};

static const uint8_t *glyph(char c)
{
    static const uint8_t blank[5] = {0,0,0,0,0};
    static const uint8_t dot[5] = {0,0x60,0x60,0,0};
    static const uint8_t colon[5] = {0,0x36,0x36,0,0};
    static const uint8_t dash[5] = {0x08,0x08,0x08,0x08,0x08};
    static const uint8_t percent[5] = {0x63,0x13,0x08,0x64,0x63};

    if (c >= '0' && c <= '9') return digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c >= 'a' && c <= 'z') return letters[c - 'a'];

    switch (c) {
        case '.': return dot;
        case ':': return colon;
        case '-': return dash;
        case '%': return percent;
        default: return blank;
    }
}

static esp_err_t oled_command(uint8_t command)
{
    uint8_t bytes[2] = {0x00, command};
    return i2c_master_transmit(oled_device, bytes, sizeof(bytes), 100);
}

static esp_err_t oled_line(uint8_t page, const char *text)
{
    if (page > 7) return ESP_ERR_INVALID_ARG;

    esp_err_t err = oled_command(0xB0 | page);
    if (err != ESP_OK) return err;

    err = oled_command(0x00);
    if (err != ESP_OK) return err;

    err = oled_command(0x10);
    if (err != ESP_OK) return err;

    uint8_t data[129] = {0};
    data[0] = 0x40;

    for (int pos = 0; pos < 21 && text[pos] != '\0'; pos++) {
        const uint8_t *shape = glyph(text[pos]);

        for (int col = 0; col < 5; col++) {
            data[1 + pos * 6 + col] = shape[col];
        }
    }

    return i2c_master_transmit(oled_device, data, sizeof(data), 100);
}

/* Original large text: two OLED pages per line. */
static esp_err_t oled_big_line(uint8_t first_page, const char *text)
{
    if (first_page > 6) return ESP_ERR_INVALID_ARG;

    uint8_t top[129] = {0};
    uint8_t bottom[129] = {0};

    top[0] = 0x40;
    bottom[0] = 0x40;

    for (int pos = 0; pos < 10 && text[pos] != '\0'; pos++) {
        const uint8_t *shape = glyph(text[pos]);

        for (int col = 0; col < 5; col++) {
            uint16_t expanded = 0;

            for (int bit = 0; bit < 7; bit++) {
                if (shape[col] & (1U << bit)) {
                    expanded |= (uint16_t)3U << (bit * 2);
                }
            }

            int x = 1 + pos * 12 + col * 2;

            top[x] = (uint8_t)expanded;
            top[x + 1] = (uint8_t)expanded;

            bottom[x] = (uint8_t)(expanded >> 8);
            bottom[x + 1] = (uint8_t)(expanded >> 8);
        }
    }

    for (int half = 0; half < 2; half++) {
        uint8_t page = first_page + half;

        esp_err_t err = oled_command(0xB0 | page);
        if (err != ESP_OK) return err;

        err = oled_command(0x00);
        if (err != ESP_OK) return err;

        err = oled_command(0x10);
        if (err != ESP_OK) return err;

        uint8_t *data = half == 0 ? top : bottom;

        err = i2c_master_transmit(oled_device, data, 129, 100);
        if (err != ESP_OK) return err;
    }

    return ESP_OK;
}

static bool oled_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = GPIO_NUM_22,
        .sda_io_num = GPIO_NUM_21,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true
    };

    i2c_master_bus_handle_t bus;

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
    if (err != ESP_OK) {
        printf("OLED: I2C setup failed: %s\n", esp_err_to_name(err));
        return false;
    }

    uint8_t address;

    if (i2c_master_probe(bus, 0x3C, 100) == ESP_OK) {
        address = 0x3C;
    } else if (i2c_master_probe(bus, 0x3D, 100) == ESP_OK) {
        address = 0x3D;
    } else {
        printf("OLED: no display found at 0x3C or 0x3D\n");
        return false;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000
    };

    err = i2c_master_bus_add_device(bus, &device_config, &oled_device);
    if (err != ESP_OK) {
        printf("OLED: device setup failed: %s\n", esp_err_to_name(err));
        return false;
    }

    const uint8_t setup[] = {
        0xAE, 0xD5,0x80, 0xA8,0x3F, 0xD3,0x00,
        0x40, 0x8D,0x14, 0x20,0x02, 0xA1, 0xC8,
        0xDA,0x12, 0x81,0x7F, 0xD9,0xF1,
        0xDB,0x40, 0xA4, 0xA6, 0xAF
    };

    for (size_t i = 0; i < sizeof(setup); i++) {
        err = oled_command(setup[i]);

        if (err != ESP_OK) {
            printf("OLED: initialization failed: %s\n",
                   esp_err_to_name(err));
            return false;
        }
    }

    for (uint8_t page = 0; page < 8; page++) {
        if (oled_line(page, "") != ESP_OK)
            return false;
    }

    printf("OLED: ready at I2C address 0x%02X\n", address);
    return true;
}

static int16_t decode_int16(const uint8_t *bytes)
{
    uint16_t raw = ((uint16_t)bytes[0] << 8) | bytes[1];
    return (int16_t)raw;
}

static const char *main_status(uint8_t flags, uint8_t duty,
                               uint8_t rule_available)
{
    if (flags & FAULT_SENSOR)    return "SENSOR ERROR";
    if (flags & FAULT_STALL)     return "POSSIBLE STALL";
    if (flags & FAULT_OVERLOAD)  return "POSSIBLE OVERLOAD";
    if (flags & FAULT_LOW_VOLT)  return "LOW BATTERY";
    if (flags & FAULT_HIGH_TEMP) return "HIGH TEMP";

    if (duty == 0) return "MOTOR OFF";
    if (!rule_available) return "RULE UNAVAILABLE";

    return "NORMAL";
}

static void print_status(uint8_t flags, uint8_t duty,
                         uint8_t rule_available)
{
    printf("\n============================================================\n");
    printf(" MOTOR STATUS | Duty: %u%%\n", (unsigned)duty);
    printf("------------------------------------------------------------\n");

    if (flags & FAULT_SENSOR)    printf(" ! SENSOR ERROR\n");
    if (flags & FAULT_STALL)     printf(" ! POSSIBLE STALL\n");
    if (flags & FAULT_OVERLOAD)  printf(" ! POSSIBLE OVERLOAD\n");
    if (flags & FAULT_LOW_VOLT)  printf(" ! LOW BATTERY\n");
    if (flags & FAULT_HIGH_TEMP) printf(" ! HIGH TEMPERATURE\n");

    if (flags == 0) {
        printf(" %s\n", main_status(flags, duty, rule_available));
    }

    printf("============================================================\n\n");
}

static void update_oled(float rpm, float current, float voltage,
                        float temperature, bool have_sensor,
                        int duty, int flags, int rule_available,
                        bool sensor_lost, bool status_lost)
{
    if (!oled_ready) return;

    char line[32];
    bool valid_data = have_sensor && !sensor_lost;

    if (valid_data)
        snprintf(line, sizeof(line), "RPM:%.1f", rpm);
    else
        snprintf(line, sizeof(line), "RPM:--");
    oled_big_line(0, line);

    if (valid_data)
        snprintf(line, sizeof(line), "I:%.3fA", current);
    else
        snprintf(line, sizeof(line), "I:--");
    oled_big_line(2, line);

    if (valid_data)
        snprintf(line, sizeof(line), "V:%.2fV", voltage);
    else
        snprintf(line, sizeof(line), "V:--");
    oled_big_line(4, line);

    if (valid_data) {
        snprintf(line, sizeof(line), "T:%.1fC  D:%d%%",
                 temperature, duty < 0 ? 0 : duty);
    } else {
        snprintf(line, sizeof(line), "T:--  D:--");
    }
    oled_line(6, line);

    if (status_lost) {
        oled_line(7, "CAN STATUS LOST");
    } else if (sensor_lost) {
        oled_line(7, "SENSOR DATA LOST");
    } else if (flags < 0) {
        oled_line(7, "WAITING FOR STATUS");
    } else {
        oled_line(7,
                  main_status((uint8_t)flags,
                              (uint8_t)(duty < 0 ? 0 : duty),
                              (uint8_t)(rule_available > 0)));
    }
}

void app_main(void)
{
    oled_ready = oled_init();

    twai_general_config_t general =
        TWAI_GENERAL_CONFIG_DEFAULT(
            GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL
        );
    general.rx_queue_len = 20;

    twai_timing_config_t timing =
        TWAI_TIMING_CONFIG_500KBITS();

    twai_filter_config_t filter =
        TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(
        twai_driver_install(&general, &timing, &filter)
    );
    ESP_ERROR_CHECK(twai_start());

    printf("\n========== MOTOR HEALTH MONITOR ==========\n");
    printf("Waiting for CAN frames...\n");

    float rpm = 0.0f;
    float current = 0.0f;
    float voltage = 0.0f;
    float temperature = 0.0f;
    bool have_sensor = false;

    int flags = -1;
    int duty = -1;
    int rule_available = -1;

    int64_t last_sensor_ms = 0;
    int64_t last_status_ms = 0;
    int64_t last_display_ms = -1000;

    bool sensor_lost_reported = false;
    bool status_lost_reported = false;

    while (1) {
        twai_message_t message;

        esp_err_t result =
            twai_receive(&message, pdMS_TO_TICKS(100));

        int64_t now_ms = esp_timer_get_time() / 1000;

        if (result == ESP_OK && !message.extd && !message.rtr) {
            if (message.identifier == 0x101 &&
                message.data_length_code == 3) {
                int new_flags = message.data[0];
                int new_duty = message.data[1];
                int new_rule = message.data[2];

                last_status_ms = now_ms;

                if (new_flags != flags ||
                    new_duty != duty ||
                    new_rule != rule_available ||
                    status_lost_reported) {
                    print_status((uint8_t)new_flags,
                                 (uint8_t)new_duty,
                                 (uint8_t)new_rule);
                }

                flags = new_flags;
                duty = new_duty;
                rule_available = new_rule;
                status_lost_reported = false;
            } else if (message.identifier == 0x100 &&
                       message.data_length_code == 8) {
                rpm =
                    decode_int16(&message.data[0]) / 10.0f;

                current =
                    decode_int16(&message.data[2]) / 1000.0f;

                voltage =
                    decode_int16(&message.data[4]) / 100.0f;

                temperature =
                    decode_int16(&message.data[6]) / 10.0f;

                have_sensor = true;
                last_sensor_ms = now_ms;
                sensor_lost_reported = false;
            }
        } else if (result != ESP_OK &&
                   result != ESP_ERR_TIMEOUT) {
            printf("CAN receive error: %s\n",
                   esp_err_to_name(result));
        }

        bool sensor_lost =
            !have_sensor ||
            (now_ms - last_sensor_ms > FRAME_TIMEOUT_MS);

        bool status_lost =
            (last_status_ms == 0) ||
            (now_ms - last_status_ms > FRAME_TIMEOUT_MS);

        if (have_sensor &&
            sensor_lost &&
            !sensor_lost_reported) {
            printf("\n! SENSOR DATA LOST: readings are stale\n");
            sensor_lost_reported = true;
        }

        if (last_status_ms != 0 &&
            status_lost &&
            !status_lost_reported) {
            printf("\n! CAN STATUS LOST: warning state is unknown\n");
            status_lost_reported = true;
        }

        if (now_ms - last_display_ms >= 1000) {
            if (have_sensor && !sensor_lost) {
                printf("[%6" PRId64 "s] Duty %3d%% | "
                       "RPM %6.1f | I %.3f A | "
                       "V %.2f V | Temp %.1f C\n",
                       now_ms / 1000,
                       duty,
                       rpm,
                       current,
                       voltage,
                       temperature);
            }

            update_oled(rpm, current, voltage,
                        temperature, have_sensor,
                        duty, flags, rule_available,
                        sensor_lost, status_lost);

            last_display_ms = now_ms;
        }
    }
}