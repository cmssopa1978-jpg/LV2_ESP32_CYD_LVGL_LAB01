#include <Wire.h>

#include "board_config.h"
#include "hal.h"

namespace hal {
namespace {
constexpr uint8_t DEFAULT_ADDRESS = 0x20; // A0=A1=A2=LOW
constexpr uint8_t INPUT_BITS = 0xF0;
uint8_t gAddress = DEFAULT_ADDRESS;
uint8_t gLedMask = 0;
uint8_t gInputMask = 0xF0;
uint32_t gLastPoll = 0;
bool gReady = false;

bool writePort()
{
    // PCF8574 outputs are quasi-bidirectional. Write 1 to release P4-P7.
    Wire.beginTransmission(gAddress);
    Wire.write(static_cast<uint8_t>(INPUT_BITS | (static_cast<uint8_t>(~gLedMask) & 0x0F)));
    return Wire.endTransmission() == 0;
}
}

void pcfBegin()
{
    Wire.begin(cyd::I2C_PIN_SDA, cyd::I2C_PIN_SCL);
    Wire.setClock(100000);
    const uint8_t savedAddress = prefs().getUChar("pcf_addr", DEFAULT_ADDRESS);
    gAddress = savedAddress >= 0x20 && savedAddress <= 0x27 ? savedAddress : DEFAULT_ADDRESS;
    gLedMask = 0;
    gReady = writePort();
    if (gReady) pcfReadInputs(gInputMask);
}

void pcfPoll(uint32_t now)
{
    if (now - gLastPoll < 40) return;
    gLastPoll = now;
    uint8_t value;
    if (!gReady) gReady = writePort();
    if (gReady && pcfReadInputs(value)) gInputMask = value;
    else gReady = false;
}

bool pcfReadInputs(uint8_t &mask)
{
    if (!writePort() || Wire.requestFrom(gAddress, static_cast<uint8_t>(1)) != 1) {
        gReady = false;
        return false;
    }
    mask = static_cast<uint8_t>(Wire.read()) & INPUT_BITS;
    gReady = true;
    return true;
}

bool pcfSetLed(uint8_t index, bool on)
{
    if (index > 3) return false;
    const uint8_t bit = static_cast<uint8_t>(1U << index);
    gLedMask = on ? static_cast<uint8_t>(gLedMask | bit) : static_cast<uint8_t>(gLedMask & ~bit);
    gReady = writePort();
    return gReady;
}

bool pcfSetAllLeds(uint8_t mask)
{
    gLedMask = mask & 0x0F;
    gReady = writePort();
    return gReady;
}

bool pcfReady() { return gReady; }
uint8_t pcfLedMask() { return gLedMask; }
uint8_t pcfInputMask() { return gInputMask; }
uint8_t pcfAddress() { return gAddress; }

bool pcfSetAddress(uint8_t address, bool save)
{
    if (address < 0x20 || address > 0x27) return false;
    gAddress = address;
    if (save) prefs().putUChar("pcf_addr", gAddress);
    if (!writePort()) {
        gReady = false;
        return false;
    }
    return pcfReadInputs(gInputMask);
}

uint8_t pcfScan()
{
    uint8_t found = 0;
    for (uint8_t address = 0x20; address <= 0x27; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() != 0) continue;
        // PCF8574 responds to a direct one-byte port read without a register pointer.
        if (Wire.requestFrom(address, static_cast<uint8_t>(1)) == 1) {
            Wire.read();
            found |= static_cast<uint8_t>(1U << (address - 0x20));
        }
    }
    return found;
}
}
