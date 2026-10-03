# Wiring for Dummies

Never wired anything before? Perfect — this guide assumes zero experience.
Open `circuit-schematic.svg` next to this file and follow along.

## What you're building (the 30-second version)

Every part in this clock is a small module. Wiring just means connecting the
right pins together with little jumper wires. There are only two kinds of
connections:

1. **Power** — every module needs electricity. Red wires carry 5V, orange
   carry 3.3V, black wires are **GND** (ground, the shared "zero volts"
   reference). Think of the three colored bars on the left of the diagram as
   power strips: everything touching the red bar gets 5V, and so on.
2. **Signals** — the ESP32 (the brain, center of the diagram) talks to each
   module over its own colored wire. The diagram shows exactly where each
   one goes.

**Two rules that prevent 99% of mistakes:**
- Match pin **names**, not positions. Your modules may line up differently
  than the drawing — `SDA` goes to `SDA` no matter where it sits.
- **Never connect 5V (red) directly to 3.3V (orange) or to GND (black).**
  Double-check every red wire before plugging in USB.

## What you need

- Female-to-female Dupont jumper wires (~25). Use red for 5V, orange for
  3.3V, black for GND if you can — future-you will be grateful.
- That's it. No soldering, no resistors.

## How to read the diagram

- Each box is a module; the little labels on its edges are its pins.
- A colored line = one wire between two pins.
- A **● dot** where lines meet means those wires are joined together.
- Lines that simply **cross** with no dot are NOT connected (just passing over).

## Step-by-step

Do one module at a time. Count your wires as you go.

### Step 0 — The brain (ESP32)
No wires yet. Just find these pins on your ESP32 and put a tiny sticker or
mark next to them: `VIN`, `3V3`, `GND`, `5`, `16`, `17`, `18`, `19`, `21`,
`22`, `23`, `4`, `25`, `26`, `27`, `32`, `33`, `13`.

### Step 1 — TFT display (9 wires)
| # | Wire color | From (TFT) | To (ESP32) | What it does |
|---|------------|------------|------------|--------------|
| 1 | red | VCC | VIN (5V) | power |
| 2 | black | GND | GND | ground |
| 3 | any | CS | GPIO 5 | "hey display, listen" |
| 4 | any | RST | GPIO 17 | reset |
| 5 | any | DC | GPIO 16 | data vs command |
| 6 | any | MOSI | GPIO 23 | screen data |
| 7 | any | SCK | GPIO 18 | clock signal |
| 8 | any | MISO | GPIO 19 | (unused, wire it anyway) |
| 9 | any | LED | GPIO 4 | backlight brightness |

### Step 2 — RTC clock module (4 wires)
| # | Wire color | From (RTC) | To (ESP32) |
|---|------------|------------|------------|
| 1 | orange | VCC | 3V3 |
| 2 | black | GND | GND |
| 3 | any | SDA | GPIO 21 |
| 4 | any | SCL | GPIO 22 |

SDA/SCL are a shared "I²C bus" — just two wires the ESP32 uses to chat
with the clock chip. Make sure the CR2032 battery is in the RTC.

### Step 3 — Buttons (6 wires)
Each button has 2 legs. One leg goes to its GPIO pin, the other leg goes
to GND. Wire all three GND legs together (daisy-chain), then run a single
black wire from that chain to ESP32 GND.

| Button | Leg 1 → | Leg 2 → |
|--------|---------|---------|
| MENU | GPIO 32 | GND |
| UP (+)| GPIO 33 | GND |
| DOWN (−) | GPIO 27 | GND |

No resistors needed — the firmware turns on the ESP32's built-in pull-ups.

### Step 4 — Buzzer (2 wires)
| # | Wire color | From (buzzer) | To (ESP32) |
|---|------------|---------------|------------|
| 1 | any | **+** | GPIO 25 |
| 2 | black | **−** | GND |

### Step 5 — DHT22 temperature sensor (3 wires)
| # | Wire color | From (DHT22) | To (ESP32) |
|---|------------|--------------|------------|
| 1 | orange | VCC | 3V3 |
| 2 | black | GND | GND |
| 3 | any | DATA | GPIO 26 |

### Step 6 — LED strip (3 wires)
| # | Wire color | From (strip) | To (ESP32) |
|---|------------|--------------|------------|
| 1 | red | 5V | VIN (5V) |
| 2 | black | GND | GND |
| 3 | any | DIN | GPIO 13 |

Plug into the **DIN** end of the strip (arrow pointing *away* from the
ESP32). 8 LEDs at full white can pull ~0.5A — fine on USB power.

### Step 7 — Power the ESP32 itself
Just plug a USB cable from your computer or a 5V phone charger into the
ESP32. That's the whole power supply. (The VIN/GND pins you wired above
are how the ESP32 *shares* that USB power with the modules.)

## Before you plug in USB — the 60-second safety check

- [ ] Every red wire goes 5V → 5V pin (VIN/VCC). None touch orange or black.
- [ ] Every module has its black GND wire connected.
- [ ] No wire is hanging loose or half-plugged.
- [ ] RTC battery is seated.
- [ ] TFT ribbon/backlight cable (if any) is firmly in.

## It doesn't work? Start here

| Symptom | Most likely cause |
|---------|-------------------|
| Screen stays dark | TFT VCC/GND swapped or loose; LED pin wire missing |
| Screen lights up but no picture | MOSI/SCK/CS/DC/RST mix-up — check names one by one |
| Time is wrong / resets | RTC SDA/SCL swapped, or dead/missing CR2032 battery |
| Buttons do nothing | Button's second leg isn't actually on GND |
| No alarm sound | Buzzer +/− reversed |
| LEDs don't light | Strip wired to DOUT end instead of DIN; or no common GND |

If one module misbehaves, unplug USB, re-check only that module's rows in
the tables above, and try again. You won't break anything by having a
signal wire in the wrong GPIO — the worst case is it just doesn't work
until you move it. (Shorting 5V to GND is the one thing to avoid, hence
the checklist.)
