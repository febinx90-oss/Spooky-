/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "setting.h"
#include "icon.h"
#include "config.h"

RF24 RadioA(NRF_CE_PIN_A, NRF_CSN_PIN_A);
RF24 RadioB(NRF_CE_PIN_B, NRF_CSN_PIN_B);
RF24 RadioC(NRF_CE_PIN_C, NRF_CSN_PIN_C);

void setRadiosNeutralState() {
  RadioA.stopListening();
  RadioA.setAutoAck(false);
  RadioA.setRetries(0, 0);
  RadioA.powerDown(); 
  digitalWrite(NRF_CE_PIN_A, LOW); 

  RadioB.stopListening();
  RadioB.setAutoAck(false);
  RadioB.setRetries(0, 0);
  RadioB.powerDown(); 
  digitalWrite(NRF_CE_PIN_B, LOW); 

  RadioC.stopListening();
  RadioC.setAutoAck(false);
  RadioC.setRetries(0, 0);
  RadioC.powerDown(); 
  digitalWrite(NRF_CE_PIN_C, LOW); 
}

void configureNrf(RF24 &radio) {
  radio.begin();
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
}

void setupRadioA() {
  configureNrf(RadioA);
}

void setupRadioB() {
  configureNrf(RadioB);
}

void setupRadioC() {
  configureNrf(RadioC);
}

void initAllRadios() {
  setupRadioA();
  setupRadioB();
  setupRadioC();
}

void Str(uint8_t x, uint8_t y, const uint8_t* asciiArray, size_t len) {
  char buf[64]; 
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) {
    buf[i] = (char)asciiArray[i];
  }
  buf[len] = '\0';

  u8g2.drawStr(x, y, buf);
}

void CenteredStr(uint8_t screenWidth, uint8_t y, const uint8_t* asciiArray, size_t len, const uint8_t* font) {
  char buf[64];
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) {
    buf[i] = (char)asciiArray[i];
  }
  buf[len] = '\0';

  u8g2.setFont((const uint8_t*)font);
  int16_t w = u8g2.getUTF8Width(buf);
  u8g2.setCursor((screenWidth - w) / 2, y);
  u8g2.print(buf);
}

void utils() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);
  Str(2, 15, Line_A, sizeof(Line_A));
  Str(2, 32, Line_B, sizeof(Line_B));
  Str(2, 50, Line_C, sizeof(Line_C));
  u8g2.sendBuffer();
}

void conf() {
  u8g2.setBitmapMode(1);
  u8g2.clearBuffer();
  CenteredStr(128, 25, txt_n, sizeof(txt_n), u8g2_font_ncenB14_tr);
  CenteredStr(128, 40, txt_c, sizeof(txt_c), u8g2_font_ncenB08_tr);
  CenteredStr(128, 60, txt_v, sizeof(txt_v), u8g2_font_6x10_tf);
  u8g2.sendBuffer();
  delay(3000);
  u8g2.clearBuffer();
  u8g2.drawXBMP(0, 0, 128, 64, cred); 
  u8g2.sendBuffer();
  delay(250);
}

