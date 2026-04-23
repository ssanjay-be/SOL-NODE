/*
 ╔══════════════════════════════════════════════════════════════════╗
 ║  SOL-HEALTH NODE — ESP32 Firmware                               ║
 ║  Reads INA219 → computes DI locally → posts to Python backend   ║
 ║  File: firmware/sol_health_node.ino                             ║
 ╚══════════════════════════════════════════════════════════════════╝
*/

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_INA219.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// ── CONFIG (edit these) ──────────────────────────────────────────
#define WIFI_SSID       "YourWiFiSSID"
#define WIFI_PASS       "YourWiFiPassword"
#define BACKEND_URL     "https://your-backend.onrender.com"
#define NODE_ID         "SHN-001"
#define API_KEY         "your-api-key-here"

// ── PINS ─────────────────────────────────────────────────────────
#define PIN_MOSFET      25
#define PIN_LED_PWR     26
#define PIN_LED_STA     33
#define PIN_LED_WARN    32
#define PIN_LED_ERR     27
#define EEPROM_SIZE     64

// ── ALGORITHM CONSTANTS ──────────────────────────────────────────
#define N_BASELINE      40
#define SAMPLE_MS       200
#define POST_INTERVAL   5000    // post to backend every 5s
#define WARN_DI         15.0f
#define CRIT_DI         30.0f
#define PULSE_MS        100

// ── HARDWARE ─────────────────────────────────────────────────────
Adafruit_INA219  ina219;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// ── STATE ────────────────────────────────────────────────────────
struct Baseline { float V, I; bool ready; };
struct Reading  { float V, I, P, DI, health, Rint; };
struct Fault    { bool solar, batt, wire, load; String label; };

Baseline BL = {0, 0, false};
Reading  RD = {};
Fault    FT = {};

int   sampleCount = 0;
float sumV = 0, sumI = 0;
unsigned long lastSample = 0;
unsigned long lastPost   = 0;
bool blinkState = false;
unsigned long blinkTimer = 0;

// ════════════════════════════════════════════════════════════════
// EEPROM — persist baseline across power cycles
// ════════════════════════════════════════════════════════════════
void saveBaseline() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0,  BL.V);
  EEPROM.put(4,  BL.I);
  EEPROM.put(8,  (byte)1);   // ready flag
  EEPROM.commit();
  EEPROM.end();
}

bool loadBaseline() {
  EEPROM.begin(EEPROM_SIZE);
  byte flag; EEPROM.get(8, flag);
  if (flag != 1) { EEPROM.end(); return false; }
  EEPROM.get(0, BL.V);
  EEPROM.get(4, BL.I);
  EEPROM.end();
  BL.ready = true;
  return true;
}

void clearBaseline() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(8, (byte)0);
  EEPROM.commit(); EEPROM.end();
  BL = {0, 0, false};
  sampleCount = 0; sumV = 0; sumI = 0;
}

// ════════════════════════════════════════════════════════════════
// ALGORITHM MODULE 1 — Baseline Learner
// ════════════════════════════════════════════════════════════════
void collectBaseline() {
  sumV += RD.V;
  sumI += RD.I;
  sampleCount++;

  // OLED progress
  oled.clearDisplay();
  oled.fillRect(0,0,128,10,SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK); oled.setTextSize(1);
  oled.setCursor(14,1); oled.print("SOL-HEALTH NODE");
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,13); oled.print("LEARNING BASELINE");
  oled.setCursor(0,24); oled.print("V: "); oled.print(RD.V,2);
  oled.setCursor(0,33);
  oled.print("Samples: "); oled.print(sampleCount);
  oled.print("/"); oled.print(N_BASELINE);
  int prog = map(sampleCount,0,N_BASELINE,0,124);
  oled.drawRect(0,44,128,10,SSD1306_WHITE);
  oled.fillRect(2,46,prog,6,SSD1306_WHITE);
  oled.display();

  if (sampleCount >= N_BASELINE) {
    BL.V = sumV / N_BASELINE;
    BL.I = sumI / N_BASELINE;
    BL.ready = true;
    saveBaseline();
    Serial.printf("Baseline stored: V=%.3f I=%.3f\n", BL.V, BL.I);
  }
}

// ════════════════════════════════════════════════════════════════
// ALGORITHM MODULE 2 — DI Computer
// ════════════════════════════════════════════════════════════════
void computeDI() {
  float Vdev = (BL.V > 0.1f) ?
    fabsf(RD.V - BL.V) / BL.V * 100.0f : 0.0f;
  float Idev = (BL.I > 0.1f) ?
    fabsf(RD.I - BL.I) / BL.I * 100.0f : 0.0f;

  // Weighted formula — Patent Claim Core
  RD.DI     = constrain(0.6f*Vdev + 0.4f*Idev, 0.0f, 100.0f);
  RD.health = 100.0f - RD.DI;
}

