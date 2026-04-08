#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_https_server.h>
#include "local_secrets.h"

// ===== HTTPS SERVER =====
static httpd_handle_t https_server = NULL;


// ===== SHIFT REGISTER PINS =====
const int dataPin = 13;   // DS (Data)
const int clockPin = 14;  // SH_CP (Clock)
const int latchPin = 15;  // ST_CP (Latch)

// ===== RELAY STATES =====
bool relayState[26] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

static bool parseRelayChannel(httpd_req_t *req, int *channel) {
  char buf[32];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK) {
    return false;
  }
  if (httpd_query_key_value(buf, "ch", buf, sizeof(buf)) != ESP_OK) {
    return false;
  }
  int ch = atoi(buf);
  if (ch < 0 || ch >= 26) {
    return false;
  }
  *channel = ch;
  return true;
}

static esp_err_t root_get_handler(httpd_req_t *req) {
  String page = webpage();
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, page.c_str(), page.length());
  return ESP_OK;
}

static esp_err_t relay_on_handler(httpd_req_t *req) {
  int ch;
  if (!parseRelayChannel(req, &ch)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid channel");
    return ESP_FAIL;
  }
  relayState[ch] = true;
  updateShiftRegisters();
  char jsonResponse[80];
  snprintf(jsonResponse, sizeof(jsonResponse), "{\"status\":\"success\",\"channel\":%d,\"state\":true}", ch);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, jsonResponse);
  return ESP_OK;
}

static esp_err_t relay_off_handler(httpd_req_t *req) {
  int ch;
  if (!parseRelayChannel(req, &ch)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid channel");
    return ESP_FAIL;
  }
  relayState[ch] = false;
  updateShiftRegisters();
  char jsonResponse[80];
  snprintf(jsonResponse, sizeof(jsonResponse), "{\"status\":\"success\",\"channel\":%d,\"state\":false}", ch);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, jsonResponse);
  return ESP_OK;
}

static bool parseAllState(httpd_req_t *req, bool *state) {
  char buf[16];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK) {
    return false;
  }
  if (httpd_query_key_value(buf, "state", buf, sizeof(buf)) != ESP_OK) {
    return false;
  }
  if (strcmp(buf, "on") == 0) {
    *state = true;
    return true;
  }
  if (strcmp(buf, "off") == 0) {
    *state = false;
    return true;
  }
  return false;
}

static esp_err_t relay_all_handler(httpd_req_t *req) {
  bool state;
  if (!parseAllState(req, &state)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid state");
    return ESP_FAIL;
  }
  for (int i = 0; i < 26; i++) {
    relayState[i] = state;
  }
  updateShiftRegisters();
  char jsonResponse[100];
  snprintf(jsonResponse, sizeof(jsonResponse), "{\"status\":\"success\",\"state\":\"%s\"}", state ? "on" : "off");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, jsonResponse);
  return ESP_OK;
}

// ===== FUNCTION TO UPDATE SHIFT REGISTERS =====
void updateShiftRegisters() {
  digitalWrite(latchPin, LOW);
  for (int i = 0; i < 4; i++) {  // 4 bytes for 32 bits (covers 26 relays + extras)
    byte data = 0;
    for (int j = 0; j < 8; j++) {
      int relayIndex = i * 8 + j;
      if (relayIndex < 26 && relayState[relayIndex]) {
        data |= (1 << j);
      }
    }
    shiftOut(dataPin, clockPin, MSBFIRST, data);
  }
  digitalWrite(latchPin, HIGH);
}

