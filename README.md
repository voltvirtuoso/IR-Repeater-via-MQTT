# IR-Repeater-via-MQTT

A complete, production-ready IR repeater system that captures infrared signals from remotes and retransmits them via MQTT. This solution handles ALL IR protocols correctly, including complex air conditioner protocols and raw signals.

## Table of Contents
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Circuit Diagrams](#circuit-diagrams)
- [Software Dependencies](#software-dependencies)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [MQTT Message Format](#mqtt-message-format)
- [Adding Custom Commands](#adding-custom-commands)
- [Creating New Menu Options](#creating-new-menu-options)
- [Troubleshooting](#troubleshooting)
- [Credits](#credits)
- [License](#license)

## Features

- **Complete Protocol Support**: Handles all IR protocols including RAW signals, AC protocols, and standard remote controls
- **Production-Ready Design**: Based directly on David Conran's SmartIRRepeater implementation
- **Dual-Mode Operation**: Separate receiver and transmitter modules for flexible deployment
- **Detailed Debugging**: Comprehensive signal analysis with human-readable output
- **Interactive Serial Interface**: Menu-driven configuration with help system
- **Auto-Reconnection Logic**: Robust MQTT and WiFi reconnection handling
- **Memory Management**: Proper cleanup of dynamically allocated memory
- **Watchdog Protection**: Prevents system lockups with yield() calls

## Hardware Requirements

### Receiver Module:
- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- IR Receiver Module (TSOP38238 or equivalent 38kHz receiver)
- Jumper wires
- Breadboard (optional)

### Transmitter Module:
- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- IR LED (940nm wavelength)
- NPN Transistor (2N3904, BC547, or equivalent)
- 100Ω Resistor (for base)
- 22Ω Resistor (for collector)
- Breadboard and jumper wires

## Circuit Diagrams

### IR Receiver Circuit:
```
ESP8266 (NodeMCU)       IR Receiver Module
     D5 (GPIO14)  -----  Signal (OUT)
     3.3V         -----  VCC
     GND          -----  GND
```

### IR Transmitter Circuit:
```
ESP8266 (NodeMCU)       Transistor Circuit
     D2 (GPIO4)   -----  100Ω Resistor ----- Base (B)
                              |
     3.3V         -----  22Ω Resistor ----- Collector (C) ----- IR LED Anode (+)
                              |                  |
     GND          --------------------------  IR LED Cathode (-) and Emitter (E)
```

> **WARNING**: The IR LED MUST be connected via a transistor circuit to prevent damage to the ESP8266 GPIO pins. See [IR Sending Circuit Reference](https://github.com/crankyoldgit/IRremoteESP8266/wiki#ir-sending) for complete details.

## Software Dependencies

- [Arduino IDE](https://www.arduino.cc/en/software) (v1.8.19 or later)
- [ESP8266 Board Support](https://github.com/esp8266/Arduino) (v3.0.0 or later)
- [IRremoteESP8266 Library](https://github.com/crankyoldgit/IRremoteESP8266) (v2.8.6 or later)
- [PubSubClient Library](https://github.com/knolleary/pubsubclient) (v2.8.0 or later)
- [ESP8266_SIH Library](https://github.com/voltvirtuoso/SerialInterfaceHandler) (for serial interface)

### Library Installation:
1. Open Arduino IDE
2. Navigate to **Sketch** > **Include Library** > **Manage Libraries**
3. Search and install:
   - `IRremoteESP8266`
   - `PubSubClient`
   - `ESP8266_SIH` (or clone from repository)

## Installation

### Repository Setup:
```bash
git clone https://github.com/voltvirtuoso/IR-Repeater-via-MQTT.git
cd IR-Repeater-via-MQTT
```

### Arduino IDE Setup:
1. Open Arduino IDE
2. Go to **File** > **Preferences**
3. Add ESP8266 board manager URL: `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
4. Go to **Tools** > **Board** > **Boards Manager**
5. Search for "esp8266" and install
6. Select your board under **Tools** > **Board** (e.g., "NodeMCU 1.0")

### Uploading Sketches:
1. For **Receiver**:
   - Open `receiver/receiver.ino`
   - Configure WiFi credentials (see Configuration section below)
   - Upload to first ESP8266 module

2. For **Transmitter**:
   - Open `transmitter/transmitter.ino`
   - Configure WiFi credentials (see Configuration section below)
   - Upload to second ESP8266 module

## Configuration

### WiFi and MQTT Setup:
Edit these constants at the top of both sketches:

```cpp
// MQTT Configuration
const char* mqtt_server = "broker.hivemq.com";  // Public MQTT broker
const int mqtt_port = 1883;
const char* mqtt_topic_receive = "home/ir/recvCode";  // Topic for receiver->transmitter

// For transmitter only:
const char* mqtt_topic_send = "home/ir/recvCode";  // Same topic for subscription
```

### IR Pin Configuration:

**Receiver** (receiver.ino):
```cpp
const uint16_t kRecvPin = 14;  // D5 on NodeMCU
```

**Transmitter** (transmitter.ino):
```cpp
const uint16_t kIrLedPin = 4;  // D2 on NodeMCU
```

### ESP8266_SIH Configuration:
Both sketches use the Serial Interface Handler for configuration menus. To customize:

```cpp
// In setup() function:
if (!ss.begin(115200)) {
    Serial.println("System initialization failed!");
    return;
}
ss.setTimeout(180000);  // Set timeout to 3 minutes (optional)
```

## Usage

### Basic Operation:
1. Power on both ESP8266 modules
2. Point your IR remote at the receiver module
3. The receiver will capture the signal and publish it to MQTT
4. The transmitter will receive the MQTT message and reproduce the IR signal
5. Your IR-controlled device should respond as if the original remote was used

### Serial Interface Commands:
Both modules provide an interactive serial interface. Connect via Serial Monitor (115200 baud) and use these commands:

```
help          - Show detailed help information
?             - Quick help summary
menu          - Access configuration menus
status        - Show system status
wifi scan     - Scan for WiFi networks
wifi connect  - Connect to WiFi network
creds list    - List stored WiFi credentials
reconnect     - Force MQTT reconnection
```

### Menu Navigation:
1. Type `menu` to access the main menu
2. Use number keys to select options:
   - **1. WiFi Configuration** - Set up WiFi connection
   - **2. Credentials Management** - Manage stored networks
   - **3. System Status** - View system information
   - **4. Exit Menu** - Return to command mode
3. Press `0` at any time to exit the current menu

## MQTT Message Format

The system uses three different message formats depending on the signal type:

### 1. RAW Signals:
```
RAW:38000:9000,4500,560,1690,560,1690,560,560,560,560...
```
Format: `RAW:<frequency>:<timing1>,<timing2>,<timing3>...`

### 2. Simple Protocol Signals:
```
1a:12345678:32
```
Format: `<protocol_hex>:<value_hex>:<bits>`

### 3. AC Protocol Signals:
```
1a:12345678:128:a1b2c3d4e5f6...
```
Format: `<protocol_hex>:<value_hex>:<bits>:<state_byte1_hex>:<state_byte2_hex>:...`

## Adding Custom Commands

The system uses the ESP8266_SIH library for command handling. To add new commands:

### Register a Command with MQTT Integration
For the transmitter, add a command to send specific IR codes:

```cpp
ss.registerCommand("sendtv", 
  [](const std::vector<String>& args) {
    // Send TV power command
    uint64_t tvPowerCode = 0x7F90EF10;  // Example Samsung TV power code
    irsend.send(NEC, tvPowerCode, 32);
    Serial.println("TV power command sent!");
    
    // Optional: Publish to MQTT
    String command = "1a:" + String(tvPowerCode, HEX) + ":32";
    if (client.connected()) {
      client.publish(mqtt_topic_send, command.c_str());
      Serial.println("Command also published to MQTT");
    }
  },
  "Send TV power command",
  "sendtv",
  "device"
);
```

### Register a WiFi Management Command
For advanced WiFi management:

```cpp
ss.registerCommand("scanall", 
  [](const std::vector<String>& args) {
    Serial.println("Scanning for ALL WiFi networks...");
    int n = WiFi.scanNetworks(true, true); // async, show hidden
    
    if (n == 0) {
      Serial.println("No networks found");
      return;
    }
    
    Serial.printf("%d networks found:\n", n);
    for (int i = 0; i < n; ++i) {
      Serial.printf("%d. %s (%d dBm) - %s\n",
        i + 1,
        WiFi.SSID(i).c_str(),
        WiFi.RSSI(i),
        WiFi.encryptionType(i) == ENC_TYPE_NONE ? "OPEN" : "SECURED"
      );
      delay(10);
    }
  },
  "Scan all WiFi networks including hidden ones",
  "scanall",
  "wifi"
);
```

## Creating New Menu Options

To add custom menu items to the configuration interface:

### Step 1: Define Your Menu Action Function
Add this to your sketch (outside setup/loop):

```cpp
void showCustomMenu() {
  ss._menuSystem.clearScreen();
  ss._menuSystem.showHeader("Custom Device Control");
  
  Serial.println("Custom device control options:");
  Serial.println("1. Control LED");
  Serial.println("2. Read Sensors");
  Serial.println("3. Back to Main Menu");
  
  ss._menuSystem.showFooter();
  Serial.print("\nSelect option (1-3): ");
}

void handleLEDControl() {
  ss._menuSystem.clearScreen();
  ss._menuSystem.showHeader("LED Control");
  
  String state = ss._menuSystem.getUserInput("Enter LED state (on/off)");
  if (state.equalsIgnoreCase("on")) {
    digitalWrite(LED_BUILTIN, LOW);  // ESP8266 LED is active low
    Serial.println("LED turned ON");
  } else if (state.equalsIgnoreCase("off")) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("LED turned OFF");
  } else {
    Serial.println("Invalid state");
  }
  
  delay(1000);
  showCustomMenu();
}
```

### Step 2: Add Menu Items to Initialization
In the `setup()` function after `ss.begin()`:

```cpp
// Add custom menu items
ss._menuSystem._mainMenuItems.push_back({
  5,  // ID (must be unique)
  "Device Control", 
  []() { showCustomMenu(); }, 
  "Control custom devices", 
  true  // isEnabled
});

// Add submenu items
std::vector<MenuSystem::MenuItem> customItems = {
  {1, "LED Control", []() { handleLEDControl(); }, "Control built-in LED", true},
  {2, "Sensor Readout", []() { /* your sensor code */ }, "Read sensor data", true},
  {3, "Back to Main", []() { ss._menuSystem.showMenu(MenuSystem::MAIN); }, "Return to main menu", true}
};

// Replace existing menu items or add to a new menu type
ss._menuSystem._customMenuItems = customItems;
```

### Step 3: Handle Menu Input
Add this to your `loop()` function or create a custom handler:

```cpp
// Custom menu handler
if (ss._menuSystem.isInMenu() && ss._menuSystem._currentMenu == MenuSystem::CUSTOM) {
  // Your custom menu handling logic here
}
```

## Troubleshooting

### Common Issues and Solutions:

| Issue | Solution |
|-------|----------|
| **IR signals not captured** | Check receiver circuit connections, ensure proper power supply, verify pin configuration |
| **IR signals not transmitted** | Verify transistor circuit is correct, check IR LED polarity, ensure sufficient current |
| **MQTT connection fails** | Check WiFi credentials, verify MQTT broker is accessible, check firewall settings |
| **System crashes frequently** | Reduce IR capture buffer size, add more delay() calls, check power supply stability |
| **Serial interface unresponsive** | Reset module, check baud rate settings, ensure no pin conflicts |
| **AC protocols not working** | Verify state array length, check protocol support in library version |

### Debugging Tips:

1. **Enable verbose logging**:
   ```cpp
   #define DEBUG_IR 1
   ```

2. **Check memory usage**:
   ```cpp
   Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
   ```

3. **Test IR receiver independently**:
   ```cpp
   if (irrecv.decode(&results)) {
     Serial.println(resultToHumanReadableBasic(&results));
     irrecv.resume();
   }
   ```

4. **Test IR transmitter independently**:
   ```cpp
   irsend.sendNEC(0x7F90EF10, 32);  // Test with known working code
   ```

## Credits

- **David Conran (crankyoldgit)** - Author of [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) library and SmartIRRepeater implementation
- **Nick O'Leary** - Author of [PubSubClient](https://github.com/knolleary/pubsubclient) MQTT library
- **Haroon Raza** - Author of [ESP8266_SIH](https://github.com/voltvirtuoso/SerialInterfaceHandler) serial interface library
- **ESP8266 Arduino Core Team** - Platform support

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

