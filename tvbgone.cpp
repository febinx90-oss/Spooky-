/* ____________________________
   TV-B-Gone idle blaster for nRFBox
   IR LED: GPIO12 -> 100ohm -> IR LED -> GND
   Fires all power-off codes after 5s idle on main menu.
   Any button press stops it immediately.
   ________________________________________ */

#include "config.h"
#include "tvbgone.h"
#include <IRremote.hpp>

#define IR_TX_PIN       12
#define IDLE_TIMEOUT_MS 5000   // 5 seconds idle before firing starts
#define SIGNAL_GAP_MS   250    // gap between each signal

namespace TvBGone {

// ── Signal table ─────────────────────────────────────────────────────────
struct IRSignal {
  uint8_t  protocol; // 0=NEC 1=SONY 3=RC5 4=RC6
  uint16_t address;
  uint16_t command;
  uint8_t  bits;
};

static const IRSignal SIGNALS[] = {
  // Sony 12-bit (Bravia KD, KV, Wega, Trinitron)
  {1, 0x0001, 21, 12},
  // Sony 15-bit (Bravia XR)
  {1, 0x0040, 21, 15},
  // Sony 20-bit
  {1, 0x0A8B, 21, 20},

  // Samsung LE / Crystal UHD / TU Series
  {0, 0x0707, 0x0002, 32},
  // Samsung QLED / Neo QLED
  {0, 0xE0E0, 0x40BF, 32},
  // Samsung Old (pre-2010)
  {0, 0x0000, 0x02FD, 32},

  // Impex Stellar
  {0, 0x00FF, 0x02FD, 32},
  // Impex Platina
  {0, 0x8080, 0x0CF3, 32},
  // Impex Generic
  {0, 0x1010, 0x02FD, 32},

  // Philips RC5 (older)
  {3, 0x00, 12, 12},
  // Philips RC6 (2010+, Ambilight, OLED, 55PUS)
  {4, 0x00, 12, 20},
};

static const uint8_t SIGNAL_COUNT = sizeof(SIGNALS) / sizeof(SIGNALS[0]);

// ── State ─────────────────────────────────────────────────────────────────
static unsigned long lastActivityTime = 0;
static bool          firing           = false;
static uint8_t       sigIndex         = 0;
static unsigned long lastFireTime     = 0;

// ── Internal: send one signal ─────────────────────────────────────────────
static void sendOne(const IRSignal& s) {
  switch (s.protocol) {
    case 0: IrSender.sendNEC(s.address, s.command, 3);          break;
    case 1: IrSender.sendSony(s.address, s.command, 3, s.bits); break;
    case 3: IrSender.sendRC5(s.address, s.command, 3);          break;
    case 4: IrSender.sendRC6(s.address, s.command, 3);          break;
  }
}

// ── Public API ────────────────────────────────────────────────────────────

void tvbgoneInit() {
  IrSender.begin(IR_TX_PIN);
  lastActivityTime = millis();
  firing           = false;
  sigIndex         = 0;
}

// Call this whenever any button is pressed in the main menu
void tvbgoneButtonHit() {
  lastActivityTime = millis();
  firing           = false;
  sigIndex         = 0;
}

// Call this every loop() while on main menu (current_screen == 0)
void tvbgoneTick() {
  unsigned long now = millis();

  if (!firing) {
    // Start firing after 5s idle
    if (now - lastActivityTime >= IDLE_TIMEOUT_MS) {
      firing       = true;
      sigIndex     = 0;
      lastFireTime = 0; // fire immediately on first tick
    }
    return;
  }

  // Firing mode — send one signal every SIGNAL_GAP_MS
  if (now - lastFireTime >= SIGNAL_GAP_MS) {
    lastFireTime = now;
    sendOne(SIGNALS[sigIndex]);
    sigIndex = (sigIndex + 1) % SIGNAL_COUNT; // loop forever
  }
}

} // namespace TvBGone
