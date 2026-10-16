#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <lvgl.h>
#include <zmk/display.h>

// Drawn on LVGL's top layer so it sits above the dongle_screen status screen
// without forking the module. Position matches the left column, BLE row.
#define UPTIME_X 20
#define UPTIME_Y 66

static lv_obj_t *uptime_label;
static int64_t last_shown_s = -1;

static void uptime_update(lv_timer_t *timer) {
    int64_t s = k_uptime_get() / 1000;
    if (s == last_shown_s) {
        return;
    }
    last_shown_s = s;

    unsigned int days = s / 86400;
    unsigned int h = (s / 3600) % 24;
    unsigned int m = (s / 60) % 60;
    unsigned int sec = s % 60;

    if (days > 0) {
        lv_label_set_text_fmt(uptime_label, "%ud %02u:%02u:%02u", days, h, m, sec);
    } else {
        lv_label_set_text_fmt(uptime_label, "%02u:%02u:%02u", h, m, sec);
    }
}

// Runs on the display work queue, which owns LVGL.
static void uptime_create(struct k_work *work) {
    uptime_label = lv_label_create(lv_layer_top());
    // The top layer doesn't inherit the status screen's white text style.
    lv_obj_set_style_text_color(uptime_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(uptime_label, LV_ALIGN_TOP_LEFT, UPTIME_X, UPTIME_Y);
    uptime_update(NULL);
    // Poll faster than 1 Hz so timer jitter never skips or repeats a second.
    lv_timer_create(uptime_update, 250, NULL);
}

static K_WORK_DEFINE(uptime_create_work, uptime_create);

// The display work queue is started from main(), after SYS_INIT, so wait for it.
static void uptime_wait_for_display(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(uptime_wait_work, uptime_wait_for_display);

static void uptime_wait_for_display(struct k_work *work) {
    if (!zmk_display_is_initialized()) {
        k_work_schedule(&uptime_wait_work, K_MSEC(100));
        return;
    }
    k_work_submit_to_queue(zmk_display_work_q(), &uptime_create_work);
}

static int uptime_init(void) {
    k_work_schedule(&uptime_wait_work, K_MSEC(100));
    return 0;
}

SYS_INIT(uptime_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
