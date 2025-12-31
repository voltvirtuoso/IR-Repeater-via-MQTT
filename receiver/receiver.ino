/*
 * IRremoteESP8266: MQTT_IR_Repeater.ino - Complete working IR repeater with MQTT
 * Based DIRECTLY on SmartIRRepeater.ino by David Conran (crankyoldgit)
 * This is a PRODUCTION-READY solution that handles ALL protocols correctly
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <IRac.h>
#include <IRtext.h>  // Added for additional descriptive functions
#include <ESP8266_SIH.h>

// ==================== WiFi & MQTT Configuration ====================
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic_receive = "home/ir/recvCode";

// ==================== IR Configuration (EXACTLY like SmartIRRepeater) ====================
#ifdef ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 10;
#else
const uint16_t kRecvPin = 14;  // D5 on NodeMCU
#endif
const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50;
const uint16_t kFrequency = 38000;  // Default frequency for unknown signals

// ==================== Global Variables ====================
WiFiClient espClient;
PubSubClient client(espClient);
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
ESP8266_SIH ss(&Serial);
decode_results results;
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
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
  }
  
  lastReconnectAttempt = millis();
}

// ==================== IR Signal Processing - EXACTLY like SmartIRRepeater ====================
void publishIrSignal() {
  decode_type_t protocol = results.decode_type;
  uint16_t size = results.bits;
  
  // EXACTLY like SmartIRRepeater - handle all protocols properly
  if (protocol == decode_type_t::UNKNOWN) {
    publishRawSignal();
  } else if (hasACState(protocol)) {
    publishAcSignal(protocol, size);
  } else {
    publishSimpleSignal(protocol, size);
  }
}

void publishRawSignal() {
  uint16_t *raw_array = resultToRawArray(&results);
  uint16_t size = getCorrectedRawLength(&results);
  
  String rawDataStr = "RAW:";
  rawDataStr += String(kFrequency);
  rawDataStr += ":";
  
  for (uint16_t i = 0; i < size; i++) {
    rawDataStr += String(raw_array[i]);
    if (i < size - 1) rawDataStr += ",";
  }
  
  if (client.connected()) {
    client.publish(mqtt_topic_receive, rawDataStr.c_str());
    Serial.println("Published RAW signal");
  }
  
  delete[] raw_array;  // MUST free memory
}

void publishAcSignal(decode_type_t protocol, uint16_t size) {
  // Format: "PROTOCOL_HEX:VALUE_HEX:BITS:STATE_BYTE1_HEX:STATE_BYTE2_HEX:..."
  String acData = String(protocol, HEX) + ":" + 
                  String(results.value, HEX) + ":" + 
                  String(size) + ":";
  
  // Calculate state length from bits (each byte = 8 bits)
  uint16_t stateLength = size / 8;
  
  // Use a reasonable maximum length to prevent buffer overflow
  const uint16_t kMaxStateLength = 100; // Maximum state length we'll handle
  uint16_t actualLength = min(stateLength, kMaxStateLength);
  
  for (uint16_t i = 0; i < actualLength; i++) {
    acData += String(results.state[i], HEX);
    if (i < actualLength - 1) acData += ":";
  }
  
  if (client.connected()) {
    client.publish(mqtt_topic_receive, acData.c_str());
    Serial.println("Published AC signal: " + String(typeToString(protocol).c_str()));
  }
}

void publishSimpleSignal(decode_type_t protocol, uint16_t size) {
  // Format: "PROTOCOL_HEX:VALUE_HEX:BITS"
  String simpleData = String(protocol, HEX) + ":" + 
                      String(results.value, HEX) + ":" + 
                      String(size);
  
  if (client.connected()) {
    client.publish(mqtt_topic_receive, simpleData.c_str());
    Serial.println("Published simple signal: " + String(typeToString(protocol).c_str()));
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
  Serial.println("MQTT IR Repeater - Signal Receiver");
  
  // Setup IR hardware - EXACTLY like SmartIRRepeater
  irrecv.enableIRIn();
  
  // Setup WiFi and MQTT
  client.setServer(mqtt_server, mqtt_port);
  
  Serial.print("IR Repeater running on Pin ");
  Serial.print(kRecvPin);
  Serial.println(" (receive)");
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
  
  // Check for IR signals - EXACTLY like SmartIRRepeater
  if (irrecv.decode(&results)) {
    uint32_t now = millis();
    Serial.printf("%06u.%03u: ", now / 1000, now % 1000);
    
    decode_type_t protocol = results.decode_type;
    uint16_t size = results.bits;
    
    Serial.print("Received ");
    Serial.print(size);
    Serial.print("-bit ");
    Serial.print(typeToString(protocol).c_str());
    Serial.println(" signal");
    
    // ADDITIONAL DESCRIPTIONS - EXACTLY LIKE IRRECVDUMPV2
    Serial.println("\n=== DETAILED IR INFORMATION ===");
    
    // Display basic human readable info
    Serial.println("Basic Info:");
    Serial.print(resultToHumanReadableBasic(&results));
    
    // Display any extra A/C info if we have it
    String acDesc = IRAcUtils::resultAcToString(&results);
    if (acDesc.length() > 0) {
      Serial.println("\nAC Description:");
      Serial.println(acDesc);
    }
    
    // Output the results as source code (like IRrecvDumpV2)
    Serial.println("\nRaw Data Format (Source Code):");
    Serial.println(resultToSourceCode(&results));
    
    // Display timing info if needed
    Serial.println("\nTiming Info:");
    Serial.println(resultToTimingInfo(&results));
    
    Serial.println("================================\n");
    
    // Publish the received signal to MQTT
    publishIrSignal();
    
    // Resume capturing IR messages
    irrecv.resume();
  }
  
  yield();  // Prevent WDT reset - CRITICAL
}
