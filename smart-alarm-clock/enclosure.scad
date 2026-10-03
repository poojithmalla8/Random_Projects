// ============================================================================
// Smart Alarm Clock — 3D Printable Enclosure (OpenSCAD)
// ============================================================================
// Units: millimeters. Print: PLA/PETG, 0.2 mm layers, 3 walls, 20% infill.
// No supports needed except maybe the USB cutout bridge (usually fine).
//
// RENDER: open in OpenSCAD, press F6, then File > Export > STL.
//   part = "shell"  -> main body (print 1x)
//   part = "back"   -> back cover (print 1x)
//   part = "layout" -> transparent fit-check with component mockups (do not print)
//
// MEASURE YOUR PARTS FIRST: update the dims below if your modules differ.
// ============================================================================

part = "assembly"; // "assembly" | "shell" | "back" | "layout"

// ---- Case dims ----
W = 150; H = 95; D = 78; wall = 3; r = 6;

// ---- Component dims (tweak to match your parts) ----
tft_win   = [52, 39];   // visible screen cutout (2.4" TFT active area ~49x37)
tft_pcb   = [71, 42];   // TFT module PCB
esp_board = [28.5, 54]; // ESP32 DevKit: [x width, z length], USB end toward back
esp_holes = [48, 22];   // mounting-hole rectangle — MEASURE your board!
spk_d     = 40;         // speaker diameter

// ---- Layout positions ----
tft_c   = [-28, 10];    // display window center (x, y) on front face
spk_c   = [42, 10];     // speaker center (x, y) on front face
btn_xs  = [-50, -24, 2];// 3 button x positions, y = -30
btn_y   = -30;
esp_c   = [15, 29];     // ESP32 center (x, z) on the floor, USB toward back
rtc_c   = [-40, 15];    // RTC module (foam-tape mounted)
buzz_c  = [-55, 45];    // buzzer (x, z)
dht_c   = [-58, 25];    // DHT22 (foam-tape mounted, near side vents)
led_slot= [0, 14];      // sunrise LED slot center (x, z) on top
usb_c   = [15, -35];    // USB cutout center (x, y) on back cover

engrave_text = true;

// ============================================================================
module rounded_cube(s, r) {
  hull()
    for (x = [-1, 1], y = [-1, 1], z = [-1, 1])
      translate([x * (s[0]/2 - r), y * (s[1]/2 - r), z * (s[2]/2 - r)])
        sphere(r, $fn = 20);
}

// ---- Interior mounting features (kept solid while hollowing the shell) ----
module posts() {
  // ESP32: 4 posts, 8 mm tall, on the floor
  for (sx = [-1, 1], sz = [-1, 1])
    translate([esp_c[0] + sx * esp_holes[0]/2, -H/2 + wall + 4, esp_c[1] + sz * esp_holes[1]/2])
      rotate([90, 0, 0]) cylinder(r = 3, h = 8, center = true, $fn = 20);
  // TFT standoffs: 4 posts behind the front panel (PCB corners inset 3.5 mm)
  for (sx = [-1, 1], sy = [-1, 1])
    translate([tft_c[0] + sx * (tft_pcb[0]/2 - 3.5), tft_c[1] + sy * (tft_pcb[1]/2 - 3.5), D - wall - 6])
      cylinder(r = 3.5, h = 12, center = true, $fn = 20);
  // Speaker: 3 locator posts around the pocket
  for (a = [90, 210, 330])
    translate([spk_c[0] + 21 * cos(a), spk_c[1] + 21 * sin(a), D - wall - 3])
      cylinder(r = 3, h = 6, center = true, $fn = 16);
  // Buzzer: 3 small feet (leave the floor sound-hole open between them)
  for (a = [90, 210, 330])
    translate([buzz_c[0] + 8 * cos(a), -H/2 + wall + 2, buzz_c[1] + 8 * sin(a)])
      rotate([90, 0, 0]) cylinder(r = 2, h = 4, center = true, $fn = 12);
  // Back-cover screw bosses (M3 heat-set inserts, 4.1 mm hole)
  for (sx = [-1, 1], sy = [-1, 1])
    translate([sx * 67, sy * 39.5, 6])
      cylinder(r = 5, h = 12, center = true, $fn = 20);
}

