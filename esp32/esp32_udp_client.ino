/*
 * ESP32 UDP Client for Windows Hotspot Key Broadcast System
 * 
 * Functions:
 * 1. Connects to Windows Mobile Hotspot (2.4 GHz)
 * 2. Periodically sends Hello registration packet (every 5s)
 * 3. Listens on UDP Port 4210 for "set_key" broadcasts
 * 4. Deduplicates message_id to prevent re-applying identical keys
 * 5. Returns "key_ack" directly to sender IP and Port (udp.remoteIP(), udp.remotePort())
 * 
 * Dependencies:
 * - ArduinoJson (Library Manager: ArduinoJson by Benoit Blanchon, v6.x or v7.x)
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// ==================== Configuration ====================
const char* WIFI_SSID     = "ESP32_Host";
const char* WIFI_PASS     = "12345678";
const uint16_t UDP_PORT   = 4210;
const char* FIRMWARE_VER  = "1.0.0";

// Custom Device ID for this ESP32 (Change for each ESP32 board or leave empty to let MAC dictate)
String DEVICE_ID          = "ESP32-001";

// Status LED (GPIO 2 on most ESP32 DevKit boards)
#define STATUS_LED_PIN 2

// ==================== Global State ====================
WiFiUDP udp;
String deviceMac;
String lastMessageId = "";
String storedChunkKey = "";
unsigned long lastHelloTime = 0;
const unsigned long HELLO_INTERVAL_MS = 5000;

// ==================== Helper Functions ====================
void blinkLed(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(STATUS_LED_PIN, LOW);
    if (i < times - 1) delay(delayMs);
  }
}

void sendHello() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Destination: broadcast or subnet broadcast
  IPAddress broadcastIp = WiFi.broadcastIP();
  if (broadcastIp == IPAddress(0, 0, 0, 0)) {
    broadcastIp = IPAddress(255, 255, 255, 255);
  }

  StaticJsonDocument<256> doc;
  doc["protocol"] = "esp32-control";
  doc["version"]  = 1;
  doc["type"]     = "hello";
  doc["mac"]      = deviceMac;
  doc["id"]       = DEVICE_ID;
  doc["firmware"] = FIRMWARE_VER;

  String jsonStr;
  serializeJson(doc, jsonStr);

  udp.beginPacket(broadcastIp, UDP_PORT);
  udp.print(jsonStr);
  udp.endPacket();

  Serial.printf("[UDP] Sent Hello to %s:%d -> %s\n", broadcastIp.toString().c_str(), UDP_PORT, jsonStr.c_str());
}

void sendAck(IPAddress targetIp, uint16_t targetPort, const String& msgId, bool success, const String& statusMsg, const String& errCode = "") {
  StaticJsonDocument<384> doc;
  doc["protocol"]   = "esp32-control";
  doc["version"]    = 1;
  doc["type"]       = "key_ack";
  doc["message_id"] = msgId;
  doc["mac"]        = deviceMac;
  doc["id"]         = DEVICE_ID;
  doc["status"]     = success ? "ok" : "error";
  doc["message"]    = statusMsg;
  if (!success && errCode.length() > 0) {
    doc["error_code"] = errCode;
  }

  String jsonStr;
  serializeJson(doc, jsonStr);

  udp.beginPacket(targetIp, targetPort);
  udp.print(jsonStr);
  udp.endPacket();

  Serial.printf("[UDP] Sent ACK to %s:%d (Status: %s) -> %s\n", 
                targetIp.toString().c_str(), targetPort, success ? "OK" : "ERROR", jsonStr.c_str());
  
  // Blink 2 quick pulses on ACK sent
  blinkLed(2, 60);
}

void processIncomingPacket(char* buffer, int len, IPAddress remoteIp, uint16_t remotePort) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, buffer, len);

  if (error) {
    Serial.printf("[UDP] JSON parse error: %s\n", error.c_str());
    return;
  }

  const char* ptype = doc["type"];
  if (!ptype) return;

  if (strcmp(ptype, "set_key") == 0) {
    const char* msgId = doc["message_id"];
    const char* key   = doc["chunk_key"];
    const char* target = doc["target"] | "all";

    if (!msgId || !key) {
      sendAck(remoteIp, remotePort, msgId ? msgId : "", false, "Missing message_id or chunk_key", "INVALID_FORMAT");
      return;
    }

    // Target filtering: must be "all" or match our DEVICE_ID
    if (strcmp(target, "all") != 0 && strcmp(target, DEVICE_ID.c_str()) != 0) {
      Serial.printf("[UDP] Ignoring key for different target: %s\n", target);
      return;
    }

    // Check message_id deduplication
    if (lastMessageId == String(msgId)) {
      Serial.printf("[UDP] Duplicate message_id [%s] detected. Re-sending ACK without overwriting.\n", msgId);
      sendAck(remoteIp, remotePort, msgId, true, "key already received (duplicate)");
      return;
    }

    // Save key
    storedChunkKey = String(key);
    lastMessageId  = String(msgId);
    Serial.printf("[KEY] *** Successfully updated chunk_key: [%s] (MsgID: %s) ***\n", storedChunkKey.c_str(), msgId);

    // Reply ACK directly to packet sender's IP and Port
    sendAck(remoteIp, remotePort, msgId, true, "key received");
  }
}

// ==================== Setup & Loop ====================
void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  delay(500);
  Serial.println("\n\n========================================");
  Serial.println("  ESP32 Hotspot Key Receiver Starting   ");
  Serial.println("========================================");

  // Initialize Wi-Fi in Station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  // Get MAC Address formatted
  deviceMac = WiFi.macAddress();
  deviceMac.toUpperCase();
  Serial.printf("[WIFI] Device MAC: %s\n", deviceMac.c_str());

  // Connect to Windows Hotspot
  Serial.printf("[WIFI] Connecting to SSID: %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    Serial.println("\n[WIFI] Connected successfully!");
    Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("[WIFI] Broadcast IP: %s\n", WiFi.broadcastIP().toString().c_str());

    // Start UDP listening on port 4210
    udp.begin(UDP_PORT);
    Serial.printf("[UDP] Listening on Port %d\n", UDP_PORT);

    // Send initial Hello packet immediately
    sendHello();
    lastHelloTime = millis();
  } else {
    Serial.println("\n[WIFI] Connection failed. Will retry in loop.");
  }
}

void loop() {
  // Reconnect if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(STATUS_LED_PIN, LOW);
    Serial.println("[WIFI] Disconnected! Reconnecting...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // Periodic Hello Heartbeat (every 5 seconds)
  if (millis() - lastHelloTime >= HELLO_INTERVAL_MS) {
    sendHello();
    lastHelloTime = millis();
  }

  // Check incoming UDP packets
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char packetBuffer[512];
    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    if (len > 0) {
      packetBuffer[len] = '\0';
      IPAddress remoteIp = udp.remoteIP();
      uint16_t remotePort = udp.remotePort();

      Serial.printf("[UDP] Received %d bytes from %s:%d: %s\n", 
                    len, remoteIp.toString().c_str(), remotePort, packetBuffer);

      processIncomingPacket(packetBuffer, len, remoteIp, remotePort);
    }
  }

  delay(10);
}
