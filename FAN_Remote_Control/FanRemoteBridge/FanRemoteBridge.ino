
#include "FanRemote.h"
#include "network_config.h"

//----------------------------------------------
// in network_config.h :
// const char* WIFI_SSID     = "YOUR_SSID";
// const char* WIFI_PASSWORD = "YOUR_PASS";
//----------------------------------------------

// ─── Wi-Fi setup ─────────────────────────────────────────────
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error "Board not ESP32/ESP8266 — adjust WiFi include"
#endif

#define DATA_PIN 3

#define MQTT_MAX_PACKET_SIZE 512
#include <PubSubClient.h>
WiFiClient net;  // WiFiClient is now known
PubSubClient mqtt(net);
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("Connecting to %s … ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
    if (millis() - t0 > 15000) {  // 15 s timeout
      Serial.println("\n⛔ Wi-Fi failed, rebooting");
      ESP.restart();
    }
  }
  Serial.printf("\n✅ Wi-Fi OK - IP: %s\n", WiFi.localIP().toString().c_str());
}

FanRemote fan(DATA_PIN);


void mqttCallback(char* topic, byte* payload, unsigned len) {
  static char cmd[64];  // adjust size if needed
  if (len >= sizeof(cmd)) len = sizeof(cmd) - 1;  // prevent overflow

  // copy payload into char buffer
  for (unsigned i = 0; i < len; i++) {
    cmd[i] = (char)payload[i];
  }
  cmd[len] = '\0';

  // lowercase in-place
  for (unsigned i = 0; i < len; i++) {
    cmd[i] = tolower((unsigned char)cmd[i]);
  }

  if (len < 2) return;

  char room = cmd[0];
  uint8_t btn = atoi(&cmd[1]);  // parse from second char onward

  switch (room) {
    case 'a': fan.send(FanRemote::ROOM_A, btn); break;
    case 'b': fan.send(FanRemote::ROOM_B, btn); break;
    case 'c': fan.send(FanRemote::ROOM_C, btn); break;
    case 'd': fan.send(FanRemote::ROOM_D, btn); break;
    default: Serial.println(F("⛔ MQTT unknown room"));
  }
}


void publishDiscovery() {
  // 4 rooms × 14 buttons
  static const char* roomPrefix[4] = { "a", "b", "c", "d" };
  static const char* roomName[4] = { "Room A", "Room B", "Room C", "Room D" };

  char topic[128];
  char payload[768];  // big enough for full JSON

  for (uint8_t r = 0; r < 4; ++r) {
    for (uint8_t b = 1; b <= 14; ++b) {

      // unique_id = fan_remote_<room><num>  e.g. fan_remote_a7
      char unique_id[24];
      snprintf(unique_id, sizeof unique_id, "fan_remote_%s%u", roomPrefix[r], b);

      // discovery topic
      snprintf(topic, sizeof topic, "homeassistant/button/%s/config", unique_id);

      // full-key payload (same schema as your working a1)
      int n = snprintf(payload, sizeof payload,
                       "{"
                       "\"name\":\"%s Button %u\","
                       "\"unique_id\":\"%s\","
                       "\"command_topic\":\"%s/cmd\","
                       "\"payload_press\":\"%s%u\","
                       "\"availability_topic\":\"%s/status\","
                       "\"payload_available\":\"online\","
                       "\"payload_not_available\":\"offline\","
                       "\"device\":{"
                       "\"identifiers\":[\"fan_remote\"],"
                       "\"name\":\"Fan Remote\","
                       "\"model\":\"ESP RF TX\","
                       "\"manufacturer\":\"YourName\","
                       "\"sw_version\":\"1.0\""
                       "}"
                       "}",
                       roomName[r], b,    // name
                       unique_id,         // unique_id
                       MQTT_BASE,         // command_topic base (fan_remote)
                       roomPrefix[r], b,  // payload_press: "a7" etc.
                       MQTT_BASE          // availability_topic base
      );

      if (n <= 0 || n >= (int)sizeof(payload)) {
        Serial.printf("❌ Truncated JSON for %s (%d bytes)\n", unique_id, n);
        continue;
      }

      bool ok = mqtt.publish(topic, payload, /*retain=*/true);
      Serial.printf("%s  (%dB)  %s\n", topic, n, ok ? "OK" : "FAIL");

      mqtt.loop();  // flush
      delay(5);
    }
  }

  Serial.println(F("✅ Published discovery for all buttons."));
}

