# Smart Alarm Clock — Bill of Materials

Everything below is commonly available (Amazon, AliExpress, Adafruit, etc.).
If you already own some of these, just check them off — the design is
parameterized so small size differences are easy to accommodate.

## Core electronics

| # | Part | Spec | Qty | Notes |
|---|------|------|-----|-------|
| 1 | ESP32 DevKit V1 (30-pin) | 54 × 28.5 mm, micro-USB or USB-C | 1 | The brains. WiFi for NTP time sync + web config |
| 2 | 2.4" TFT LCD, SPI (ILI9341) | 320×240, ~71 × 42 mm PCB | 1 | Main display. Any 2.4" SPI ILI9341 module works |
| 3 | DS3231 RTC module | with CR2032 battery included | 1 | Keeps time when WiFi/power is out |
| 4 | Passive buzzer | 5 V, 12 mm | 1 | Alarm sound (melodies in firmware) |
| 5 | Tactile push buttons | 12 × 12 mm | 3 | MENU / + / − |
| 6 | DHT22 (AM2302) | temp + humidity sensor | 1 | Shows room temp/humidity on screen |
| 7 | WS2812B LED strip | 8 LEDs, 5 V | 1 | Sunrise-simulation glow before the alarm |
| 8 | USB cable + 5 V / 2 A supply | matches your ESP32's port | 1 | Power |

## Enclosure hardware

| # | Part | Spec | Qty | Notes |
|---|------|------|-----|-------|
| 9 | M3 × 8 mm screws | + M3 heat-set inserts (or nuts) | ~14 | Mount boards + close the case |
| 10 | M3 × 12 mm brass standoffs | male–female | 4 | Mount the TFT behind the front panel |
| 11 | Rubber feet | adhesive, ~10 mm | 4 | |
| 12 | Jumper wires | female–female Dupont | ~20 | Internal wiring |
| 13 | (Optional) 18650 holder + TP4056 charger | | 1 | Battery backup for portability |

**Approx. total: $25–40** if buying everything new.

## What each part does in this build

- **ESP32** — runs the clock, connects to WiFi, syncs time via NTP, serves a tiny
  web page where you set alarms from your phone.
- **DS3231** — battery-backed real-time clock; the alarm still works with no WiFi.
- **TFT** — big readable time, date, temperature, alarm indicators.
- **Buzzer** — plays alarm melodies (two built in, easy to add more).
- **Buttons** — set time/alarms/brightness on the device itself.
- **DHT22** — room climate on the display.
- **WS2812B strip** — fades from deep orange to bright white in the 10 minutes
  before your alarm (sunrise simulation), hidden in a top channel so it glows
  without blinding you.

> Measure your actual TFT module's PCB and visible screen area and update
> `tft_pcb` / `tft_win` at the top of `enclosure.scad` if they differ.
