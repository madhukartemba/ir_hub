#ifndef HAPTICS_H
#define HAPTICS_H

#include <Arduino.h>
#include <Wire.h>
#include "../Log/Log.h"

// TI DRV2605 — shared I2C with OLED (Wire must be started before begin()).
class Haptics {
   private:
    static constexpr uint8_t kDefaultAddr = 0x5A;

    // Registers
    static constexpr uint8_t REG_STATUS = 0x00;
    static constexpr uint8_t REG_MODE = 0x01;
    static constexpr uint8_t REG_RTPIN = 0x02;
    static constexpr uint8_t REG_LIBRARY = 0x03;
    static constexpr uint8_t REG_WAVESEQ1 = 0x04;
    static constexpr uint8_t REG_WAVESEQ2 = 0x05;
    static constexpr uint8_t REG_GO = 0x0C;
    static constexpr uint8_t REG_RATED_V = 0x16;
    static constexpr uint8_t REG_OD_CLAMP = 0x17;
    static constexpr uint8_t REG_FEEDBACK = 0x1A;
    static constexpr uint8_t REG_CONTROL3 = 0x1D;

    static constexpr uint8_t MODE_INTTRIG = 0x00;
    static constexpr uint8_t MODE_CALIB = 0x07;

    static constexpr uint8_t LIB_LRA = 0x06;

    // Library 6 (LRA) ROM effects — short, crisp taps
    static constexpr uint8_t EFFECT_STRONG_CLICK_100 = 1;
    static constexpr uint8_t EFFECT_STRONG_CLICK_60 = 2;
    static constexpr uint8_t EFFECT_SHARP_TICK = 4;
    static constexpr uint8_t EFFECT_SHARP_TICK_2 = 5;

    uint8_t addr;
    // present  = chip ACKs on the bus (cheap probe)
    // initialized = also auto-calibrated and in INTTRIG mode
    // The split lets us skip the calibration buzz at boot when haptics are muted.
    bool present = false;
    bool initialized = false;
    bool muted = false;

    /** True if a device ACKs this 7-bit address (same probe as an I2C scan). */
    static bool i2cAddressAck(uint8_t address7) {
        Wire.beginTransmission(address7);
        return Wire.endTransmission() == 0;
    }

    bool writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(addr);
        Wire.write(reg);
        Wire.write(val);
        return Wire.endTransmission() == 0;
    }

    bool readReg(uint8_t reg, uint8_t* out) {
        Wire.beginTransmission(addr);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) {
            return false;
        }
        if (Wire.requestFrom(static_cast<int>(addr), 1) != 1) {
            return false;
        }
        if (!Wire.available()) {
            return false;
        }
        *out = static_cast<uint8_t>(Wire.read());
        return true;
    }

    void triggerEffect(uint8_t effect) {
        writeReg(REG_WAVESEQ1, effect);
        writeReg(REG_WAVESEQ2, 0);
        writeReg(REG_GO, 1);
    }

    bool waitGoClear(unsigned long timeoutMs) {
        unsigned long start = millis();
        for (;;) {
            uint8_t go = 0;
            if (!readReg(REG_GO, &go)) {
                return false;
            }
            if ((go & 0x01) == 0) {
                return true;
            }
            if (millis() - start > timeoutMs) {
                return false;
            }
            yield();
        }
    }

   public:
    explicit Haptics(uint8_t i2cAddr = kDefaultAddr) : addr(i2cAddr) {}

    /// Bus probe only, no motor drive. Safe before LittleFS is mounted.
    bool probe() {
        present = i2cAddressAck(addr);
        return present;
    }

    bool isPresent() const { return present; }

    bool begin() {
        initialized = false;
        // Wire.begin() expected to have been called (e.g. by display).
        if (!i2cAddressAck(addr)) {
            present = false;
            return false;
        }
        present = true;
        if (!writeReg(REG_FEEDBACK, 0x80)) {
            return false;
        }
        if (!writeReg(REG_LIBRARY, LIB_LRA) || !writeReg(REG_CONTROL3, 0xA3) || !writeReg(REG_RATED_V, 0x50) ||
            !writeReg(REG_OD_CLAMP, 0x89)) {
            return false;
        }

        if (!writeReg(REG_MODE, MODE_CALIB) || !writeReg(REG_GO, 1)) {
            return false;
        }
        if (!waitGoClear(2000)) {
            LOG_ERROR("[Haptics] Calibration timeout or bus error");
            return false;
        }

        uint8_t status = 0;
        if (!readReg(REG_STATUS, &status)) {
            return false;
        }
        if (status & 0x08) {
            LOG_ERROR("[Haptics] Calibration failed (status 0x%02X)", status);
            return false;
        }

        if (!writeReg(REG_MODE, MODE_INTTRIG)) {
            return false;
        }
        initialized = true;
        LOG_DEBUG("[Haptics] DRV2605 ready at 0x%02X", addr);
        return true;
    }

    bool isReady() const { return initialized; }

    void setMuted(bool m) { muted = m; }
    bool isMuted() const { return muted; }

    /** Raw ROM effect (library 6 / LRA). */
    void playEffect(uint8_t effectId) {
        if (!initialized || muted) {
            return;
        }
        triggerEffect(effectId);
    }

    /** ~UIImpactFeedbackStyle.light — quick, subtle pop on touch-down. */
    void playLightImpact() {
        playEffect(EFFECT_SHARP_TICK);
    }

    /** ~UIImpactFeedbackStyle.medium — slightly stronger confirmation. */
    void playMediumImpact() {
        playEffect(EFFECT_STRONG_CLICK_60);
    }

    /** Down-stroke of a simulated mechanical click (press). */
    void playButtonPress() { playEffect(EFFECT_SHARP_TICK); }

    /** Up-stroke of a simulated mechanical click (release). Slightly different ROM slot so down/up feel distinct. */
    void playButtonRelease() { playEffect(EFFECT_SHARP_TICK_2); }

    /** Stronger tap — e.g. long-ack (paired with long-press action). */
    void playLongPressAck() {
        if (!initialized) {
            return;
        }
        playEffect(EFFECT_STRONG_CLICK_100);
        delay(40);
        playEffect(EFFECT_SHARP_TICK);
    }

    /** ~UISelectionFeedbackGenerator — light double tick. */
    void playSelection() {
        if (!initialized) {
            return;
        }
        playEffect(EFFECT_SHARP_TICK);
        delay(35);
        playEffect(EFFECT_SHARP_TICK);
    }
};

#endif  // HAPTICS_H
