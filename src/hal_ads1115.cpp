#include <Wire.h>

#include "hal.h"

namespace hal {
namespace {
constexpr uint8_t DEFAULT_ADDRESS = 0x48; // ADDR -> GND
constexpr uint16_t CONFIG_BASE = 0x8000 | 0x0200 | 0x0100 | 0x0080 | 0x0003; // single shot, +/-4.096V, 128 SPS
constexpr float LSB_VOLTS = 4.096f / 32768.0f;
uint8_t gAddress = DEFAULT_ADDRESS;
int16_t gRaw[4] = {};
bool gValid[4] = {};
bool gReady = false;
bool gConverting = false;
uint8_t gChannel = 0;
uint32_t gConversionStarted = 0;
uint32_t gLastStartAttempt = 0;

bool readRegister(uint8_t address, uint8_t reg, uint16_t &value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission() != 0 || Wire.requestFrom(address, static_cast<uint8_t>(2)) != 2) return false;
    value = static_cast<uint16_t>(Wire.read()) << 8;
    value |= static_cast<uint8_t>(Wire.read());
    return true;
}

bool startConversion(uint8_t channel, uint32_t now)
{
    const uint16_t config = static_cast<uint16_t>(CONFIG_BASE | (0x4000U + (channel << 12)));
    Wire.beginTransmission(gAddress);
    Wire.write(static_cast<uint8_t>(0x01));
    Wire.write(static_cast<uint8_t>(config >> 8));
    Wire.write(static_cast<uint8_t>(config));
    if (Wire.endTransmission() != 0) {
        gReady = false;
        gConverting = false;
        gLastStartAttempt = now;
        return false;
    }
    gConverting = true;
    gConversionStarted = now;
    gLastStartAttempt = now;
    return true;
}
}

void adsBegin()
{
    const uint8_t saved = prefs().getUChar("ads_addr", DEFAULT_ADDRESS);
    gAddress = saved >= 0x48 && saved <= 0x4B ? saved : DEFAULT_ADDRESS;
    gReady = false;
    gConverting = false;
    gChannel = 0;
    for (uint8_t i = 0; i < 4; ++i) gValid[i] = false;
}

void adsPoll(uint32_t now)
{
    if (!gConverting) {
        if (now - gLastStartAttempt >= 100) startConversion(gChannel, now);
        return;
    }
    if (now - gConversionStarted < 10) return; // 128 SPS conversion time is about 7.8 ms

    uint16_t value;
    if (!readRegister(gAddress, 0x00, value)) {
        gReady = false;
        gConverting = false;
        gLastStartAttempt = now;
        return;
    }
    gRaw[gChannel] = static_cast<int16_t>(value);
    gValid[gChannel] = true;
    gReady = true;
    gChannel = static_cast<uint8_t>((gChannel + 1) & 0x03);
    gConverting = false;
}

bool adsSetAddress(uint8_t address, bool save)
{
    if (address < 0x48 || address > 0x4B) return false;
    gAddress = address;
    gConverting = false;
    gChannel = 0;
    for (uint8_t i = 0; i < 4; ++i) gValid[i] = false;
    if (save) prefs().putUChar("ads_addr", gAddress);
    uint16_t config;
    gReady = readRegister(gAddress, 0x01, config);
    gLastStartAttempt = 0;
    return gReady;
}

uint8_t adsAddress() { return gAddress; }

uint8_t adsScan()
{
    uint8_t found = 0;
    for (uint8_t address = 0x48; address <= 0x4B; ++address) {
        uint16_t config;
        if (readRegister(address, 0x01, config)) found |= static_cast<uint8_t>(1U << (address - 0x48));
    }
    return found;
}

bool adsReady() { return gReady; }
int16_t adsRaw(uint8_t channel) { return channel < 4 ? gRaw[channel] : 0; }
float adsVoltage(uint8_t channel) { return channel < 4 ? gRaw[channel] * LSB_VOLTS : 0.0f; }
bool adsChannelValid(uint8_t channel) { return channel < 4 && gValid[channel]; }
}
