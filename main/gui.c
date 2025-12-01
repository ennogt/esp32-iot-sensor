#include "gui.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <sys/lock.h>
#include <unistd.h>

// ================= CONFIGURATION =================
#define I2C_BUS_PORT 0
#define PIN_NUM_SDA 20
#define PIN_NUM_SCL 19
#define PIN_NUM_RST -1
#define I2C_HW_ADDR 0x3C

// LCD Settings
#define LCD_H_RES 128
#define LCD_V_RES 32
#define LCD_PIXEL_CLOCK_HZ (400 * 1000)

// LVGL Settings
#define LVGL_TICK_PERIOD_MS 5
#define LVGL_TASK_STACK_SIZE (4 * 1024)
#define LVGL_TASK_PRIORITY 2

// Global variables for UI widgets
static lv_obj_t *label_status = NULL;
static lv_obj_t *label_temp = NULL;

// Mutex for thread safety
static _lock_t lvgl_api_lock;

// Buffer for monochrome conversion
static uint8_t oled_buffer[LCD_H_RES * LCD_V_RES / 8];

static const char *TAG = "GUI";

LV_FONT_DECLARE(lv_font_montserrat_10);
LV_FONT_DECLARE(lv_font_montserrat_14);

// ---------------- INTERNAL HELPER FUNCTIONS ----------------

// Callback when the display has finished drawing
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

// Convert LVGL pixel data to SSD1306 format and send to display
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    // Skip palette (LVGL specific for I1 format)
    px_map += 8; // EXAMPLE_LVGL_PALETTE_SIZE (8 bytes)

    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    for (int y = y1; y <= y2; y++)
    {
        for (int x = x1; x <= x2; x++)
        {
            bool chroma_color = (px_map[(hor_res >> 3) * y + (x >> 3)] & 1 << (7 - x % 8));
            uint8_t *buf = oled_buffer + hor_res * (y >> 3) + (x);
            if (chroma_color)
            {
                (*buf) &= ~(1 << (y % 8));
            }
            else
            {
                (*buf) |= (1 << (y % 8));
            }
        }
    }
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, oled_buffer);
}

// LVGL Tick Increase Timer Callback
static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// Task to handle LVGL updates
static void lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1)
    {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);

        if (time_till_next_ms > 500)
            time_till_next_ms = 500;
        if (time_till_next_ms < 5)
            time_till_next_ms = 5;

        usleep(1000 * time_till_next_ms);
    }
}

static void setup_ui(void)
{
    lv_obj_t *scr = lv_screen_active();

    // Status label (top)
    label_status = lv_label_create(scr);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_10, 0);
    lv_label_set_text(label_status, "Booting...");
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 0);

    // Temperature and Humidity label (center)
    label_temp = lv_label_create(scr);
    lv_label_set_text(label_temp, "--.-°C --%");
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, 8);
}

// ---------------- PUBLIC FUNCTIONS ----------------

void gui_init(void)
{
    ESP_LOGI(TAG, "Init I2C Bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = PIN_NUM_SDA,
        .scl_io_num = PIN_NUM_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Install Panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = I2C_HW_ADDR,
        .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6, // SSD1306 specific
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 Driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = PIN_NUM_RST,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Init LVGL");
    lv_init();
    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_user_data(display, panel_handle);

    // Buffer setup
    size_t draw_buffer_sz = LCD_H_RES * LCD_V_RES / 8 + 8; // + palette size
    void *buf = heap_caps_calloc(1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display, buf, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    // Register flush ready callback
    const esp_lcd_panel_io_callbacks_t cbs = {.on_color_trans_done = notify_lvgl_flush_ready};
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display);

    // Timer setup
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    // Create UI
    _lock_acquire(&lvgl_api_lock);
    setup_ui();
    _lock_release(&lvgl_api_lock);

    // Start task
    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
}

void gui_set_values(float temperature, float humidity)
{
    _lock_acquire(&lvgl_api_lock);
    if (label_temp)
    {
        // Format temperature and humidity
        int t_int = (int)temperature;
        int t_dec = (int)((temperature - t_int) * 10);
        if (t_dec < 0)
            t_dec = -t_dec;

        int h_int = (int)humidity;
        int h_dec = (int)((humidity - h_int) * 10);
        if (h_dec < 0)
            h_dec = -h_dec;

        // lv_label_set_text_fmt(label_temp, "%d.%d°C  %d%%", t_int, t_dec, h_int);
        lv_label_set_text_fmt(label_temp, "%d.%d°C %d.%d%%", t_int, t_dec, h_int, h_dec);
    }
    _lock_release(&lvgl_api_lock);
}

void gui_set_status(const char *status_text)
{
    _lock_acquire(&lvgl_api_lock);
    if (label_status)
    {
        lv_label_set_text(label_status, status_text);
        // Center align again
        lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 0);
    }
    _lock_release(&lvgl_api_lock);
}
