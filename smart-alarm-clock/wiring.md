# Wiring — ESP32 DevKit V1 (30-pin)

Wire everything with female–female Dupont jumpers. All modules run on the
ESP32's 3.3 V / 5 V rails (VIN = 5 V from USB).

## TFT 2.4" ILI9341 (SPI)

| TFT pin | ESP32 pin | Note |
|---------|-----------|------|
| VCC     | VIN (5 V) | most modules have their own 3.3 V regulator |
| GND     | GND | |
| CS      | GPIO 5 | |
| RESET   | GPIO 17 | |
| DC      | GPIO 16 | |
| MOSI    | GPIO 23 | |
| SCK     | GPIO 18 | |
| MISO    | GPIO 19 | (unused by firmware, but wire it — harmless) |
| LED     | GPIO 4  | backlight, PWM brightness control |

> Some TFT modules label the backlight pin `LED` or `BL`. If your module's
> LED pin is hardwired on, leave GPIO 4 unconnected and brightness control
> will simply have no effect.

## DS3231 RTC (I²C)

| RTC pin | ESP32 pin |
|---------|-----------|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

## Buzzer (passive, 5 V)

| Buzzer pin | ESP32 pin |
|------------|-----------|
| + | GPIO 25 |
| − | GND |

## Buttons (×3, tactile, active LOW)

Each button: one leg → GPIO, other leg → GND. Internal pull-ups are enabled
in firmware, so no resistors needed.

| Button | ESP32 pin | Function |
|--------|-----------|----------|
| MENU   | GPIO 32 | open menu / select / hold 2 s = back |
| +      | GPIO 33 | up / increase |
| −      | GPIO 27 | down / decrease |

## DHT22

| DHT22 pin | ESP32 pin |
|-----------|-----------|
| VCC | 3V3 |
| DATA | GPIO 26 |
| GND | GND |

(The module version usually has the pull-up resistor built in.)

## WS2812B strip (8 LEDs)

| Strip | ESP32 pin |
|-------|-----------|
| 5 V (red) | VIN (5 V) |
| GND (white/black) | GND |
| DIN (green) | GPIO 13 |

> Power note: 8 LEDs at full white can draw ~0.5 A. The ESP32's USB 5 V rail
> handles this fine. For longer strips, power the strip directly from the
> 5 V supply instead of through the board.

## Block diagram

```
            ┌─────────────────────┐
            │   2.4" TFT (SPI)    │── CS:5 DC:16 RST:17 MOSI:23 SCK:18 LED:4
            └─────────────────────┘
┌────────┐   ┌─────────────────────┐
│ USB 5V │──▶│   ESP32 DevKit      │── I2C 21/22 ──▶ DS3231 RTC
└────────┘   │                     │── GPIO 25 ────▶ Buzzer
             │                     │── GPIO 32/33/27 → Buttons (to GND)
             │                     │── GPIO 26 ────▶ DHT22
             │                     │── GPIO 13 ────▶ WS2812B strip
             └─────────────────────┘
```
