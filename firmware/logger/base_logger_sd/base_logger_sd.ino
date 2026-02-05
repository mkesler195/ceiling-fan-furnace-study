/*
  FINAL LOGGER (Fan + Furnace + BMP + SD + RTC + LINK HEALTH + SETPOINT)

  Manual serial input:
    fan off | fan cw | fan ccw
    out <temp>
    set <temp>

  Timed logging:
    mins <n>   -> log for n minutes (starting now)
    start      -> resume logging (no time limit)
    stop       -> stop logging (no SD rows)

  Logs periodic rows (not event-only).

  Packet hardening:
    - Adds magic byte 0xA5 for validation
    - Includes furnaceOn state in EVERY packet (incl heartbeat)
    - Logger updates furnaceOn from every valid packet

  Enhancement:
    - Tracks lastStateEvt separately (only updates on evt=1 or evt=2)
*/

#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <NRFLite.h>
#include <RTClib.h>
#include <Adafruit_BMP280.h>

// ---------------- Pins ----------------
const uint8_t PIN_SD_CS     = 4;
const uint8_t PIN_BMP_CS    = 8;
const uint8_t PIN_RADIO_CE  = 9;
const uint8_t PIN_RADIO_CSN = 10;

// ---------------- IDs ----------------
const uint8_t LOGGER_NODE_ID   = 0;
const uint8_t LISTENER_NODE_ID = 2; // Basement listener node

// ---------------- Timing ----------------
const uint32_t STATUS_EVERY_MS = 10000;   // status print
const uint32_t LOG_EVERY_MS    = 60000;   // 1 minute logging

// ---------------- Objects ----------------
NRFLite radio;
RTC_DS3231 rtc;
Adafruit_BMP280 bmp(PIN_BMP_CS);

// ---------------- State ----------------
enum FanMode : uint8_t { FAN_OFF=0, FAN_CW=1, FAN_CCW=2 };
FanMode fanMode = FAN_OFF;

float outdoorTemp = NAN;
float setTemp     = NAN;

bool furnaceOn = false;

// Radio health (last packet)
bool     haveRx   = false;
uint32_t lastRxMs = 0;
uint8_t  lastEvt  = 0;
uint16_t lastP2P  = 0;
uint16_t lastSeq  = 0;

// NEW: last *state-change* event only (evt 1 or 2)
bool     haveStateEvt   = false;
uint8_t  lastStateEvt   = 0;  // 1=ON, 2=OFF
uint32_t lastStateEvtMs = 0;

// ---------------- Logging ----------------
const char* LOG_FILE = "wxlog.csv";

// Timed logging control
bool     loggingEnabled = true;   // default: logging ON
bool     timedMode      = false;  // true if mins <n> was set
uint32_t logEndMs       = 0;      // millis() at which timed logging ends
bool     printedDone    = false;  // ensure we print "done" once

// ---------------- Packet (MUST match listener) ----------------
struct FurnacePkt {
  uint8_t  magic;     // 0xA5
  uint8_t  nodeId;    // 2
  uint8_t  eventType; // 1=ON, 2=OFF, 3=HEARTBEAT
  uint8_t  furnaceOn; // 0/1 (ALWAYS current state)
  uint16_t seq;
  uint16_t p2p;
};

// ---------------- Helpers ----------------
void deselectAllSPI() {
  pinMode(PIN_SD_CS, OUTPUT);     digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BMP_CS, OUTPUT);    digitalWrite(PIN_BMP_CS, HIGH);
  pinMode(PIN_RADIO_CSN, OUTPUT); digitalWrite(PIN_RADIO_CSN, HIGH);
}

String ts(const DateTime& dt) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           dt.year(), dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  return String(buf);
}