void publishDiscoveryA7() {
  // Discovery topic uses fan_remote
  const char* topic =
    "homeassistant/button/fan_remote_a7/config";

  // Full keys (no abbreviations), device id = fan_remote
  const char* payload =
    "{"
    "\"name\":\"Room A Button 7\","
    "\"unique_id\":\"fan_remote_a7\","
    "\"command_topic\":\"" MQTT_BASE "/cmd\","
    "\"payload_press\":\"a7\","
    "\"availability_topic\":\"" MQTT_BASE "/status\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"device\":{"
    "\"identifiers\":[\"fan_remote\"],"
    "\"name\":\"Fan Remote\","
    "\"model\":\"ESP RF TX\","
    "\"manufacturer\":\"YourName\","
    "\"sw_version\":\"1.0\""
    "}"
    "}";

  bool ok = mqtt.publish(topic, payload, true);  // retain
  Serial.println(ok ? F("✅ Published discovery fan_remote_a1")
                    : F("❌ Publish failed"));
  mqtt.loop();
  delay(10);
}


void connectMQTT() {
  if (mqtt.connected()) return;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(1024);  // allow full discovery JSON

  while (!mqtt.connected()) {
    Serial.print(F("MQTT… "));
    bool ok = mqtt.connect(
      "fan-bridge",              // client id
      MQTT_USER, MQTT_PASSWORD,  // creds
      MQTT_BASE "/status",       // LWT topic  -> fan_remote/status
      0, true, "offline"         // LWT qos=0, retain=true, payload
    );

    if (ok) {
      Serial.println(F("connected"));

      // Birth/availability + subscription
      mqtt.publish(MQTT_BASE "/status", "online", true);  // retained
      mqtt.subscribe(MQTT_BASE "/cmd");

      // Publish discovery for a1 exactly once per boot
      static bool announced = false;
      if (!announced) {
        //  clearAllDiscovery();   // RUN ONCE, then remove/comment
        publishDiscoveryA7();
        publishDiscovery();  // sends to homeassistant
        announced = true;
      }

      // Give the client a moment to actually flush packets
      for (int i = 0; i < 5; ++i) {
        mqtt.loop();
        delay(10);
      }

    } else {
      Serial.printf("fail (%d)\n", mqtt.state());
      delay(2000);
    }
  }
}
void clearAllDiscovery() {
  char topic[128];
  for (char r : { 'a', 'b', 'c', 'd' }) {
    for (int b = 1; b <= 14; ++b) {
      snprintf(topic, sizeof(topic),
               "homeassistant/button/fan_remote_%c%d/config", r, b);
      mqtt.publish(topic, "", true);  // empty payload, retained → deletes
      mqtt.loop();
      delay(5);
    }
  }
  Serial.println(F("🧹 Cleared retained discovery topics."));
}


//-----------------

void setup() {
  Serial.begin(115200);
  connectWiFi();
  connectMQTT();

  Serial.println(F("\n--- RF Transmitter Ready ---"));
  Serial.println(F("Enter: a1–a14, b1–b14, c1–c14, or d1–d14"));
  // after debug I use Tx pin as data out
  if (DATA_PIN == 3) {
    Serial.end();  // Serial communication is now OFF.
    pinMode(DATA_PIN, OUTPUT);  
    pinMode(1, OUTPUT);  // Blinking led
    digitalWrite(DATA_PIN, LOW);
    
  }
}
/*
  This rewritten loop function maintains the core WiFi and MQTT connectivity
  while removing the serial port command handling.

  It now includes a non-blocking blink mechanism for GPIO1 (the TX pin on an ESP-01),
  causing it to toggle its state every 500 milliseconds.

  IMPORTANT: For this to work, you must add `pinMode(1, OUTPUT);` to your
  `setup()` function to configure GPIO1 as an output pin.
*/
void loop() {
  // --- Core connectivity logic (from original code) ---
  connectWiFi();
  connectMQTT();  // Handles auto-reconnection
  mqtt.loop();    // Keeps the MQTT client connection alive

  // --- Non-blocking blink logic for GPIO1 (TX pin) ---
  const long interval = 500; // The blink interval in milliseconds
  static unsigned long previousMillis = 0; // Stores the timestamp of the last state change
  static int ledState = LOW; // Stores the current state of the LED (LOW or HIGH)

  // Get the current time
  unsigned long currentMillis = millis();

  // Check if it's time to toggle the LED state
  if (currentMillis - previousMillis >= interval) {
    // Update the timestamp for the last state change
    previousMillis = currentMillis;

    // Toggle the LED state
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // Apply the new state to the GPIO pin
    digitalWrite(1, ledState);
  }
}

void loop_Serial() {
  connectWiFi();
  connectMQTT();  // auto-reconnect
  mqtt.loop();    // keep client alive
  static String cmd;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (!cmd.length()) break;

      char room = tolower(cmd[0]);
      uint8_t btn = cmd.substring(1).toInt();

      switch (room) {
        case 'a': fan.send(FanRemote::ROOM_A, btn); break;  
        case 'b': fan.send(FanRemote::ROOM_B, btn); break;  
        case 'c': fan.send(FanRemote::ROOM_C, btn); break;  
        case 'd': fan.send(FanRemote::ROOM_D, btn); break;  
        default: Serial.println(F("⛔ Unknown room"));
      }
      cmd = "";
    } else {
      cmd += c;
    }
  }
}
