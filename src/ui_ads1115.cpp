#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal.h"
#include "ui.h"

namespace {
lv_obj_t *gStatus;
lv_obj_t *gResult;
lv_obj_t *gAddressMenu;
lv_obj_t *gValues[4];
lv_obj_t *gRawValues[4];
lv_obj_t *gGauges[4];
lv_obj_t *gGaugeCenter[4];
float gSmoothMv[4] = {};
bool gSmoothValid[4] = {};

void gaugeAnimExec(void *obj, int32_t value)
{
    lv_arc_set_value(static_cast<lv_obj_t *>(obj), static_cast<int16_t>(value));
}

void addressEvent(lv_event_t *e)
{
    const uint8_t address = static_cast<uint8_t>(0x48 + lv_dropdown_get_selected(lv_event_get_target(e)));
    const bool ready = hal::adsSetAddress(address);
    lv_label_set_text_fmt(gResult, "%s 0x%02X", ready ? "Connected" : "No ADS1115 at", address);
    hal::soundClick();
}

void scanEvent(lv_event_t *)
{
    const uint8_t found = hal::adsScan();
    if (!found) {
        lv_label_set_text(gResult, "No response at 0x48-0x4B");
    } else {
        uint8_t count = 0;
        uint8_t first = 0;
        char list[56] = "Found:";
        size_t used = strlen(list);
        for (uint8_t i = 0; i < 4; ++i) {
            if (!(found & (1U << i))) continue;
            if (count++ == 0) first = static_cast<uint8_t>(0x48 + i);
            used += static_cast<size_t>(snprintf(list + used, sizeof(list) - used, " 0x%02X", 0x48 + i));
        }
        if (count == 1) {
            lv_dropdown_set_selected(gAddressMenu, first - 0x48);
            const bool connected = hal::adsSetAddress(first);
            lv_label_set_text_fmt(gResult, "%s 0x%02X", connected ? "Connected" : "Read failed at", first);
        } else {
            lv_label_set_text_fmt(gResult, "Multiple devices; select address. %s", list);
        }
    }
    hal::soundClick();
}

void tick(lv_timer_t *)
{
    const uint32_t now = millis();
    hal::adsPoll(now);
    static uint32_t lastUi = 0;
    if (now - lastUi < 100) return;
    lastUi = now;
    lv_label_set_text_fmt(gStatus, "%s  I2C 0x%02X", hal::adsReady() ? "ONLINE" : "OFFLINE", hal::adsAddress());
    lv_obj_set_style_text_color(gStatus, hal::adsReady() ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED), 0);
    for (uint8_t i = 0; i < 4; ++i) {
        if (!hal::adsChannelValid(i)) continue;
        const float volts = hal::adsVoltage(i);
        const float measuredMv = volts * 1000.0f;
        if (!gSmoothValid[i]) {
            gSmoothMv[i] = measuredMv;
            gSmoothValid[i] = true;
        } else {
            gSmoothMv[i] += (measuredMv - gSmoothMv[i]) * 0.35f;
        }
        const int millivolts = static_cast<int>(gSmoothMv[i] + (gSmoothMv[i] >= 0 ? 0.5f : -0.5f));
        lv_label_set_text_fmt(gValues[i], "%d.%03d V", millivolts / 1000, abs(millivolts % 1000));
        lv_label_set_text_fmt(gRawValues[i], "RAW %d", hal::adsRaw(i));
        const int gaugeValue = constrain(millivolts, 0, 3300);
        lv_label_set_text_fmt(gGaugeCenter[i], "%d%%", gaugeValue * 100 / 3300);
        if (lv_arc_get_value(gGauges[i]) != gaugeValue) {
            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, gGauges[i]);
            lv_anim_set_values(&anim, lv_arc_get_value(gGauges[i]), gaugeValue);
            lv_anim_set_time(&anim, 120);
            lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
            lv_anim_set_exec_cb(&anim, gaugeAnimExec);
            lv_anim_start(&anim);
        }
        const lv_palette_t levelColor = gaugeValue < 1100 ? LV_PALETTE_CYAN :
                                        gaugeValue < 2200 ? LV_PALETTE_GREEN :
                                        gaugeValue < 3000 ? LV_PALETTE_AMBER : LV_PALETTE_RED;
        lv_obj_set_style_arc_color(gGauges[i], lv_palette_main(levelColor), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(gValues[i], lv_palette_main(levelColor), 0);
        lv_obj_set_style_text_color(gGaugeCenter[i], lv_palette_main(levelColor), 0);
    }
}
}

