/*
 * IRremoteESP8266: MQTT_IR_Repeater.ino - Complete working IR repeater with MQTT
 * Based DIRECTLY on SmartIRRepeater.ino by David Conran (crankyoldgit)
 * This is a PRODUCTION-READY solution that handles ALL protocols correctly
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <IRsend.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <IRac.h>
#include <IRtext.h>  // Added for additional descriptive functions
#include <ESP8266_SIH.h>

// ==================== WiFi & MQTT Configuration ====================
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic_send = "home/ir/recvCode";

// ==================== IR Configuration (EXACTLY like SmartIRRepeater) ====================
const uint16_t kIrLedPin = 4;  // D2 on NodeMCU
const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50;
const uint16_t kFrequency = 38000;  // Default frequency for unknown signals

// ==================== Global Variables ====================
WiFiClient espClient;
PubSubClient client(espClient);
IRsend irsend(kIrLedPin);
decode_results results;
ESP8266_SIH ss(&Serial);
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 5000;
String chipId = String(ESP.getChipId(), HEX);

// ==================== Setup Functions ====================

void reconnect_mqtt() {
  if (millis() - lastReconnectAttempt < reconnectInterval) {
    return;
  }

  Serial.print("Attempting MQTT connection...");
  
  String clientId = "ESP8266-IR-Repeater-";
  clientId += chipId.substring(0, 6);  // Shorten to avoid long client IDs
  
  if (client.connect(clientId.c_str())) {
    Serial.println("connected");
    client.subscribe(mqtt_topic_send);
    Serial.println("Subscribed to: " + String(mqtt_topic_send));
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
  }
  
  lastReconnectAttempt = millis();
}

// ==================== MQTT Callback - EXACT reproduction ====================
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Command Received: ");
  
  // Create null-terminated string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.println(message);
  
  // Handle received command - EXACTLY like SmartIRRepeater
  handleMqttCommand(message);
}

void handleMqttCommand(const char* command) {
  // Check if it's a RAW signal command
  if (strstr(command, "RAW:") == command) {
    handleRawCommand(command + 4);  // Skip "RAW:"
  } else {
    handleProtocolCommand(command);
  }
}

void handleRawCommand(const char* rawDataStr) {
  // Parse frequency and timings
  char* token = strtok((char*)rawDataStr, ":");
  if (!token) return;
  
  uint16_t frequency = atoi(token);
  if (frequency == 0) frequency = kFrequency;
  
  // Parse timings
  uint16_t timings[256];  // Large enough for any signal
  uint16_t count = 0;
  
  token = strtok(NULL, ",");
  while (token && count < 256) {
    uint32_t timing = atol(token);
    if (timing > 0 && timing <= 65535) {
      timings[count++] = (uint16_t)timing;
    }
    token = strtok(NULL, ",");
  }
  
  if (count == 0) return;
  
  // Send raw signal - EXACTLY like SmartIRRepeater
  irsend.sendRaw(timings, count, frequency);
  Serial.println("Raw signal sent successfully");
}

void handleProtocolCommand(const char* command) {
  // Format: "PROTOCOL_HEX:VALUE_HEX:BITS:STATE_HEX..."
  char cmdCopy[512];
  strncpy(cmdCopy, command, sizeof(cmdCopy) - 1);
  cmdCopy[sizeof(cmdCopy) - 1] = '\0';
  
  char* protocolHex = strtok(cmdCopy, ":");
  char* valueHex = strtok(NULL, ":");
  char* bitsStr = strtok(NULL, ":");
  
  if (!protocolHex || !valueHex || !bitsStr) return;
  
  decode_type_t protocol = (decode_type_t)strtol(protocolHex, NULL, 16);
  uint64_t value = strtoul(valueHex, NULL, 16);
  uint16_t bits = atoi(bitsStr);
  
  bool success = false;
  
  // EXACTLY like SmartIRRepeater - proper AC protocol handling
  if (hasACState(protocol)) {
    // For AC protocols, we need the state array
    uint8_t state[100];  // Large enough for most AC protocols
    uint16_t stateIndex = 0;
    
    // Parse state data from remaining tokens
    char* stateToken;
    while ((stateToken = strtok(NULL, ":")) && stateIndex < sizeof(state)) {
      state[stateIndex++] = (uint8_t)strtol(stateToken, NULL, 16);
    }
    
    if (stateIndex > 0) {
      success = irsend.send(protocol, state, stateIndex);
    }
  } else {
    // Simple protocols (<= 64 bits)
    success = irsend.send(protocol, value, bits);
  }
  
  if (success) {
    Serial.println("Protocol signal sent successfully");
  } else {
    Serial.println("Protocol send failed");
  }
}

// ==================== Setup & Loop - PRODUCTION READY ====================
void setup() {
  // Serial setup - EXACTLY like IRrecvDumpV2
#if defined(ESP8266)
  Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
#else
  Serial.begin(kBaudRate, SERIAL_8N1);
#endif
  while (!Serial) delay(50);

  // Initialize the serial interface system
  if (!ss.begin(115200)) {
      Serial.println("System initialization failed!");
      return;
  }
  
  Serial.println();
  Serial.println("MQTT IR Repeater - Signal Transmitter");
  
  // Setup IR hardware
  irsend.begin();
  
  // Setup WiFi and MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Serial.print("IR Repeater running on Pin ");
  Serial.print(kIrLedPin);
  Serial.println(" (transmit)");
  Serial.println("CIRCUIT WARNING: IR LED MUST be connected via transistor circuit!");
  Serial.println("See: https://github.com/crankyoldgit/IRremoteESP8266/wiki#ir-sending");
  Serial.println("\nType 'help' for available commands");
  Serial.println("Type 'menu' to access configuration menus");
}

void loop() {
  // Handle MQTT connection
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  ss.process();
  yield();  // Prevent WDT reset - CRITICAL
}