// ════════════════════════════════════════════════════════════════
// ALGORITHM MODULE 3 — Fault Detector
// ════════════════════════════════════════════════════════════════
String classifyFault() {
  if (RD.DI >= CRIT_DI) return runPulseDiagnosis();
  if (RD.DI >= WARN_DI) return "WARNING";
  return "NORMAL";
}

// ════════════════════════════════════════════════════════════════
// ALGORITHM MODULE 4 — Pulse Analyser (R_int + recovery curve)
// ════════════════════════════════════════════════════════════════
String runPulseDiagnosis() {
  float V_before = ina219.getBusVoltage_V();
  float I_before = ina219.getCurrent_mA() / 1000.0f;

  // Fire MOSFET pulse
  digitalWrite(PIN_MOSFET, HIGH);
  delay(PULSE_MS);

  float V_during = ina219.getBusVoltage_V();
  float I_during = ina219.getCurrent_mA() / 1000.0f;
  float deltaV   = V_before - V_during;
  float Ip       = I_during;

  // R_int estimate
  RD.Rint = (Ip > 0.1f) ? (deltaV / Ip) * 1000.0f : 9999.0f;

  digitalWrite(PIN_MOSFET, LOW);
  delay(200);  // wait for recovery

  float V_after  = ina219.getBusVoltage_V();
  float I_after  = ina219.getCurrent_mA() / 1000.0f;
  float recoveryRate = (V_after - V_during) / 0.2f;  // V/s

  // Classify by recovery curve signature
  // Signature rules from patent claim:
  if (V_during < V_before * 0.15f && I_during < I_before * 0.15f
      && V_after  < V_before * 0.15f) {
    return "WIRE_FAULT";       // both V and I → 0 simultaneously
  }
  if (V_during < V_before * 0.15f && recoveryRate < 0.5f) {
    return "PANEL_FAULT";      // V collapses, slow recovery
  }
  if (RD.Rint > 300.0f) {
    return "BATTERY_FAULT";    // high R_int under pulse
  }
  if (I_during > I_before * 1.7f) {
    return "LOAD_FAULT";       // current spike, no recovery lag
  }
  return "DEGRADED";
}

// ════════════════════════════════════════════════════════════════
// ALGORITHM MODULE 5 — Alert Manager (LED + OLED)
// ════════════════════════════════════════════════════════════════
void updateLEDs(const String& fault) {
  digitalWrite(PIN_LED_PWR, HIGH);

  if (!BL.ready) {
    if (millis()-blinkTimer > 600) { blinkState=!blinkState; blinkTimer=millis(); }
    digitalWrite(PIN_LED_STA, blinkState);
    digitalWrite(PIN_LED_WARN, LOW);
    digitalWrite(PIN_LED_ERR, LOW);
    return;
  }

  bool isCrit = (fault != "NORMAL" && fault != "WARNING");
  bool isWarn = (fault == "WARNING");

  if (isCrit) {
    if (millis()-blinkTimer > 120) { blinkState=!blinkState; blinkTimer=millis(); }
    digitalWrite(PIN_LED_STA, LOW);
    digitalWrite(PIN_LED_WARN, HIGH);
    digitalWrite(PIN_LED_ERR, blinkState);
  } else if (isWarn) {
    if (millis()-blinkTimer > 500) { blinkState=!blinkState; blinkTimer=millis(); }
    digitalWrite(PIN_LED_STA, LOW);
    digitalWrite(PIN_LED_WARN, blinkState);
    digitalWrite(PIN_LED_ERR, LOW);
  } else {
    digitalWrite(PIN_LED_STA, HIGH);
    digitalWrite(PIN_LED_WARN, LOW);
    digitalWrite(PIN_LED_ERR, LOW);
  }
}

void updateOLED(const String& fault) {
  oled.clearDisplay();
  oled.fillRect(0,0,128,10,SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK); oled.setTextSize(1);
  oled.setCursor(14,1); oled.print("SOL-HEALTH NODE");
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(0,12);
  oled.print("V:"); oled.print(RD.V,2);
  oled.print("  I:"); oled.print(RD.I,2);
  oled.setCursor(0,22);
  oled.print("P:"); oled.print(RD.P,1);
  oled.print("W");
  oled.setCursor(0,32);
  oled.print("DI:"); oled.print(RD.DI,1); oled.print("%");
  int diBar = map((int)RD.DI,0,100,0,72);
  oled.drawRect(47,32,74,7,SSD1306_WHITE);
  oled.fillRect(47,32,diBar,7,SSD1306_WHITE);
  oled.setCursor(0,42);
  oled.print("HP:"); oled.print((int)RD.health); oled.print("%");
  oled.setCursor(0,53);
  if (fault == "NORMAL") oled.print("STATUS: NORMAL");
  else { oled.print("!! "); oled.print(fault); oled.print(" !!"); }
  oled.display();
}