// ===== HTML PAGE =====
String webpage() {
  String page;
  page.reserve(11000);
  page += R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Smart IoT Relay Control</title>
  <style>
    :root { --bg: linear-gradient(135deg, #667eea 0%, #764ba2 100%); --c-bg: white; --t-c: #333; --cd-bg: #f7fafc; --cd-b: #e2e8f0; --s-on: #38a169; --s-off: #e53e3e; --sl-bg: #ccc; --sl-c: #38a169; }
    .dark { --bg: linear-gradient(135deg, #2d3748 0%, #1a202c 100%); --c-bg: #2d3748; --t-c: #e2e8f0; --cd-bg: #4a5568; --cd-b: #718096; --s-on: #48bb78; --s-off: #f56565; --sl-bg: #718096; --sl-c: #48bb78; }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: var(--bg); margin: 0; padding: 20px; color: var(--t-c); }
    .container { max-width: 800px; margin: 0 auto; background: var(--c-bg); border-radius: 10px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); padding: 30px; }
    h1 { text-align: center; margin-bottom: 30px; }
    .theme-toggle { position: absolute; top: 20px; right: 20px; background: var(--c-bg); border: 1px solid var(--cd-b); border-radius: 5px; padding: 10px; cursor: pointer; }
    .relay-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; }
    .relay-card { background: var(--cd-bg); border: 1px solid var(--cd-b); border-radius: 8px; padding: 20px; text-align: center; }
    .all-control { grid-column: 1 / -1; background: linear-gradient(90deg, #805ad5, #d53f8c); color: white; }
    .relay-title { font-size: 18px; font-weight: bold; margin-bottom: 10px; }
    .status { font-size: 16px; font-weight: bold; margin: 10px 0; }
    .status.on { color: var(--s-on); }
    .status.off { color: var(--s-off); }
    .switch { position: relative; display: inline-block; width: 60px; height: 34px; margin-top: 15px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; inset: 0; background-color: var(--sl-bg); transition: .2s; border-radius: 34px; }
    .slider:before { position: absolute; content: ''; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .2s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--sl-c); }
    input:checked + .slider:before { transform: translateX(26px); }
    .loading .slider { opacity: .6; pointer-events: none; }
    @media (max-width: 600px) { .container { padding: 20px; } .relay-grid { grid-template-columns: 1fr; } }
  </style>
  <script>
    function toggleRelay(channel, checkbox) {
      const on = checkbox.checked;
      const card = checkbox.closest('.relay-card');
      card.classList.add('loading');
      fetch('/' + (on ? 'on' : 'off') + '?ch=' + channel)
        .then(r => r.json())
        .then(() => {
          const s = card.querySelector('.status');
          s.textContent = on ? 'ON' : 'OFF';
          s.className = 'status ' + (on ? 'on' : 'off');
          updateMasterStatus();
          card.classList.remove('loading');
        })
        .catch(() => card.classList.remove('loading'));
    }
    function toggleAll(checkbox) {
      const on = checkbox.checked;
      const card = checkbox.closest('.relay-card');
      card.classList.add('loading');
      fetch('/all?state=' + (on ? 'on' : 'off'))
        .then(r => r.json())
        .then(() => {
          const s = card.querySelector('.status');
          s.textContent = on ? 'ALL ON' : 'ALL OFF';
          s.className = 'status ' + (on ? 'on' : 'off');
          document.querySelectorAll('.relay-card:not(.all-control) input[type=checkbox]').forEach(i => {
            i.checked = on;
            const st = i.closest('.relay-card').querySelector('.status');
            st.textContent = on ? 'ON' : 'OFF';
            st.className = 'status ' + (on ? 'on' : 'off');
          });
          card.classList.remove('loading');
        })
        .catch(() => card.classList.remove('loading'));
    }
    function updateMasterStatus() {
      const inputs = document.querySelectorAll('.relay-card:not(.all-control) input[type=checkbox]');
      const values = Array.from(inputs).map(x => x.checked);
      const anyOn = values.some(Boolean);
      const allOn = values.length > 0 && values.every(Boolean);
      const master = document.querySelector('.all-control input[type=checkbox]');
      const status = document.querySelector('.all-control .status');
      master.checked = anyOn;
      status.textContent = allOn ? 'ALL ON' : (anyOn ? 'SOME ON' : 'ALL OFF');
      status.className = 'status ' + (anyOn ? 'on' : 'off');
    }
    function toggleTheme() {
      const body = document.body;
      const btn = document.querySelector('.theme-toggle');
      body.classList.toggle('dark');
      const dark = body.classList.contains('dark');
      btn.textContent = dark ? 'Light Mode' : 'Dark Mode';
      localStorage.setItem('theme', dark ? 'dark' : 'light');
    }
    window.onload = function() {
      const saved = localStorage.getItem('theme');
      if (saved === 'dark') {
        document.body.classList.add('dark');
        document.querySelector('.theme-toggle').textContent = 'Light Mode';
      }
    };
  </script>
</head>
<body>
  <div class="container">
    <div class="theme-toggle" onclick="toggleTheme()">Dark Mode</div>
    <h1>ESP32 Smart IoT Relay Control</h1>
    <div class="relay-grid">
)HTML";

  bool anyRelayOn = false;
  bool allRelayOn = true;
  for (int i = 0; i < 26; i++) {
    if (relayState[i]) {
      anyRelayOn = true;
    } else {
      allRelayOn = false;
    }
  }

  String masterStatusText = anyRelayOn ? (allRelayOn ? "ALL ON" : "SOME ON") : "ALL OFF";
  String masterChecked = anyRelayOn ? "checked" : "";
  page += "<div class='relay-card all-control'>";
  page += "<div class='relay-title'>All Relays</div>";
  page += "<div class='status " + String(anyRelayOn ? "on" : "off") + "'>";
  page += masterStatusText;
  page += "</div>";
  page += "<label class=\"switch\">";
  page += "<input type=\"checkbox\" onchange=\"toggleAll(this)\" " + masterChecked + ">";
  page += "<span class=\"slider\"></span>";
  page += "</label>";
  page += "</div>";

  for (int i = 0; i < 26; i++) {
    page += "<div class='relay-card'>";
    page += "<div class='relay-title'>Relay " + String(i + 1) + "</div>";
    page += "<div class='status ";
    page += relayState[i] ? "on" : "off";
    page += "'>";
    page += relayState[i] ? "ON" : "OFF";
    page += "</div>";
    page += "<label class=\"switch\">";
    page += "<input type=\"checkbox\" onchange=\"toggleRelay(" + String(i) + ", this)\" " + (relayState[i] ? "checked" : "") + ">";
    page += "<span class=\"slider\"></span>";
    page += "</label>";
    page += "</div>";
  }

  page += R"HTML(
    </div>
  </div>
</body>
</html>
)HTML";

  return page;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  // Shift register pins
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);

  // Initialize all relays OFF
  updateShiftRegisters();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Initialize mDNS
  if (!MDNS.begin("smart_iot")) {
    Serial.println("Error starting mDNS");
  } else {
    Serial.println("mDNS started. Access at: https://smart_iot.local");
    MDNS.addService("https", "tcp", 443);
  }

  httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
  config.servercert = (const uint8_t *)server_cert;
  config.servercert_len = sizeof(server_cert);
  config.prvtkey_pem = (const uint8_t *)server_key;
  config.prvtkey_len = sizeof(server_key);
  config.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;
  config.port_secure = 443;
  config.port_insecure = 0;

  if (httpd_ssl_start(&https_server, &config) != ESP_OK) {
    Serial.println("Failed to start HTTPS server");
    return;
  }

  httpd_uri_t uri_root = {"/", HTTP_GET, root_get_handler, NULL};
  httpd_register_uri_handler(https_server, &uri_root);

  httpd_uri_t uri_on = {"/on", HTTP_GET, relay_on_handler, NULL};
  httpd_register_uri_handler(https_server, &uri_on);

  httpd_uri_t uri_off = {"/off", HTTP_GET, relay_off_handler, NULL};
  httpd_register_uri_handler(https_server, &uri_off);

  httpd_uri_t uri_all = {"/all", HTTP_GET, relay_all_handler, NULL};
  httpd_register_uri_handler(https_server, &uri_all);

  Serial.println("HTTPS server started");
}

// ===== LOOP =====
void loop() {
  delay(1);
}


