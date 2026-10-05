# ESP32 Home Security Monitoring System

A browser-based dashboard for monitoring an ESP32 home-security and environment-sensing setup. The dashboard polls the ESP32 over the local network and displays sensor readings, connection state, and alerts.

## Features

- Live motion state from a PIR sensor.
- IR proximity/object detection and an object count supplied by the ESP32.
- Temperature and relative humidity readings from a DHT11 sensor.
- Alarm LED state.
- Alerts for motion, object detection/count changes, sensor read errors, and temperature or humidity thresholds.
- An in-page alert banner, temporary notifications, an alert log, optional audible beeps, and vibration where the browser/device supports it.
- ESP32 online/offline status and reported uptime.

## Sensors and Hardware

The dashboard is written for these signals:

| Device/signal | Dashboard display | Meaning |
| --- | --- | --- |
| PIR motion sensor | Motion (PIR) | `Motion` when the ESP32 reports `pir: true`; otherwise `Clear`. |
| IR sensor | Proximity (IR) | `Object` when the ESP32 reports `ir: true`; otherwise `Clear`. |
| Object counter | Objects counted | Count reported by the ESP32. The dashboard alerts when the count increases. |
| DHT11 | Temperature and humidity | Temperature in degrees Celsius and relative humidity in percent. |
| Alarm LED | Alarm LED | `Blinking` when `led: true`; otherwise `Off`. |

This repository contains the dashboard, not the ESP32 firmware or a wiring diagram. Sensor pin assignments, power requirements, counting logic, LED behavior, and the mechanism that serves the API must be defined by the firmware used with your hardware. Check your board and sensor documentation before wiring components.

## Requirements

- An ESP32 running firmware that exposes `GET /api/status` on your local network.
- A modern browser with JavaScript enabled.
- Python 3 (or another static HTTP server) to serve the dashboard. Opening the HTML directly with `file://` may prevent browser requests to the ESP32 from working.
- The dashboard device and ESP32 connected to a network that allows them to communicate.

## Configure and Run

1. Find the ESP32's local IP address and update `ESP32_BASE_URL` near the top of [`data/app.js`](data/app.js) to match it. For example:

   ```js
   const ESP32_BASE_URL = "http://192.168.1.50";
   ```

2. In PowerShell, from the project root, start a static server:

   ```powershell
   py -m http.server 8000 --directory data
   ```

3. Open [http://localhost:8000](http://localhost:8000) in a browser. Keep the terminal running while using the dashboard.

4. Confirm that the ESP32 is reachable at `http://<ESP32-IP>/api/status`. If the dashboard remains offline, check the configured IP, Wi-Fi/network isolation, and browser developer console. If the firmware serves the dashboard from a different origin, it must allow the browser request with an appropriate CORS policy. A page served over HTTPS generally cannot fetch an insecure `http://` ESP32 endpoint; use a trusted local HTTP setup or configure HTTPS on the device/network.

## ESP32 API Contract

The dashboard requests `GET /api/status` once per second. It expects a JSON object with these fields:

```json
{
  "pir": false,
  "ir": false,
  "count": 0,
  "temperature": 22.5,
  "humidity": 45,
  "led": false,
  "uptime": 120
}
```

`pir`, `ir`, and `led` should be booleans. `count` should be a number. `temperature` and `humidity` should be numbers or `null` when a DHT11 reading is unavailable. `uptime` is expected in seconds. The firmware should return a successful HTTP status and JSON content for the endpoint.

## Alerts and Controls

The current thresholds are defined at the top of [`data/app.js`](data/app.js):

- High temperature: 35 °C or above.
- Low temperature: 5 °C or below.
- High humidity: 80% or above.
- Low humidity: 20% or below.

Threshold alerts are raised when a condition becomes active. Motion and IR alerts trigger on a transition to detected, and a count alert triggers when the reported count increases. Three consecutive failed polls mark the ESP32 offline. Use **Sound off/on** to toggle the optional beep and **Clear** to clear the on-page alert log; clearing the log does not change sensor state or dismiss an active banner.

## Project Layout

```text
.
|-- README.md
|-- command          # Previously used local server command
`-- data
    |-- app.js       # ESP32 polling, display updates, and alert logic
    |-- index.html   # Dashboard structure
    `-- style.css    # Dashboard styles
```

No build step or package installation is needed. The `command` file records the original `py -m http.server 8000 --bind 0.0.0.0` command; the run instructions above serve the `data` directory directly.

## Network and Safety Notes

This is a local-network monitoring dashboard, not a certified security system. The web UI currently uses plain HTTP and the API contract shown above does not specify authentication. Keep the ESP32 and dashboard on a trusted network, do not expose the endpoint directly to the public internet, and add appropriate authentication and transport security in the firmware/network before using it for sensitive monitoring.