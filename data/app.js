const ESP32_BASE_URL = "http://192.168.1.50"; // Replace with your ESP32 IP
const POLL_MS = 1000;
const OFFLINE_AFTER_FAILS = 3;
const TEMP_HIGH_C = 35;
const TEMP_LOW_C = 5;
const HUMIDITY_HIGH = 80;
const HUMIDITY_LOW = 20;

const $ = (id) => document.getElementById(id);

let previousStatus = null;
let failedPolls = 0;
let online = false;
let soundOn = false;
let audioContext = null;

const activeAlerts = {
  temperatureHigh: false,
  temperatureLow: false,
  humidityHigh: false,
  humidityLow: false,
  dhtError: false
};

function beep() {
  if (!soundOn) return;

  audioContext ??= new (window.AudioContext || window.webkitAudioContext)();

  const oscillator = audioContext.createOscillator();
  const gain = audioContext.createGain();

  oscillator.frequency.value = 660;
  gain.gain.value = 0.08;

  oscillator.connect(gain);
  gain.connect(audioContext.destination);

  oscillator.start();
  oscillator.stop(audioContext.currentTime + 0.2);
}

function raiseAlert(message) {
  const timestamp = new Date().toLocaleTimeString();

  $("bannerText").textContent = message;
  $("banner").classList.remove("hidden");

  const toast = document.createElement("div");
  toast.className = "toast";
  toast.textContent = message;
  $("toasts").appendChild(toast);
  setTimeout(() => toast.remove(), 5000);

  const list = $("logList");
  list.querySelector(".empty")?.remove();

  const item = document.createElement("li");
  const text = document.createElement("span");
  const time = document.createElement("time");

  text.textContent = message;
  time.textContent = timestamp;
  item.append(text, time);
  list.prepend(item);

  beep();
  navigator.vibrate?.(200);
}

function setCardState(id, isAlert) {
  const card = $(id);
  card.classList.toggle("alert", isAlert);
  card.classList.toggle("ok", !isAlert);
}

function updateEdgeAlert(key, condition, message) {
  if (condition && !activeAlerts[key]) {
    raiseAlert(message);
  }

  activeAlerts[key] = condition;
}

function renderStatus(status) {
  $("pir").textContent = status.pir ? "Motion" : "Clear";
  $("ir").textContent = status.ir ? "Object" : "Clear";
  $("count").textContent = status.count;
  $("temp").textContent = status.temperature ?? "--";
  $("hum").textContent = status.humidity ?? "--";
  $("led").textContent = status.led ? "Blinking" : "Off";
  $("uptime").textContent = `Uptime ${Math.floor(status.uptime / 60)} min`;

  const temperatureAlert =
    status.temperature !== null &&
    (status.temperature >= TEMP_HIGH_C || status.temperature <= TEMP_LOW_C);

  const humidityAlert =
    status.humidity !== null &&
    (status.humidity >= HUMIDITY_HIGH || status.humidity <= HUMIDITY_LOW);

  setCardState("cardPir", status.pir);
  setCardState("cardIr", status.ir);
  setCardState("cardTemp", temperatureAlert || status.temperature === null);
  setCardState("cardHum", humidityAlert || status.humidity === null);
  setCardState("cardLed", status.led);
}

function evaluateAlerts(status) {
  if (previousStatus) {
    if (status.pir && !previousStatus.pir) {
      raiseAlert("PIR: motion detected");
    }

    if (status.ir && !previousStatus.ir) {
      raiseAlert("IR: object detected");
    }

    if (status.count > previousStatus.count) {
      raiseAlert(`Object count is now ${status.count}`);
    }
  }

  const temperature = status.temperature;
  const humidity = status.humidity;

  updateEdgeAlert(
    "dhtError",
    temperature === null || humidity === null,
    "DHT11: sensor read error"
  );
  updateEdgeAlert(
    "temperatureHigh",
    temperature !== null && temperature >= TEMP_HIGH_C,
    `Temperature high: ${temperature} °C`
  );
  updateEdgeAlert(
    "temperatureLow",
    temperature !== null && temperature <= TEMP_LOW_C,
    `Temperature low: ${temperature} °C`
  );
  updateEdgeAlert(
    "humidityHigh",
    humidity !== null && humidity >= HUMIDITY_HIGH,
    `Humidity high: ${humidity} %`
  );
  updateEdgeAlert(
    "humidityLow",
    humidity !== null && humidity <= HUMIDITY_LOW,
    `Humidity low: ${humidity} %`
  );

  previousStatus = status;
}

function setOnline(isOnline) {
  if (isOnline === online) return;

  online = isOnline;
  $("link").textContent = online ? "Online" : "Offline";
  $("link").className = `pill ${online ? "pill-on" : "pill-off"}`;

  if (!online && previousStatus) {
    raiseAlert("Connection to ESP32 lost");
  }
}

async function pollStatus() {
  try {
    const response = await fetch(`${ESP32_BASE_URL}/api/status`, {
      cache: "no-store",
      signal: AbortSignal.timeout(2500)
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const status = await response.json();

    failedPolls = 0;
    setOnline(true);
    renderStatus(status);
    evaluateAlerts(status);
  } catch {
    failedPolls += 1;

    if (failedPolls >= OFFLINE_AFTER_FAILS) {
      setOnline(false);
    }
  } finally {
    setTimeout(pollStatus, POLL_MS);
  }
}

$("bannerClose").addEventListener("click", () => {
  $("banner").classList.add("hidden");
});

$("clearLog").addEventListener("click", () => {
  $("logList").innerHTML = '<li class="empty">No alerts yet.</li>';
});

$("soundBtn").addEventListener("click", () => {
  soundOn = !soundOn;
  $("soundBtn").textContent = soundOn ? "Sound on" : "Sound off";
  $("soundBtn").classList.toggle("on", soundOn);

  if (soundOn) {
    beep();
  }
});

pollStatus();