// ---- All cutouts (windows, holes, vents, slots) ----
module cutouts() {
  fc = D - wall/2; // center of the front wall (for through-cutouts)
  // Display window
  translate([tft_c[0], tft_c[1], fc]) cube([tft_win[0], tft_win[1], wall + 2], center = true);
  // Speaker grille: hex-ish hole pattern
  for (iy = [-2:2], ix = [-2:2]) {
    px = spk_c[0] + ix * 7 + (iy % 2 == 0 ? 0 : 3.5);
    py = spk_c[1] + iy * 6;
    if ((px - spk_c[0]) * (px - spk_c[0]) + (py - spk_c[1]) * (py - spk_c[1]) < 17 * 17)
      translate([px, py, fc]) cylinder(d = 4, h = wall + 2, center = true, $fn = 16);
  }
  // Buttons (7 mm holes for 12 mm tactile buttons behind the panel)
  for (bx = btn_xs)
    translate([bx, btn_y, fc]) cylinder(d = 7, h = wall + 2, center = true, $fn = 20);
  // Sunrise LED slot through the top
  translate([led_slot[0], H/2 - 1, led_slot[1]]) cube([70, wall + 2, 12], center = true);
  // DHT vent slits on the left wall
  for (i = [0:5])
    translate([-W/2, -30, 14 + i * 6]) cube([wall + 4, 2.5, 20], center = true);
  // Buzzer sound hole through the floor
  translate([buzz_c[0], -H/2, buzz_c[1]]) rotate([90, 0, 0])
    cylinder(d = 4, h = wall + 2, center = true, $fn = 16);
  // ESP32 post screw holes (M3 self-tapping)
  for (sx = [-1, 1], sz = [-1, 1])
    translate([esp_c[0] + sx * esp_holes[0]/2, -H/2 + wall + 4, esp_c[1] + sz * esp_holes[1]/2])
      rotate([90, 0, 0]) cylinder(r = 1.5, h = 12, center = true, $fn = 12);
  // TFT standoff screw holes
  for (sx = [-1, 1], sy = [-1, 1])
    translate([tft_c[0] + sx * (tft_pcb[0]/2 - 3.5), tft_c[1] + sy * (tft_pcb[1]/2 - 3.5), D - wall - 6])
      cylinder(r = 1.5, h = 16, center = true, $fn = 12);
  // Back-cover boss holes for heat-set inserts
  for (sx = [-1, 1], sy = [-1, 1])
    translate([sx * 67, sy * 39.5, 6]) cylinder(r = 2.05, h = 16, center = true, $fn = 16);
  // Engraved label
  if (engrave_text)
    translate([tft_c[0], -41, D - 1.2])
      linear_extrude(1.4) text("SMART CLOCK", size = 6, halign = "center", valign = "center");
}

// ---- Main shell ----
module shell() {
  difference() {
    translate([0, 0, D/2]) rounded_cube([W, H, D], r);          // outer
    difference() {
      translate([0, 0, (D - wall)/2 - 0.5])                     // inner hollow,
        rounded_cube([W - 2*wall, H - 2*wall, D - wall + 1], r);// open at back
      posts();                                                 // ...minus posts
    }
    cutouts();
  }
}

// ---- Back cover ----
module back() {
  difference() {
    translate([0, 0, -1.5]) rounded_cube([W, H, 3], 1.5);
    translate([usb_c[0], usb_c[1], -4]) cube([12, 7, 8], center = true); // USB
    for (i = [0:5])                                                        // vents
      translate([-50 + i * 20, 18, -4]) cube([26, 3, 8], center = true);
    for (sx = [-1, 1], sy = [-1, 1])                                       // screws
      translate([sx * 67, sy * 39.5, -4]) cylinder(d = 3.4, h = 8, center = true, $fn = 16);
  }
}

// ---- Transparent fit-check with component mockups (do NOT print) ----
module layout() {
  color("LightBlue", 0.25) shell();
  color("Red")    translate([esp_c[0], -H/2 + wall + 9.5, esp_c[1]]) cube([esp_board[0], 3, esp_board[1]], center = true);
  color("Green")  translate([tft_c[0], tft_c[1], D - wall - 10]) cube([tft_pcb[0], tft_pcb[1], 4], center = true);
  color("Orange") translate([spk_c[0], spk_c[1], D - wall - 5]) cylinder(d = spk_d, h = 8, center = true, $fn = 32);
  color("Purple") translate([rtc_c[0], -H/2 + wall + 1.5, rtc_c[1]]) cube([38, 3, 22], center = true);
  color("Yellow") translate([buzz_c[0], -H/2 + wall + 3, buzz_c[1]]) rotate([90,0,0]) cylinder(d = 12, h = 6, center = true, $fn = 20);
  color("Cyan")   translate([dht_c[0], -H/2 + wall + 2, dht_c[1]]) cube([26, 4, 13], center = true);
  color("White")  translate([led_slot[0], H/2 - wall - 1, led_slot[1]]) cube([64, 2, 10], center = true);
}

// ---- Top-level ----
if (part == "shell") shell();
else if (part == "back") back();
else if (part == "layout") layout();
else { shell(); back(); }