void ui_ads_tab(lv_obj_t *tab)
{
    lv_obj_t *row = ui_row(tab);
    ui_label(row, "ADDR");
    gAddressMenu = lv_dropdown_create(row);
    lv_dropdown_set_options(gAddressMenu, "0x48 GND\n0x49 VDD\n0x4A SDA\n0x4B SCL");
    lv_dropdown_set_selected(gAddressMenu, hal::adsAddress() - 0x48);
    lv_obj_set_width(gAddressMenu, 118);
    lv_obj_add_event_cb(gAddressMenu, addressEvent, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t *scanButton = ui_btn(row, "Scan Address", scanEvent);
    lv_obj_set_size(scanButton, 126, 44);
    gStatus = ui_label(tab, "Waiting for ADC...", &font_th_16, lv_palette_main(LV_PALETTE_CYAN));
    gResult = ui_label(tab, "", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));

    for (uint8_t r = 0; r < 2; ++r) {
        row = ui_row(tab);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_set_style_pad_row(row, 4, 0);
        for (uint8_t col = 0; col < 2; ++col) {
            const uint8_t i = static_cast<uint8_t>(r * 2 + col);
            lv_obj_t *card = lv_obj_create(row);
            lv_obj_set_size(card, 144, 78);
            lv_obj_set_style_pad_all(card, 5, 0);
            lv_obj_set_style_pad_column(card, 5, 0);
            lv_obj_set_style_radius(card, 9, 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_border_color(card, lv_palette_main(LV_PALETTE_CYAN), 0);
            lv_obj_set_style_bg_color(card, lv_color_hex(0x172638), 0);
            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

            gGauges[i] = lv_arc_create(card);
            lv_obj_set_size(gGauges[i], 62, 62);
            lv_arc_set_bg_angles(gGauges[i], 135, 45);
            lv_arc_set_range(gGauges[i], 0, 3300);
            lv_arc_set_value(gGauges[i], 0);
            lv_obj_set_style_arc_width(gGauges[i], 6, LV_PART_MAIN);
            lv_obj_set_style_arc_width(gGauges[i], 6, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(gGauges[i], lv_color_hex(0x33445A), LV_PART_MAIN);
            lv_obj_set_style_arc_color(gGauges[i], lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
            lv_obj_set_style_bg_opa(gGauges[i], LV_OPA_TRANSP, LV_PART_KNOB);
            lv_obj_set_style_border_opa(gGauges[i], LV_OPA_TRANSP, LV_PART_KNOB);
            lv_obj_clear_flag(gGauges[i], LV_OBJ_FLAG_CLICKABLE);
            gGaugeCenter[i] = lv_label_create(gGauges[i]);
            lv_label_set_text(gGaugeCenter[i], "0%");
            lv_obj_set_style_text_font(gGaugeCenter[i], &font_th_16, 0);
            lv_obj_set_style_text_color(gGaugeCenter[i], lv_palette_main(LV_PALETTE_CYAN), 0);
            lv_obj_center(gGaugeCenter[i]);

            lv_obj_t *info = lv_obj_create(card);
            lv_obj_remove_style_all(info);
            lv_obj_set_size(info, 64, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(info, 0, 0);
            lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
            char title[4];
            snprintf(title, sizeof(title), "A%u", i);
            ui_label(info, title, &font_th_16, lv_palette_main(LV_PALETTE_CYAN));
            gValues[i] = ui_label(info, "--.--- V", &font_th_16);
            gRawValues[i] = ui_label(info, "RAW --", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
        }
    }
    ui_label(tab, "Single-ended. Input range GND to VDD (3.3V).", nullptr,
             lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_timer_create(tick, 5, nullptr);
    tick(nullptr);
}
