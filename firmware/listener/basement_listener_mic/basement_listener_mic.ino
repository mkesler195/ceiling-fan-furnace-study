#include <SPI.h>
#include <NRFLite.h>

NRFLite radio;

// --------- IDs ----------
const uint8_t NODE_ID = 2;
const uint8_t DEST_ID = 0;

// --------- Pins ----------
const uint8_t MIC_PIN = A0;

// --------- Mic sampling ----------
const uint16_t P2P_WINDOW_MS   = 100;
const uint16_t SAMPLE_EVERY_MS = 250;

// --------- Detection thresholds (your updated values) ----------
const uint16_t LOUD_THR_ON  = 120;
const uint16_t LOUD_THR_OFF = 100;

// --------- Persistence window ----------
const uint8_t  WINDOW_SAMPLES = 40;   // 10s
const uint8_t  ON_COUNT_THR   = 26;
const uint8_t  OFF_COUNT_THR  = 12;

const uint32_t MIN_STATE_HOLD_MS = 5000;

// --------- Heartbeat ----------
const bool ENABLE_HEARTBEAT = true;
const uint32_t HEARTBEAT_ON_MS  = 15000;
const uint32_t HEARTBEAT_OFF_MS = 60000;

// --------- Packet (MUST match logger) ----------
struct FurnacePkt {
  uint8_t  magic;     // 0xA5
  uint8_t  nodeId;
  uint8_t  eventType; // 1=ON, 2=OFF, 3=HEARTBEAT
  uint8_t  furnaceOn; // ALWAYS current state
  uint16_t seq;
  uint16_t p2p;
};

uint16_t seqCounter = 0;

// Rolling window ring buffer of 0/1 loud samples
uint8_t ring[WINDOW_SAMPLES];
uint8_t ringIdx = 0;
uint16_t loudSum = 0;

enum FurnaceState : uint8_t { STATE_OFF = 0, STATE_ON = 1 };
FurnaceState state = STATE_OFF;

uint32_t lastSampleMs = 0;
uint32_t lastStateChangeMs = 0;
uint32_t lastHeartbeatMs = 0;

int readPeakToPeak(uint16_t windowMs) {
  uint32_t start = millis();
  int minV = 1023;
  int maxV = 0;
  while ((uint32_t)(millis() - start) < windowMs) {
    int v = analogRead(MIC_PIN);
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
  }
  return maxV - minV;
}

void pushLoudSample(uint8_t isLoud) {
  loudSum -= ring[ringIdx];
  ring[ringIdx] = isLoud;
  loudSum += isLoud;
  ringIdx = (ringIdx + 1) % WINDOW_SAMPLES;
}

void sendEvent(uint8_t eventType, uint16_t p2p) {
  FurnacePkt pkt;
  pkt.magic     = 0xA5;
  pkt.nodeId    = NODE_ID;
  pkt.eventType = eventType;
  pkt.furnaceOn = (state == STATE_ON) ? 1 : 0;   // <- key change
  pkt.seq       = ++seqCounter;
  pkt.p2p       = p2p;

  bool ok = radio.send(DEST_ID, &pkt, sizeof(pkt));

  Serial.print(F("[RADIO] evt="));
  Serial.print(pkt.eventType);
  Serial.print(F(" furnaceOn="));
  Serial.print(pkt.furnaceOn);
  Serial.print(F(" seq="));
  Serial.print(pkt.seq);
  Serial.print(F(" p2p="));
  Serial.print(pkt.p2p);
  Serial.print(F(" ok="));
  Serial.println(ok ? F("1") : F("0"));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println(F("=== BASEMENT FURNACE LISTENER (Mic + NRFLite TX) ==="));

  for (uint8_t i = 0; i < WINDOW_SAMPLES; i++) ring[i] = 0;
  loudSum = 0;

  if (!radio.init(NODE_ID, 9, 10)) {
    Serial.println(F("[RADIO] FAIL: radio.init()"));
    while (1) delay(1000);
  }
  Serial.println(F("[RADIO] PASS: init ok"));

  lastSampleMs = millis();
  lastStateChangeMs = millis();
  lastHeartbeatMs = millis();
}

void loop() {
  uint32_t now = millis();

  if ((uint32_t)(now - lastSampleMs) >= SAMPLE_EVERY_MS) {
    lastSampleMs = now;

    uint16_t p2p = (uint16_t)readPeakToPeak(P2P_WINDOW_MS);

    uint8_t isLoud = 0;
    if (state == STATE_OFF) isLoud = (p2p >= LOUD_THR_ON) ? 1 : 0;
    else                   isLoud = (p2p >= LOUD_THR_OFF) ? 1 : 0;

    pushLoudSample(isLoud);

    // Debug
    Serial.print(F("p2p=")); Serial.print(p2p);
    Serial.print(F(" loudSum=")); Serial.print(loudSum);
    Serial.print(F(" state=")); Serial.print(state == STATE_ON ? F("ON") : F("OFF"));
    Serial.print(F(" isLoud=")); Serial.println(isLoud);

    bool canChange = ((uint32_t)(now - lastStateChangeMs) >= MIN_STATE_HOLD_MS);

    if (state == STATE_OFF) {
      if (canChange && loudSum >= ON_COUNT_THR) {
        state = STATE_ON;
        lastStateChangeMs = now;
        lastHeartbeatMs = now;
        Serial.println(F("*** STATE CHANGE: OFF -> ON ***"));
        sendEvent(1, p2p);
      }
    } else {
      if (canChange && loudSum <= OFF_COUNT_THR) {
        state = STATE_OFF;
        lastStateChangeMs = now;
        lastHeartbeatMs = now;
        Serial.println(F("*** STATE CHANGE: ON -> OFF ***"));
        sendEvent(2, p2p);
      }
    }

    if (ENABLE_HEARTBEAT) {
      uint32_t hbInterval = (state == STATE_ON) ? HEARTBEAT_ON_MS : HEARTBEAT_OFF_MS;
      if ((uint32_t)(now - lastHeartbeatMs) >= hbInterval) {
        lastHeartbeatMs = now;
        sendEvent(3, p2p); // now includes furnaceOn every time
      }
    }
  }
}