// ════════════════════════════════════════════════════════════════
// NETWORK — Post reading to Python backend
// ════════════════════════════════════════════════════════════════
void postReading(const String& fault) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(BACKEND_URL) + "/api/readings");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);

  StaticJsonDocument<256> doc;
  doc["node_id"]    = NODE_ID;
  doc["voltage"]    = RD.V;
  doc["current"]    = RD.I;
  doc["power"]      = RD.P;
  doc["di"]         = RD.DI;
  doc["health"]     = RD.health;
  doc["fault_type"] = fault;
  doc["r_int"]      = RD.Rint;
  doc["v_baseline"] = BL.V;
  doc["i_baseline"] = BL.I;

  String body;
  serializeJson(doc, body);

  int code = http.POST(body);
  Serial.printf("POST /readings → %d\n", code);
  http.end();

  // Check for commands from backend
  http.begin(String(BACKEND_URL) + "/api/commands/" + NODE_ID);
  http.addHeader("X-API-Key", API_KEY);
  int getCode = http.GET();
  if (getCode == 200) {
    String resp = http.getString();
    StaticJsonDocument<128> cmd;
    deserializeJson(cmd, resp);
    String command = cmd["command"] | "";
    if (command == "reset_baseline") {
      clearBaseline();
      Serial.println("Baseline reset by backend command");
    }
  }
  http.end();
}

// ════════════════════════════════════════════════════════════════
// Serial Plotter output (Wokwi simulation)
// ════════════════════════════════════════════════════════════════
void serialPlot(const String& fault) {
  Serial.print("Voltage:"); Serial.print(RD.V,3); Serial.print("\t");
  Serial.print("Current:"); Serial.print(RD.I,3); Serial.print("\t");
  Serial.print("DI:");      Serial.print(RD.DI,3); Serial.print("\t");
  Serial.print("Health_div10:");
  Serial.print(RD.health/10.0f,3); Serial.print("\t");
  Serial.print("Baseline_V:");
  Serial.print(BL.ready ? BL.V : 0.0f, 3); Serial.print("\t");
  Serial.print("WARN:"); Serial.print(1.5f); Serial.print("\t");
  Serial.print("CRIT:"); Serial.println(3.0f);
}

// ════════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  pinMode(PIN_MOSFET,   OUTPUT); digitalWrite(PIN_MOSFET, LOW);
  pinMode(PIN_LED_PWR,  OUTPUT);
  pinMode(PIN_LED_STA,  OUTPUT);
  pinMode(PIN_LED_WARN, OUTPUT);
  pinMode(PIN_LED_ERR,  OUTPUT);

  Wire.begin(21, 22);

  if (!ina219.begin()) Serial.println("INA219 not found!");
  else Serial.println("INA219 OK");

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    Serial.println("OLED not found!");

  oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2); oled.setCursor(4,4);
  oled.print("SOL-NODE"); oled.setTextSize(1);
  oled.setCursor(10,26); oled.print("M. Kumarasamy CE");
  oled.setCursor(10,38); oled.print("Connecting WiFi...");
  oled.display();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); tries++;
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  else
    Serial.println("\nWiFi FAILED — running offline");

  // Try to load saved baseline
  if (loadBaseline())
    Serial.printf("Baseline loaded: V=%.3f I=%.3f\n", BL.V, BL.I);

  digitalWrite(PIN_LED_PWR, HIGH);
}

// ════════════════════════════════════════════════════════════════
// MAIN LOOP
// ════════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();
  if (now - lastSample < SAMPLE_MS) { updateLEDs(""); return; }
  lastSample = now;

  // Read INA219
  RD.V = ina219.getBusVoltage_V();
  RD.I = ina219.getCurrent_mA() / 1000.0f;
  RD.P = RD.V * RD.I;

  if (!BL.ready) {
    collectBaseline();
    updateLEDs("LEARNING");
    return;
  }

  // Run algorithm
  computeDI();
  String fault = classifyFault();

  // Output
  serialPlot(fault);
  updateLEDs(fault);
  updateOLED(fault);

  // Post to backend every 5s
  if (now - lastPost >= POST_INTERVAL) {
    postReading(fault);
    lastPost = now;
  }
}
