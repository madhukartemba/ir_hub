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

    uint8_t addr;
    bool initialized = false;

    void writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(addr);
        Wire.write(reg);
        Wire.write(val);
        Wire.endTransmission();
    }

    uint8_t readReg(uint8_t reg) {
        Wire.beginTransmission(addr);
        Wire.write(reg);
        Wire.endTransmission(false);
        Wire.requestFrom(static_cast<int>(addr), 1);
        return Wire.available() ? static_cast<uint8_t>(Wire.read()) : 0;
    }

    void triggerEffect(uint8_t effect) {
        writeReg(REG_WAVESEQ1, effect);
        writeReg(REG_WAVESEQ2, 0);
        writeReg(REG_GO, 1);
    }

    bool waitGoClear(unsigned long timeoutMs) {
        unsigned long start = millis();
        while ((readReg(REG_GO) & 0x01) != 0) {
            if (millis() - start > timeoutMs) {
                return false;
            }
            yield();
        }
        return true;
    }

   public:
    explicit Haptics(uint8_t i2cAddr = kDefaultAddr) : addr(i2cAddr) {}

    bool begin() {
        // Wire.begin() expected to have been called (e.g. by display).
        writeReg(REG_FEEDBACK, 0x80);  // LRA mode
        writeReg(REG_LIBRARY, LIB_LRA);
        writeReg(REG_CONTROL3, 0xA3);
        writeReg(REG_RATED_V, 0x50);
        writeReg(REG_OD_CLAMP, 0x89);

        writeReg(REG_MODE, MODE_CALIB);
        writeReg(REG_GO, 1);
        if (!waitGoClear(2000)) {
            LOG_ERROR("[Haptics] Calibration timeout");
            initialized = false;
            return false;
        }

        uint8_t status = readReg(REG_STATUS);
        if (status & 0x08) {
            LOG_ERROR("[Haptics] Calibration failed (status 0x%02X)", status);
            initialized = false;
            return false;
        }

        writeReg(REG_MODE, MODE_INTTRIG);
        initialized = true;
        LOG_DEBUG("[Haptics] DRV2605 ready at 0x%02X", addr);
        return true;
    }

    bool isReady() const { return initialized; }

    /** Raw ROM effect (library 6 / LRA). */
    void playEffect(uint8_t effectId) {
        if (!initialized) {
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

    /** Primary button “tap” on press (Taptic-style, avoids double-hit on release). */
    void playButtonPress() { playLightImpact(); }

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