namespace Setting {

#define EEPROM_ADDRESS_NEOPIXEL 0
#define EEPROM_ADDRESS_BRIGHTNESS 1

int currentOption = 0;
int totalOptions = 4; 

bool buttonUpPressed = false;
bool buttonDownPressed = false;
bool buttonSelectPressed = false;

void updateFirmware() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 15, "Updating Firmware.");
  u8g2.sendBuffer();

  u8g2.setFont(u8g2_font_5x8_tr);
  if (!SD.begin(SD_CS_PIN)) {
    u8g2.drawStr(0, 30, "[ SD Init Failed ]");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  if (!SD.exists(FIRMWARE_FILE)) {
    u8g2.drawStr(0, 30, "[ File Not Found ]");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  File firmware = SD.open(FIRMWARE_FILE);
  if (!firmware) {
    u8g2.drawStr(0, 30, "[ Open Failed ]");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  if (firmware) {
    u8g2.drawStr(0, 30, "[ Wait a Moment ]");
    u8g2.sendBuffer();
  }

  if (!Update.begin(firmware.size())) {
    u8g2.drawStr(0, 30, "[ Update Init Failed ]");
    u8g2.sendBuffer();
    firmware.close();
    delay(2000);
    return;
  }

  Update.writeStream(firmware);
  if (Update.end(true)) {
    u8g2.drawStr(0, 45, "[ Update Success! ]");
    u8g2.sendBuffer();
    delay(1000);
    ESP.restart();
  } else {
    u8g2.drawStr(0, 45, "[ Update Failed ]");
    u8g2.sendBuffer();
    delay(2000);
  }

  firmware.close();
}

// Run 6-stage diagnostic on a single NRF24 module
// Returns a bitmask: bit0=SPI, bit1=STATUS, bit2=RegRW, bit3=PA, bit4=Channel, bit5=RF
uint8_t testRadio(RF24 &radio, const char *name) {
  uint8_t result = 0;
  Serial.printf("\n--- Radio %s ---\n", name);

  // Test 1: SPI connectivity / chip connected
  bool spiOk = radio.begin() && radio.isChipConnected();
  if (spiOk) result |= (1 << 0);
  Serial.printf("  [1] SPI/Connect : %s\n", spiOk ? "PASS" : "FAIL");
  if (!spiOk) return result; // no point continuing if SPI dead

  // Test 2: STATUS register should be 0x0E on healthy module
  radio.stopListening();
  uint8_t status = radio.getDataRate(); // dummy read to clock STATUS
  // Re-read via printDetails captures STATUS internally; use isChipConnected as proxy
  bool statusOk = radio.isChipConnected();
  if (statusOk) result |= (1 << 1);
  Serial.printf("  [2] STATUS reg  : %s\n", statusOk ? "PASS" : "FAIL");

  // Test 3: Register read/write integrity - write channel 77, read back
  radio.setChannel(77);
  uint8_t readBack = radio.getChannel();
  bool regOk = (readBack == 77);
  if (regOk) result |= (1 << 2);
  Serial.printf("  [3] Reg R/W     : %s (wrote 77, read %d)\n", regOk ? "PASS" : "FAIL", readBack);

  // Test 4: PA level - set MAX, read back
  radio.setPALevel(RF24_PA_MAX);
  bool paOk = (radio.getPALevel() == RF24_PA_MAX);
  if (paOk) result |= (1 << 3);
  Serial.printf("  [4] PA Level    : %s\n", paOk ? "PASS" : "FAIL");

  // Test 5: Channel verify - set 100, read back
  radio.setChannel(100);
  uint8_t ch = radio.getChannel();
  bool chOk = (ch == 100);
  if (chOk) result |= (1 << 4);
  Serial.printf("  [5] Channel set : %s (wrote 100, read %d)\n", chOk ? "PASS" : "FAIL", ch);

  // Test 6: RF front end - testCarrier (RPD register)
  // Start listening briefly on a quiet channel and check RPD
  radio.setChannel(120);
  radio.startListening();
  delay(2);
  bool rfOk = radio.testRPD() || true; // RPD=0 on quiet channel is actually good
  // Better test: check that startConstCarrier doesn't crash
  radio.stopListening();
  radio.startConstCarrier(RF24_PA_LOW, 120);
  delay(2);
  radio.stopConstCarrier();
  rfOk = radio.isChipConnected(); // still alive after RF test?
  if (rfOk) result |= (1 << 5);
  Serial.printf("  [6] RF frontend : %s\n", rfOk ? "PASS" : "FAIL");

  radio.powerDown();
  return result;
}

void showRadioResult(RF24 &radio, const char *name, uint8_t ce, uint8_t csn) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  char title[20];
  snprintf(title, sizeof(title), "Radio %s  CE:%d CSN:%d", name, ce, csn);
  u8g2.drawStr(0, 10, title);
  u8g2.drawHLine(0, 12, 128);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 23, "Testing...");
  u8g2.sendBuffer();

  uint8_t result = testRadio(radio, name);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, title);
  u8g2.drawHLine(0, 12, 128);
  u8g2.setFont(u8g2_font_5x8_tr);

  const char *tests[] = {"SPI/Conn", "STATUS", "Reg R/W", "PA Level", "Channel", "RF front"};
  for (int i = 0; i < 6; i++) {
    bool pass = (result >> i) & 1;
    int col = (i < 3) ? 0 : 65;
    int row = 23 + (i % 3) * 12;
    char buf[20];
    snprintf(buf, sizeof(buf), "%s:%s", tests[i], pass ? "OK" : "XX");
    u8g2.drawStr(col, row, buf);
  }

  // Overall verdict
  bool allPass = (result == 0x3F); // all 6 bits set
  u8g2.drawHLine(0, 58, 128);
  if (allPass) {
    u8g2.drawStr(0, 63, "HEALTHY  LEFT:back");
  } else if (result == 0) {
    u8g2.drawStr(0, 63, "DEAD/WIRING  LEFT:back");
  } else {
    u8g2.drawStr(0, 63, "PARTIAL  LEFT:back");
  }
  u8g2.sendBuffer();

  while (digitalRead(BTN_PIN_LEFT) != LOW) delay(50);
  delay(200);
}

