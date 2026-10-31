#include <string.h>

#include "hal.h"
#include "ui.h"

namespace {
lv_obj_t *gStatus;
lv_obj_t *gAddressMenu;
lv_obj_t *gScanResult;
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

void addressEvent(lv_event_t *e)
{
    const uint8_t address = static_cast<uint8_t>(0x20 + lv_dropdown_get_selected(lv_event_get_target(e)));
    const bool connected = hal::pcfSetAddress(address);
    lv_label_set_text_fmt(gScanResult, "%s 0x%02X", connected ? "Connected" : "No PCF read/write at", address);
    hal::soundClick();
}

void tick(lv_timer_t *);

void scanEvent(lv_event_t *)
{
    const uint8_t found = hal::pcfScan();
    if (found == 0) {
        lv_label_set_text(gScanResult, "ไม่พบอุปกรณ์ I2C ในช่วง 0x20-0x27");
    } else {
        uint8_t count = 0;
        char result[72] = "Found:";
        size_t used = strlen(result);
        bool first = true;
        uint8_t firstAddress = 0;
        for (uint8_t i = 0; i < 8; ++i) {
            if (!(found & (1U << i))) continue;
            ++count;
            const uint8_t address = static_cast<uint8_t>(0x20 + i);
            if (first) { firstAddress = address; first = false; }
            used += static_cast<size_t>(snprintf(result + used, sizeof(result) - used, " 0x%02X", address));
        }
        if (count == 1) {
            lv_dropdown_set_selected(gAddressMenu, firstAddress - 0x20);
            const bool connected = hal::pcfSetAddress(firstAddress);
            char report[112];
            snprintf(report, sizeof(report), "%s 0x%02X", connected ? "Connected" : "Read/write failed", firstAddress);
            lv_label_set_text(gScanResult, report);
        } else {
            char report[112];
            snprintf(report, sizeof(report), "Multiple I2C devices; select address. %s", result);
            lv_label_set_text(gScanResult, report);
        }
    }
    tick(nullptr);
    hal::soundClick();
}

void tick(lv_timer_t *)
{
    hal::pcfPoll(millis());
    lv_label_set_text_fmt(gStatus, "%s   I2C 0x%02X   SDA 27 / SCL 22",
                          hal::pcfReady() ? "ONLINE" : "OFFLINE", hal::pcfAddress());
    lv_obj_set_style_text_color(gStatus, hal::pcfReady() ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED), 0);
    const uint8_t inputs = hal::pcfInputMask();
    for (uint8_t i = 0; i < 4; ++i) {
        const bool pressed = (inputs & (1U << (i + 4))) == 0;
        lv_label_set_text_fmt(gInputs[i], "P%u   %s", i + 4, pressed ? "กด LOW" : "ปล่อย HIGH");
        lv_obj_set_style_text_color(gInputs[i], pressed ? lv_palette_main(LV_PALETTE_AMBER) : lv_palette_main(LV_PALETTE_GREY), 0);
    }
}
}

void ui_pcf_tab(lv_obj_t *tab)
{
    lv_obj_t *card = ui_card(tab, "PCF8574 I/O");
    lv_obj_set_style_pad_all(card, 5, 0);
    lv_obj_set_style_pad_row(card, 4, 0);
    ui_label(card, "A0-A2 เลือก 0x20-0x27  |  SDA 27 / SCL 22", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_obj_t *addrRow = ui_row(card);
    ui_label(addrRow, "Address");
    gAddressMenu = lv_dropdown_create(addrRow);
    lv_dropdown_set_options(gAddressMenu, "0x20  (000)\n0x21  (001)\n0x22  (010)\n0x23  (011)\n0x24  (100)\n0x25  (101)\n0x26  (110)\n0x27  (111)");
    lv_dropdown_set_selected(gAddressMenu, hal::pcfAddress() - 0x20);
    lv_obj_set_width(gAddressMenu, 124);
    lv_obj_add_event_cb(gAddressMenu, addressEvent, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t *scanButton = ui_btn(addrRow, "Scan\nAddress", scanEvent);
    lv_obj_set_size(scanButton, 78, 44);
    gScanResult = ui_label(card, "Scan เพื่อแสดง address ที่เชื่อมต่อ", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
    gStatus = ui_label(card, "กำลังเชื่อมต่อ...", &font_th_16, lv_palette_main(LV_PALETTE_CYAN));

    card = ui_card(tab, "เอาต์พุต LED Isink");
    lv_obj_set_style_pad_all(card, 5, 0);
    lv_obj_set_style_pad_row(card, 4, 0);
    ui_label(card, "P0-P3  ON = LOW", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_obj_t *row = ui_row(card);
    lv_obj_set_style_pad_column(row, 4, 0);
    for (uint8_t i = 0; i < 4; ++i) {
        char label[16]; snprintf(label, sizeof(label), "LED%u\nP%u", i, i);
        gLeds[i] = ui_btn(row, label, ledEvent, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_set_size(gLeds[i], 64, 42);
        lv_obj_set_style_bg_color(gLeds[i], lv_palette_main(LV_PALETTE_GREY), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(gLeds[i], lv_palette_main(LV_PALETTE_AMBER), LV_STATE_CHECKED);
        lv_obj_set_style_transform_zoom(gLeds[i], 270, LV_STATE_PRESSED);
    }
    row = ui_row(card);
    ui_btn(row, "เปิดทั้งหมด", allOn);
    ui_btn(row, "ปิดทั้งหมด", allOff);
    ui_btn(row, "ไฟวิ่ง", demoEvent);

    card = ui_card(tab, "อินพุต Active Low");
    lv_obj_set_style_pad_all(card, 5, 0);
    lv_obj_set_style_pad_row(card, 4, 0);
    ui_label(card, "P4-P7  Pull-up 10k", nullptr, lv_palette_lighten(LV_PALETTE_GREY, 2));
    for (uint8_t i = 0; i < 4; ++i) gInputs[i] = ui_label(card, "กำลังอ่าน...", &font_th_16);
    lv_timer_create(tick, 50, nullptr);
    tick(nullptr);
}