void writeHeaderIfNeeded() {
  if (!SD.exists(LOG_FILE)) {
    File f = SD.open(LOG_FILE, FILE_WRITE);
    if (f) {
      // NEW: add lastStateEvt as one extra column (end)
      f.println(F("datetime,fanMode,outdoorTemp,setTemp,furnaceOn,tempC,press_hPa,linkAgeSec,lastEvt,lastSeq,p2p,lastStateEvt"));
      f.close();
      Serial.println(F("[SD] Header OK"));
    } else {
      Serial.println(F("[SD] Header write FAIL"));
    }
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println(F("=== FINAL LOGGER (Fan + Furnace + BMP + SD + RTC + LINK HEALTH + SETPOINT) ==="));

  Wire.begin();
  SPI.begin();
  deselectAllSPI();

  // RTC
  if (rtc.begin()) {
    Serial.print(F("[RTC] OK: "));
    Serial.println(ts(rtc.now()));
  } else {
    Serial.println(F("[RTC] FAIL"));
  }

  // BMP
  if (bmp.begin()) {
    Serial.println(F("[BMP] OK"));
  } else {
    Serial.println(F("[BMP] FAIL"));
  }

  // SD
  if (SD.begin(PIN_SD_CS)) {
    Serial.println(F("[SD] OK"));
    writeHeaderIfNeeded();
  } else {
    Serial.println(F("[SD] FAIL"));
  }

  // Radio
  if (radio.init(LOGGER_NODE_ID, PIN_RADIO_CE, PIN_RADIO_CSN)) {
    Serial.println(F("[RADIO] OK (listening)"));
  } else {
    Serial.println(F("[RADIO] FAIL: radio.init()"));
  }

  Serial.println(F("Type: fan off | fan cw | fan ccw | out <temp> | set <temp> | mins <n> | start | stop"));
}

// ---------------- Serial Input ----------------
void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "fan off") {
    fanMode = FAN_OFF;
    Serial.println(F("[INPUT] Fan = OFF"));
  } else if (cmd == "fan cw") {
    fanMode = FAN_CW;
    Serial.println(F("[INPUT] Fan = CW"));
  } else if (cmd == "fan ccw") {
    fanMode = FAN_CCW;
    Serial.println(F("[INPUT] Fan = CCW"));
  } else if (cmd.startsWith("out ")) {
    outdoorTemp = cmd.substring(4).toFloat();
    Serial.print(F("[INPUT] Outdoor temp = "));
    Serial.println(outdoorTemp, 2);
  } else if (cmd.startsWith("set ")) {
    setTemp = cmd.substring(4).toFloat();
    Serial.print(F("[INPUT] Setpoint = "));
    Serial.println(setTemp, 2);
  } else if (cmd.startsWith("mins ")) {
    int mins = cmd.substring(5).toInt();
    if (mins <= 0) {
      Serial.println(F("[INPUT] mins must be > 0"));
      return;
    }
    timedMode = true;
    loggingEnabled = true;
    printedDone = false;
    uint32_t nowMs = millis();
    logEndMs = nowMs + (uint32_t)mins * 60000UL;

    Serial.print(F("[INPUT] Timed logging for "));
    Serial.print(mins);
    Serial.println(F(" minute(s)."));
  } else if (cmd == "stop") {
    loggingEnabled = false;
    timedMode = false;
    Serial.println(F("[INPUT] Logging STOPPED (no SD rows will be written)."));
  } else if (cmd == "start") {
    loggingEnabled = true;
    timedMode = false;
    printedDone = false;
    Serial.println(F("[INPUT] Logging STARTED (no time limit)."));
  } else {
    Serial.println(F("[INPUT] Unknown. Try: fan off | fan cw | fan ccw | out <temp> | set <temp> | mins <n> | start | stop"));
  }
}

