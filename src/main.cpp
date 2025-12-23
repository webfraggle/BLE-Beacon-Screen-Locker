#include <Arduino.h>
#include <NimBLEDevice.h>
#include <FastLED.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include "debug.h"
#include <AceButton.h>
#include <EEPROM.h>
#include <WiFi.h>
using namespace ace_button;


USBHIDKeyboard Keyboard;

static const char* TARGET_MAC = "de:39:e5:a9:1e:49";
static int8_t RSSI_THRESHOLD = -60;  // RSSI Threshold for Near/Far (default)


static const int HYSTERESIS = 2;        // Hysteresis for Near/Far transitions

enum class BeaconState {
  Near,
  Far,
  Offline
};

static BeaconState beaconState = BeaconState::Offline;
static BeaconState lastBeaconState = BeaconState::Offline;

static int8_t nearFarCounter = 0;
static u_int16_t offlineCounter = 0;

// ===== LED =====
static constexpr uint8_t LED_PIN  = 15;
static constexpr uint16_t NUM_LEDS = 1;
CRGB leds[NUM_LEDS];

// ===== RSSI =====
volatile int8_t  beaconRssi = -127;
volatile bool beaconSeen = false;

static NimBLEScan* pScan = nullptr;

class MyScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    if (dev->getAddress().toString() == std::string(TARGET_MAC)) {
      beaconRssi = dev->getRSSI();   // RSSI in Variable
      beaconSeen = true;
    }
  }
};

void loadRSSIThresholdFromEEPROM() {
  EEPROM.begin(512);
  RSSI_THRESHOLD = (int8_t)EEPROM.readInt(0);
  EEPROM.end();
  if (RSSI_THRESHOLD < -127 || RSSI_THRESHOLD > 0) {  // Check if uninitialized
    RSSI_THRESHOLD = -60;
  }
}

void saveRSSIThresholdToEEPROM() {
  EEPROM.begin(512);
  EEPROM.writeInt(0, RSSI_THRESHOLD);
  EEPROM.commit();
  EEPROM.end();
}

// Buttons
AceButton button((int) BUTTON_PIN, HIGH, 0);
AceButton button1((int) BUTTON3_PIN, HIGH, 1);

void handleButton0Event(AceButton*, uint8_t, uint8_t);

void handleButton0Event(AceButton* button, uint8_t eventType,
    uint8_t buttonState) {
      // SERIAL_DEBUG_LN(button->getId());
      // SERIAL_DEBUG_LN(button->getPin());

      switch (eventType)
      {
      case AceButton::kEventPressed:
        if (button->getPin() == BUTTON_PIN) {
          SERIAL_DEBUG_LN("Button 0 Pressed");
          RSSI_THRESHOLD -= 5;
          saveRSSIThresholdToEEPROM();
          SERIAL_DEBUG_LNF("RSSI Threshold decreased to: %d", RSSI_THRESHOLD);
          #if DEBUG == 0
            Keyboard.print(String(RSSI_THRESHOLD)+" ");
            Keyboard.releaseAll();
            #endif
          } else if (button->getPin() == BUTTON3_PIN) {
            SERIAL_DEBUG_LN("Button 3 Pressed");
            RSSI_THRESHOLD += 5;
            saveRSSIThresholdToEEPROM();
            SERIAL_DEBUG_LNF("RSSI Threshold increased to: %d", RSSI_THRESHOLD);
            #if DEBUG == 0
            Keyboard.print(String(RSSI_THRESHOLD)+" ");
            Keyboard.releaseAll();
          #endif
        }

        break;
      case AceButton::kEventReleased:

        break;
      case AceButton::kEventLongPressed:
        break;
      }
}

void setup() {
  Serial.begin(115200);
#if DEBUG != 0
  delay(5000);
#endif
#if DEBUG == 0
  USB.begin();
  Keyboard.begin();
#endif
  

  // FastLED
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64);
  leds[0] = CRGB::Black;
  FastLED.show();

  SERIAL_DEBUG_LNF("Tracking RSSI of %s\n", TARGET_MAC);

  loadRSSIThresholdFromEEPROM();
  SERIAL_DEBUG_LNF("Loaded RSSI Threshold: %d", RSSI_THRESHOLD);

    // Buttonconfig
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);

  // Configure the ButtonConfig with the event handler, and enable all higher
  // level events.
  ButtonConfig* buttonConfig = button.getButtonConfig();
  buttonConfig->setEventHandler(handleButton0Event);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);
  button.setButtonConfig(buttonConfig);
  button1.setButtonConfig(buttonConfig);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  NimBLEDevice::init("");

  pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(48);
  pScan->setWindow(48);
  pScan->setDuplicateFilter(false);
  pScan->setScanCallbacks(new MyScanCallbacks(), true);
  // Kontinuierlichen Scan starten (0 = unendlich)
  pScan->start(0, true, true);
}

void loop() {

  button.check();
  button1.check();
  // Scan starten
  // if (!pScan->isScanning()) {
  //   beaconSeen = false;
  //   pScan->start(200, true, true);
  // }

  // Wenn Beacon gesehen → RSSI auswerten + LED setzen
  if (beaconSeen) {
    beaconSeen = false;
    offlineCounter = 0;

    SERIAL_DEBUG_LNF("RSSI: %d\n", beaconRssi);

    if (beaconRssi < RSSI_THRESHOLD) {
      nearFarCounter--;
      if (nearFarCounter <= -1*HYSTERESIS){
        nearFarCounter = -1*HYSTERESIS;
        beaconState = BeaconState::Far;
      }
    } else {
      nearFarCounter++;
      if (nearFarCounter >= HYSTERESIS)
      {
        nearFarCounter = HYSTERESIS;
        beaconState = BeaconState::Near;
      } 
    } 
    SERIAL_DEBUG_LNF("State: %d | NearFarCounter: %d | OfflineCounter: %d", (int)beaconState, nearFarCounter, offlineCounter);
  } else {
    offlineCounter++;
    if (offlineCounter >= 100) {
      beaconState = BeaconState::Offline;
      nearFarCounter = -2;
    }
  }
  // Handle state transitions
  switch (beaconState) {
    case BeaconState::Near:
      leds[0] = CRGB::Green;
      break;
    case BeaconState::Far:
      if (lastBeaconState == BeaconState::Near) {
        #if DEBUG == 0
          Keyboard.press(KEY_LEFT_GUI);
          Keyboard.press('l');
          Keyboard.releaseAll();
          SERIAL_DEBUG_LN("LOOOCKOUT!");
        #endif
      }
      leds[0] = CRGB::Red;
      break;
    case BeaconState::Offline:
      leds[0] = CRGB::Black;
      break;
  }
  lastBeaconState = beaconState;
  FastLED.show();
  FastLED.delay(100);
}