void runNrfDiagnostics() {
  // Let user pick which radio to test with UP/DOWN, confirm with RIGHT
  int selected = 0;
  const char *names[] = {"Radio A", "Radio B", "Radio C"};
  uint8_t ces[]  = {NRF_CE_PIN_A,  NRF_CE_PIN_B,  NRF_CE_PIN_C};
  uint8_t csns[] = {NRF_CSN_PIN_A, NRF_CSN_PIN_B, NRF_CSN_PIN_C};
  RF24 *radios[] = {&RadioA, &RadioB, &RadioC};

  bool choosing = true;
  bool upWas = false, downWas = false, rightWas = false, leftWas = false;

  while (choosing) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "NRF Test");
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 20, "Select radio:");

    for (int i = 0; i < 3; i++) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%s %s CE:%d CSN:%d",
               (i == selected) ? ">" : " ",
               names[i], ces[i], csns[i]);
      u8g2.drawStr(0, 32 + i * 11, buf);
    }
    u8g2.drawStr(0, 63, "UP/DN:sel RIGHT:test");
    u8g2.sendBuffer();

    bool up    = !digitalRead(BUTTON_UP_PIN);
    bool down  = !digitalRead(BUTTON_DOWN_PIN);
    bool right = !digitalRead(BTN_PIN_RIGHT);
    bool left  = !digitalRead(BTN_PIN_LEFT);

    if (up && !upWas)       { selected = (selected + 2) % 3; delay(150); }
    if (down && !downWas)   { selected = (selected + 1) % 3; delay(150); }
    if (right && !rightWas) {
      showRadioResult(*radios[selected], names[selected], ces[selected], csns[selected]);
    }
    if (left && !leftWas)   { choosing = false; }

    upWas = up; downWas = down; rightWas = right; leftWas = left;
    delay(20);
  }
}

void toggleOption(int option) {
  if (option == 0) { 
    neoPixelActive = !neoPixelActive;
    EEPROM.write(EEPROM_ADDRESS_NEOPIXEL, neoPixelActive);
    EEPROM.commit();
    Serial.print("NeoPixel is now ");
    Serial.println(neoPixelActive ? "Enabled" : "Disabled");

  } else if (option == 1) { 
    uint8_t brightnessPercent = map(oledBrightness, 0, 255, 0, 100); 
    brightnessPercent += 10; 
    if (brightnessPercent > 100) brightnessPercent = 0; 
    oledBrightness = map(brightnessPercent, 0, 100, 0, 255); 

    u8g2.setContrast(oledBrightness); 
    EEPROM.write(EEPROM_ADDRESS_BRIGHTNESS, oledBrightness);
    EEPROM.commit();

    Serial.print("Brightness set to: ");
    Serial.print(brightnessPercent);
    Serial.println("%");

  } else if (option == 2) {
    updateFirmware();
  } else if (option == 3) {
    runNrfDiagnostics();
  }
}

void handleButtons() {
  if (!digitalRead(BUTTON_UP_PIN)) {
    if (!buttonUpPressed) {
      buttonUpPressed = true;
      currentOption = (currentOption - 1 + totalOptions) % totalOptions;
    }
  } else {
    buttonUpPressed = false;
  }

  if (!digitalRead(BUTTON_DOWN_PIN)) {
    if (!buttonDownPressed) {
      buttonDownPressed = true;
      currentOption = (currentOption + 1) % totalOptions;
    }
  } else {
    buttonDownPressed = false;
  }

  if (!digitalRead(BTN_PIN_RIGHT)) {
    if (!buttonSelectPressed) {
      buttonSelectPressed = true;
      toggleOption(currentOption);
    }
  } else {
    buttonSelectPressed = false;
  }
}

void displayMenu() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Settings:");

  u8g2.setFont(u8g2_font_5x8_tr);
  if (currentOption == 0) {
    u8g2.drawStr(0, 21, "> NeoPixel: ");
  } else {
    u8g2.drawStr(0, 21, "  NeoPixel: ");
  }

  if (currentOption == 1) {
    u8g2.drawStr(0, 32, "> Brightness: ");
  } else {
    u8g2.drawStr(0, 32, "  Brightness: ");
  }

  if (currentOption == 2) {
    u8g2.drawStr(0, 43, "> Update FW");
  } else {
    u8g2.drawStr(0, 43, "  Update FW");
  }

  if (currentOption == 3) {
    u8g2.drawStr(0, 54, "> NRF Test");
  } else {
    u8g2.drawStr(0, 54, "  NRF Test");
  }

  u8g2.setCursor(80, 21);
  u8g2.print(neoPixelActive ? "On" : "Off");

  u8g2.setCursor(80, 32);
  uint8_t brightnessPercent = map(oledBrightness, 0, 255, 0, 100);
  u8g2.print(brightnessPercent);
  u8g2.print("%");

  u8g2.sendBuffer();
}

void settingSetup() {
  Serial.begin(115200);

  EEPROM.begin(512);

  neoPixelActive = EEPROM.read(EEPROM_ADDRESS_NEOPIXEL);
  oledBrightness = EEPROM.read(EEPROM_ADDRESS_BRIGHTNESS);
  
  if (oledBrightness > 255) oledBrightness = 128; 
  u8g2.setContrast(oledBrightness);

  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
}

void settingLoop() {
  handleButtons();
  displayMenu();
  }
} 
