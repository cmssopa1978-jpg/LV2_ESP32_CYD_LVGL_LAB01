#include "hal.h"
#include "ui.h"

namespace {
lv_obj_t *gStatus;
lv_obj_t *gInputs[4];
lv_obj_t *gLeds[4];
bool gDemoOn = false;
lv_timer_t *gDemoTimer = nullptr;

void ledEvent(lv_event_t *e)
{
    const uint8_t i = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    const bool on = (hal::pcfLedMask() & (1U << i)) == 0;
    if (!hal::pcfSetLed(i, on)) return;
    if (on) lv_obj_add_state(gLeds[i], LV_STATE_CHECKED);
    else lv_obj_clear_state(gLeds[i], LV_STATE_CHECKED);
    hal::soundClick();
}

void allOn(lv_event_t *)
{
    if (gDemoTimer) { lv_timer_del(gDemoTimer); gDemoTimer = nullptr; gDemoOn = false; }
    hal::pcfSetAllLeds(0x0F);
    for (auto *b : gLeds) lv_obj_add_state(b, LV_STATE_CHECKED);
    hal::soundClick();
}

void allOff(lv_event_t *)
{
    if (gDemoTimer) { lv_timer_del(gDemoTimer); gDemoTimer = nullptr; gDemoOn = false; }
    hal::pcfSetAllLeds(0);
    for (auto *b : gLeds) lv_obj_clear_state(b, LV_STATE_CHECKED);
    hal::soundClick();
}

void demo(lv_timer_t *)
{
    gDemoOn = !gDemoOn;
    hal::pcfSetAllLeds(gDemoOn ? static_cast<uint8_t>(1U << ((millis() / 500) % 4)) : 0);
    for (uint8_t i = 0; i < 4; ++i) {
        if (hal::pcfLedMask() & (1U << i)) lv_obj_add_state(gLeds[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(gLeds[i], LV_STATE_CHECKED);
    }
}

void demoEvent(lv_event_t *)
{
    if (gDemoOn) { lv_timer_del(gDemoTimer); gDemoTimer = nullptr; gDemoOn = false; hal::pcfSetAllLeds(0); }
    else { gDemoOn = true; gDemoTimer = lv_timer_create(demo, 350, nullptr); }
    hal::soundClick();
}

void tick(lv_timer_t *)
{
    hal::pcfPoll(millis());
    lv_label_set_text_fmt(gStatus, "%s   I²C 0x%02X   SDA 27 / SCL 22",
                          hal::pcfReady() ? "● ONLINE" : "● OFFLINE", hal::pcfAddress());
    lv_obj_set_style_text_color(gStatus, hal::pcfReady() ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED), 0);
    const uint8_t inputs = hal::pcfInputMask();
    for (uint8_t i = 0; i < 4; ++i) {
        const bool pressed = (inputs & (1U << (i + 4))) == 0;
        lv_label_set_text_fmt(gInputs[i], "P%u   %s", i + 4, pressed ? "กดอยู่  •  LOW" : "ปล่อย  •  HIGH");
        lv_obj_set_style_text_color(gInputs[i], pressed ? lv_palette_main(LV_PALETTE_AMBER) : lv_palette_main(LV_PALETTE_GREY), 0);
    }
}
}

void ui_pcf_tab(lv_obj_t *tab)
{
    lv_obj_t *card = ui_card(tab, "PCF8574  •  I/O EXPANDER");
    ui_label(card, "A0 A1 A2 = 0  |  Address 0x20  |  I²C 100 kHz", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
    gStatus = ui_label(card, "กำลังเชื่อมต่อ...", &font_th_16, lv_palette_main(LV_PALETTE_CYAN));

    card = ui_card(tab, "เอาต์พุต  •  P0–P3 / LED Isink");
    ui_label(card, "แตะเพื่อสลับไฟ  •  ON = ขา LOW (sink current)", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_obj_t *row = ui_row(card);
    for (uint8_t i = 0; i < 4; ++i) {
        char label[16]; snprintf(label, sizeof(label), "LED%u  P%u", i, i);
        gLeds[i] = ui_btn(row, label, ledEvent, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_set_size(gLeds[i], 68, 48);
        lv_obj_set_style_bg_color(gLeds[i], lv_palette_main(LV_PALETTE_GREY), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(gLeds[i], lv_palette_main(LV_PALETTE_AMBER), LV_STATE_CHECKED);
        lv_obj_set_style_transform_zoom(gLeds[i], 270, LV_STATE_PRESSED);
        lv_obj_set_style_transition_time(gLeds[i], 140, LV_STATE_DEFAULT);
    }
    row = ui_row(card);
    ui_btn(row, "เปิดทั้งหมด", allOn);
    ui_btn(row, "ปิดทั้งหมด", allOff);
    ui_btn(row, "ไฟวิ่ง  ▶", demoEvent);

    card = ui_card(tab, "อินพุต  •  P4–P7 / Active Low");
    ui_label(card, "Pull-up 10 kΩ  •  กดสวิตช์แล้วอ่าน LOW", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
    for (uint8_t i = 0; i < 4; ++i) gInputs[i] = ui_label(card, "กำลังอ่าน...", &font_th_16);
    lv_timer_create(tick, 50, nullptr);
    tick(nullptr);
}
