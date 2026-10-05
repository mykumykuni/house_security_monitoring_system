# ESP32 Home Security Monitoring System

A local-network ESP32 sensor monitor with a browser dashboard. The firmware reads motion, proximity, temperature, and humidity; controls an alarm LED; and exposes an HTTP API. The dashboard polls that API and shows current readings and browser-side alerts.

This is a learning/prototyping system, not a certified alarm product. Review [Safety and Limitations](#safety-and-limitations) before wiring or relying on it for security.

## Contents

- [How It Works](#how-it-works)
- [Parts and Software](#parts-and-software)
- [Wiring Manual](#wiring-manual)
- [Step-by-Step Setup](#step-by-step-setup)
- [Using the System](#using-the-system)
- [HTTP API](#http-api)
- [Changing Settings](#changing-settings)
- [Troubleshooting](#troubleshooting)
- [Safety and Limitations](#safety-and-limitations)

## Features

- Live motion state from a PIR sensor.
- IR proximity/object detection and an object count supplied by the ESP32.
- Temperature and relative humidity readings from a DHT11 sensor.
- Alarm LED state.
- Alerts for motion, object detection/count changes, sensor read errors, and temperature or humidity thresholds.
- An in-page alert banner, temporary notifications, an alert log, optional audible beeps, and vibration where the browser/device supports it.
- ESP32 online/offline status and reported uptime.

## How It Works

The sketch in [`firmware/esp32_home_monitoring.ino`](firmware/esp32_home_monitoring.ino) continuously reads the digital sensor pins, samples the DHT11 every two seconds, and serves HTTP on port 80. The static dashboard in [`data/`](data/) polls `GET /api/status` every second.

Firmware security modes control the alarm LED and latched alarm. Dashboard alerts are separate browser-side notifications for sensor transitions, DHT11 errors, and temperature/humidity limits; they can occur in every firmware mode. The dashboard currently displays alarm status but does not provide mode controls.

## Parts and Software

### Hardware

- ESP32 development board with Wi-Fi.
- Digital-output PIR motion sensor.
- FC-51 IR obstacle/proximity sensor module.
- DHT11 temperature/humidity sensor or module.
- LED with a current-limiting resistor (typically 220-1,000 ohms), unless using an appropriate LED module.
- Breadboard, jumper wires, and USB cable/power source.

### Software

- Arduino IDE or Arduino CLI and the ESP32 board platform for your exact board.
- Arduino DHT sensor library and any dependencies it requests.
- A compatible `qrcode.h` library/component that provides `esp_qrcode_config_t`, `esp_qrcode_generate`, and `esp_qrcode_print_console`. QR libraries are not interchangeable; the sketch needs this specific API. If the selected Arduino core does not provide it, install a compatible component or remove the optional QR-printing code.
- Python 3 (or another static HTTP server) and a modern browser with JavaScript enabled.
- A local Wi-Fi network that the ESP32 and dashboard device can both reach. The ESP32 normally requires a 2.4 GHz network.

Install the board platform and libraries using the instructions for your board and Arduino IDE version. Compile the sketch before mounting the circuit permanently; QR-code integration can vary between Arduino-ESP32 versions.

## Wiring Manual

Disconnect USB and other power before wiring. Connect every sensor ground to ESP32 GND. Pin numbers below are ESP32 GPIO numbers, not physical header positions; use the pinout for your exact board.

| Component pin | Connect to | Notes |
| --- | --- | --- |
| DHT11 VCC | Sensor supply allowed by its datasheet/module | Check whether yours is a bare sensor or a breakout module. |
| DHT11 GND | ESP32 GND | Common ground. |
| DHT11 DATA/SIG | GPIO 4 | `DHT_PIN`; a bare DHT11 may need a pull-up resistor from DATA to VCC; many modules include one. |
| PIR VCC | Supply allowed by PIR module | Check its rated voltage. |
| PIR GND | ESP32 GND | Common ground. |
| PIR OUT | GPIO 13 | `PIR_PIN`, configured as `INPUT`. |
| FC-51 VCC | Supply allowed by module | Check its rated voltage and output behavior. |
| FC-51 GND | ESP32 GND | Common ground. |
| FC-51 DO/OUT | GPIO 27 | `FC51_PIN`, configured with `INPUT_PULLUP`; detection is LOW. |
| LED anode (long leg) | GPIO 21 through a series resistor | `LED_PIN`; LED turns on when the pin is HIGH. |
| LED cathode (short leg/flat side) | ESP32 GND | Do not connect an LED directly without a resistor. |

The sketch's pin constants are at the top of the `.ino` file. Module pin order varies, so follow its labels/datasheet. ESP32 GPIOs are not 5 V tolerant: verify sensor output voltage before connecting it. Use a suitable level shifter/divider if an output can exceed the ESP32's permitted input voltage. Use a transistor/driver for any load that exceeds GPIO current limits.

## Step-by-Step Setup

### 1. Find the dashboard computer's LAN address

Connect the computer and ESP32 to a network that allows devices to communicate. On Windows, run `ipconfig` and note the IPv4 address under the active adapter. The example addresses in this guide (`192.168.1.27` for the computer and `192.168.1.50` for the ESP32) are placeholders; use your actual network addresses. A DHCP reservation can keep them stable.

### 2. Wire the hardware

With power disconnected, follow [Wiring Manual](#wiring-manual). Verify each module's pin labels and electrical requirements before connecting it to the ESP32.

### 3. Install Arduino support and libraries

Install Arduino IDE or Arduino CLI, the ESP32 board platform for your exact board, the DHT sensor library and dependencies, and a compatible QR-code library/component from [Parts and Software](#software). Connect the ESP32 over USB and confirm the board and port are available. Compile before permanently mounting the circuit.

### 4. Configure local Wi-Fi credentials

From the project root, copy the example header:

```powershell
Copy-Item firmware/secrets.h.example firmware/secrets.h
```

Edit the new local `firmware/secrets.h` and set `WIFI_SSID`, `WIFI_PASSWORD`, and `DASHBOARD_URL`. Set the URL to the computer's LAN IP plus port and `/data/`, for example `http://192.168.1.27:8000/data/`. The real secrets file is ignored by Git; do not commit or share it.

### 5. Compile and upload the firmware

Open [`firmware/esp32_home_monitoring.ino`](firmware/esp32_home_monitoring.ino) in Arduino IDE. Select the exact ESP32 board and USB port, compile, then upload. The sketch requires `firmware/secrets.h` and the installed libraries.

If the compiler cannot find `qrcode.h` or `esp_qrcode_*`, the QR library/core combination is incompatible. Install a component that implements this API or remove the optional QR-printing code. QR printing is not required for sensor readings or the HTTP API.

### 6. Check startup in Serial Monitor

Open Serial Monitor at **115200 baud**, then reset the ESP32. It prints the configured QR URL, the ESP32 IP once Wi-Fi connects, and sensor diagnostics about every two seconds. Note the ESP32 IP. Check `http://<ESP32-IP>/api/status` from a browser on the same network; a JSON response confirms the API is responding.

If Wi-Fi does not connect, verify the SSID/password, 2.4 GHz network availability, signal, and router client-isolation settings. There is no captive portal or on-device credentials screen.

### 7. Point the dashboard at the ESP32

Set `ESP32_BASE_URL` near the top of [`data/app.js`](data/app.js) to the ESP32 IP, without an API path or trailing slash:

```js
const ESP32_BASE_URL = "http://192.168.1.50";
```

The dashboard adds `/api/status` itself. Update this value if the ESP32's address changes.

### 8. Start the dashboard web server

From the project root, run this in PowerShell:

```powershell
py -m http.server 8000 --bind 0.0.0.0
```

Keep the terminal running. Open `http://localhost:8000/data/` on that same computer. On another LAN device, open `http://<dashboard-computer-ip>:8000/data/`, for example `http://192.168.1.27:8000/data/`. If Windows Firewall prompts, permit private-network access only if appropriate. The URL printed in the ESP32 QR code should resolve to this dashboard address.

### 9. Verify the sensors and dashboard

- The dashboard status should change from Offline to Online.
- Walk in front of the PIR sensor and confirm Motion changes.
- Trigger the FC-51 and confirm Proximity changes and the count increments once on each confirmed transition into detection. It does not count repeatedly while detection stays active.
- Wait for the DHT11 values; they update about every two seconds.
- Enable sound with the Sound button and test the optional audible notification. Some browsers require a click before they allow sound.
- Use the mode API in [Using the System](#using-the-system) to test the alarm LED. Do not test it while relying on the system to protect a space.

## Using the System

### Firmware modes

The firmware implements the modes below, but the current web dashboard has no mode buttons. Send a URL-encoded form field named `mode` to `POST /api/mode`. Example using PowerShell (replace the address with the ESP32 IP):

```powershell
Invoke-RestMethod -Method Post -Uri "http://192.168.1.50/api/mode" -Body @{ mode = "test" }
```

| Mode | Behavior |
| --- | --- |
| `disarmed` | Clears a latched alarm and turns off the LED. Sensors and dashboard notifications continue. |
| `home` | Starts a 5-second exit delay, then arms PIR motion. Confirmed PIR motion latches the alarm. FC-51 objects are counted but do not trigger the security alarm. |
| `away` | Starts a 5-second exit delay, then arms PIR and FC-51 count-increase triggers. Either trigger latches the alarm. |
| `test` | Does not arm/latch the alarm. The LED blinks while PIR motion or FC-51 detection is active. |

The LED blinks at about 250 ms intervals while active. The alarm remains latched until a mode request clears it, such as `disarmed`. Mode and count reset to Disarmed/zero after an ESP32 reboot. There is no entry delay.

Test and disarm from PowerShell:

```powershell
Invoke-RestMethod -Method Post -Uri "http://192.168.1.50/api/mode" -Body @{ mode = "test" }
Invoke-RestMethod -Method Post -Uri "http://192.168.1.50/api/mode" -Body @{ mode = "disarmed" }
```

### Dashboard alerts and controls

The browser-side limits are constants in [`data/app.js`](data/app.js):

| Alert | Trigger |
| --- | --- |
| High temperature | 35 °C or above |
| Low temperature | 5 °C or below |
| High humidity | 80% or above |
| Low humidity | 20% or below |
| PIR motion | Clear changes to detected |
| FC-51 proximity | Clear changes to detected |
| Object count | The reported count increases |
| DHT11 read error | Temperature or humidity is `null` |
| ESP32 disconnected | Three consecutive polls fail after it was connected |

Alerts are generated when conditions become active, not on every poll. Dismiss hides the banner. **Sound off/on** toggles a short beep. **Clear** removes alert-list entries but does not affect the ESP32 or dismiss the banner. Vibration is best-effort and depends on browser/device support.

## HTTP API

The ESP32 serves these routes on port 80:

| Method and path | Purpose |
| --- | --- |
| `GET /api/status` | Returns sensor, mode, alarm, Wi-Fi, and uptime data. The dashboard polls it once per second. |
| `POST /api/mode` | Accepts URL-encoded `mode=disarmed`, `mode=home`, `mode=away`, or `mode=test`. Returns `{"ok":true}` for a valid mode and HTTP 400 for an invalid mode. |
| `OPTIONS /api/mode` | Handles browser CORS preflight requests. |

Example `GET /api/status` response:

```json
{
  "pir": false,
  "ir": false,
  "count": 0,
  "temperature": 22.5,
  "humidity": 45.0,
  "led": false,
  "mode": "disarmed",
  "modeState": "disarmed",
  "modeRemainingSec": 0,
  "alarm": false,
  "rssi": -55,
  "sensorOk": true,
  "sensorReady": true,
  "sensorAgeMs": 250,
  "uptime": 120
}
```

Boolean fields: `pir`, `ir`, `led`, `alarm`, `sensorOk`, and `sensorReady`. Numeric fields: `count`, `modeRemainingSec`, `sensorAgeMs`, and `uptime` (seconds). `rssi` is Wi-Fi signal strength or `null`. Temperature and humidity are numbers or `null` after a sensor read failure. `modeState` can be `disarmed`, `exit_delay`, `armed`, `alarm`, or `test`. The current dashboard uses only `pir`, `ir`, `count`, `temperature`, `humidity`, `led`, and `uptime`.

## Changing Settings

- **ESP32 address:** edit `ESP32_BASE_URL` in `data/app.js`.
- **Wi-Fi and dashboard QR URL:** edit the local, ignored `firmware/secrets.h`.
- **GPIO pins, debounce periods, sample period, and exit delay:** edit the constants near the top of `firmware/esp32_home_monitoring.ino`. If pins change, update the wiring table above too.
- **Browser poll/offline behavior:** edit `POLL_MS` and `OFFLINE_AFTER_FAILS` in `data/app.js`.
- **Browser temperature and humidity thresholds:** edit `TEMP_HIGH_C`, `TEMP_LOW_C`, `HUMIDITY_HIGH`, and `HUMIDITY_LOW` in `data/app.js`.
- **Dashboard markup and styling:** edit `data/index.html` and `data/style.css`.

After a firmware change, compile and upload again. For dashboard changes, reload the page; a hard refresh may be needed if the browser cached old files.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| `DHT.h` missing during compile | Install the DHT sensor library and its requested dependencies. |
| `qrcode.h` or `esp_qrcode_*` missing | The installed QR library does not provide the API used here. Install a compatible component or remove the optional QR printing code. |
| `secrets.h` missing | Copy `firmware/secrets.h.example` to `firmware/secrets.h` and set local credentials. |
| ESP32 does not connect to Wi-Fi | Check SSID/password, 2.4 GHz support, signal, and router client isolation. Read Serial Monitor at 115200 baud. |
| Dashboard says Offline | Open `http://<ESP32-IP>/api/status` directly. Verify the IP in `data/app.js`, same-LAN reachability, router isolation, and firewall rules. |
| Dashboard works on computer but not phone | Use the computer's LAN IP, not `localhost`; allow the server through Windows Firewall on a private network if appropriate. Ensure both devices are on the same non-isolated LAN. |
| CORS or mixed-content error | Use HTTP on the trusted LAN. HTTPS pages are generally blocked from fetching the ESP32's HTTP API. Router rules can block traffic even when CORS is allowed. |
| PIR is always active or never changes | Check VCC/GND/OUT order, GPIO 13, sensor output voltage/polarity, and the module's warm-up time. |
| FC-51 signal is inverted or always active | Firmware expects active-low output on GPIO 27. Check wiring and module logic; adjust `FC51_DETECTED_LEVEL` only if required. |
| DHT values are missing or show errors | Check GPIO 4, supply, ground, DATA, and any required pull-up. Allow the first two-second sample and inspect Serial Monitor. |
| Object count seems inaccurate | FC-51 is a proximity/obstacle sensor, not a precision people counter. Adjust module sensitivity and object path. It counts confirmed transitions into detection, not each object identity. |
| LED does not light or board resets | Check polarity and series resistor. Do not exceed GPIO current limits; use a transistor driver for larger loads. |
| Readings seem stale | Browser polls each second, while DHT11 updates every two seconds. Check `sensorAgeMs` in the API response and inspect browser/network errors. |

## Project Layout

```text
.
|-- .gitignore                  # Excludes local firmware/secrets.h
|-- README.md
|-- command                     # Original local server command
|-- firmware
|   |-- esp32_home_monitoring.ino
|   `-- secrets.h.example       # Copy locally to ignored secrets.h
`-- data
  |-- app.js                  # ESP32 polling, UI updates, browser alerts
  |-- index.html              # Dashboard structure
  `-- style.css               # Dashboard appearance
```

The dashboard is static and needs no frontend build step. Firmware compilation requires the ESP32 board platform and libraries listed above.

## Safety and Limitations

- This is a prototype/learning project, not a certified security or life-safety system. Do not rely on it as the only protection for people or property.
- Sensor modules and ESP32 boards vary. Verify supply voltage, output levels, wiring, GPIO capabilities, LED current, and exact board documentation. ESP32 GPIOs are not 5 V tolerant.
- The API uses unencrypted HTTP, allows cross-origin requests from any origin, and has no authentication. Anyone who can reach the ESP32 on the LAN can read status and change modes. Do not port-forward it or expose it directly to the internet.
- Keep real Wi-Fi credentials only in the ignored local `firmware/secrets.h`. Never commit or share that file. If credentials have been published, change them on the router and update the local header.
- Browser alerts stop when the page is closed, suspended, or disconnected. Sound and vibration depend on browser/device support.
- Mode/alarm state and object count are held in RAM and reset on power loss/reboot. There is no persistent event log, entry delay, backup power, siren, tamper detection, or guaranteed monitoring service.