// ---------------- Loop ----------------
void loop() {
  handleSerial();

  uint32_t nowMs = millis();
  DateTime now = rtc.now();

  // -------- Timed logging cutoff --------
  if (timedMode && loggingEnabled) {
    // signed compare handles millis() wrap safely
    if ((int32_t)(nowMs - logEndMs) >= 0) {
      loggingEnabled = false;
      timedMode = false;
      if (!printedDone) {
        printedDone = true;
        Serial.print(F("[LOG] Timed run complete @ "));
        Serial.println(ts(now));
      }
    }
  }

  // -------- Radio RX --------
  while (radio.hasData()) {
    FurnacePkt pkt;
    radio.readData(&pkt);

    // Validate packet
    if (pkt.magic != 0xA5) {
      Serial.println(F("[RX] Ignored (bad magic)"));
      continue;
    }
    if (pkt.nodeId != LISTENER_NODE_ID) {
      Serial.println(F("[RX] Ignored (wrong node)"));
      continue;
    }
    if (pkt.eventType < 1 || pkt.eventType > 3) {
      Serial.println(F("[RX] Ignored (bad evt)"));
      continue;
    }

    haveRx   = true;
    lastRxMs = nowMs;
    lastEvt  = pkt.eventType;
    lastP2P  = pkt.p2p;
    lastSeq  = pkt.seq;

    // KEY: update furnace state on EVERY valid packet (incl heartbeat)
    furnaceOn = (pkt.furnaceOn != 0);

    // ----- State change announcement -----
  if (pkt.eventType == 1 || pkt.eventType == 2) {
  lastStateEvt = pkt.eventType;

  Serial.print(F("*** STATE CHANGE: "));
  if (pkt.eventType == 1) {
    Serial.println(F("OFF -> ON ***"));
  } else {
    Serial.println(F("ON -> OFF ***"));
  }
}

    // NEW: update lastStateEvt only on actual state-change events
    if (pkt.eventType == 1 || pkt.eventType == 2) {
      haveStateEvt = true;
      lastStateEvt = pkt.eventType;
      lastStateEvtMs = nowMs;
    }

    Serial.print(F("[RX] node="));
    Serial.print(pkt.nodeId);
    Serial.print(F(" evt="));
    Serial.print(pkt.eventType);
    Serial.print(F(" seq="));
    Serial.print(pkt.seq);
    Serial.print(F(" p2p="));
    Serial.print(pkt.p2p);
    Serial.print(F(" furnaceOn="));
    Serial.println(furnaceOn);
  }

  // -------- Status --------
  static uint32_t lastStatusMs = 0;
  if (nowMs - lastStatusMs >= STATUS_EVERY_MS) {
    lastStatusMs = nowMs;

    Serial.print(F("[STATUS] "));
    Serial.print(ts(now));
    Serial.print(F(" fan="));
    Serial.print(fanMode);

    Serial.print(F(" out="));
    if (isnan(outdoorTemp)) Serial.print(F("nan"));
    else Serial.print(outdoorTemp, 2);

    Serial.print(F(" set="));
    if (isnan(setTemp)) Serial.print(F("nan"));
    else Serial.print(setTemp, 2);

    Serial.print(F(" furnaceOn="));
    Serial.print(furnaceOn ? 1 : 0);

    if (!haveRx) {
      Serial.print(F(" link=NO_RX_YET"));
    } else {
      Serial.print(F(" link="));
      Serial.print((nowMs - lastRxMs) / 1000);
      Serial.print(F("s ago evt="));
      Serial.print(lastEvt);
      Serial.print(F(" seq="));
      Serial.print(lastSeq);
      Serial.print(F(" p2p="));
      Serial.print(lastP2P);
    }

    // NEW: show lastStateEvt (the “real” ON/OFF transition)
    Serial.print(F(" lastStateEvt="));
    if (!haveStateEvt) Serial.print(F("none"));
    else Serial.print(lastStateEvt);

    // show logging state
    Serial.print(F(" logging="));
    Serial.print(loggingEnabled ? F("ON") : F("OFF"));
    if (timedMode && loggingEnabled) {
      uint32_t remainSec = (logEndMs - nowMs) / 1000;
      Serial.print(F(" remainSec="));
      Serial.print(remainSec);
    }

    Serial.println();
  }

  // -------- Logging --------
  static uint32_t lastLogMs = 0;
  if (loggingEnabled && (nowMs - lastLogMs >= LOG_EVERY_MS)) {
    lastLogMs = nowMs;

    float tempC = bmp.readTemperature();
    float press = bmp.readPressure() / 100.0f;

    File f = SD.open(LOG_FILE, FILE_WRITE);
    if (f) {
      f.print(ts(now)); f.print(',');
      f.print(fanMode); f.print(',');

      if (isnan(outdoorTemp)) f.print("nan"); else f.print(outdoorTemp, 2);
      f.print(',');

      if (isnan(setTemp)) f.print("nan"); else f.print(setTemp, 2);
      f.print(',');

      f.print(furnaceOn ? 1 : 0); f.print(',');
      f.print(tempC, 2); f.print(',');
      f.print(press, 2); f.print(',');

      if (!haveRx) f.print("nan");
      else f.print((nowMs - lastRxMs) / 1000);
      f.print(',');

      f.print(lastEvt); f.print(',');
      f.print(lastSeq); f.print(',');
      f.print(lastP2P); f.print(',');

      // NEW: one extra field – lastStateEvt
      if (!haveStateEvt) f.println("nan");
      else f.println(lastStateEvt);

      f.close();

      Serial.print(F("[LOG] Row written @ "));
      Serial.println(ts(now));
    } else {
      Serial.println(F("[SD] Write FAIL"));
    }
  }
}
