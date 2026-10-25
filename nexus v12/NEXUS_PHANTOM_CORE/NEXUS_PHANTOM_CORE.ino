/*
  NEXUS: PHANTOM CORE v1.0
  ESP8266 / NodeMCU firmware
  Libraries: ESP8266 core, ESP8266WebServer, LittleFS (bundled with core)
  No ArduinoJson dependency.

  SECURITY / CAPABILITY NOTES:
  - Wi-Fi scan reports nearby access points, not all clients on your router.
  - Web login logs are for this ESP's web console only; they do not monitor router Wi-Fi authentication.
  - HTTP is not encrypted. Use only on your trusted local network; do not port-forward this device.
  - GPIO outputs are logic signals, not relay/motor power outputs. Use suitable drivers/modules.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

const char* WIFI_SSID     = "wifi_2.4";
const char* WIFI_PASSWORD = "wifi_2.4";

const char* AP_SSID       = "nexus";
const char* AP_PASSWORD   = "12345678";

const char* ADMIN_PASSWORD = "admin";
const char* USER_PASSWORD  = "user";

static const uint8_t GPIO_COUNT = 4;
// NodeMCU labels: D1=GPIO5, D2=GPIO4, D5=GPIO14, D6=GPIO12
const uint8_t GPIO_PINS[GPIO_COUNT] = {5, 4, 14, 12};
const char* GPIO_LABELS[GPIO_COUNT] = {"D1/GPIO5", "D2/GPIO4", "D5/GPIO14", "D6/GPIO12"};
const uint8_t BUZZER_PIN = 13; // D7/GPIO13; use a transistor driver for buzzers that draw > a few mA.

ESP8266WebServer server(80);
String sessionToken = "";
String sessionRole = "";
unsigned long sessionLastSeen = 0;
const unsigned long SESSION_TIMEOUT_MS = 30UL * 60UL * 1000UL;

String events[32];
uint8_t eventWrite = 0;
uint8_t eventCount = 0;
unsigned long bootMillis = 0;
uint8_t failedLogins = 0;
bool wifiAttemptStarted = false;
unsigned long lastWiFiRetry = 0;

String jsonEscape(const String& in) {
  String out; out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '\"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}
String jsonBool(bool b) { return b ? "true" : "false"; }

void addEvent(const String& msg) {
  String stamp = String(millis() / 1000UL) + "s ";
  events[eventWrite] = stamp + msg;
  eventWrite = (eventWrite + 1) % 32;
  if (eventCount < 32) eventCount++;
  Serial.println("[EVENT] " + msg);
}

void beepMs(uint16_t durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}
void beepBoot() {
  beepMs(65); delay(60); beepMs(65);
}
void beepAdmin() {
  beepMs(70); delay(70); beepMs(70); delay(70); beepMs(150);
}
void beepFailedTriple() {
  beepMs(100); delay(90); beepMs(100); delay(90); beepMs(100);
}
void beepTest() {
  beepMs(70); delay(60); beepMs(140); delay(60); beepMs(70);
}

String formatUptime() {
  unsigned long s = millis() / 1000UL;
  unsigned long d = s / 86400UL; s %= 86400UL;
  unsigned long h = s / 3600UL; s %= 3600UL;
  unsigned long m = s / 60UL; s %= 60UL;
  char buf[48];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", d, h, m, s);
  return String(buf);
}
String ipString(IPAddress ip) { return String(ip[0])+"."+String(ip[1])+"."+String(ip[2])+"."+String(ip[3]); }

bool wifiConfigured() {
  return String(WIFI_SSID).length() > 0 && String(WIFI_SSID) != "CHANGE_WIFI_NAME";
}
void startWiFiStation() {
  if (!wifiConfigured()) {
    Serial.println("[WIFI] SSID placeholder unchanged; AP mode remains available.");
    addEvent("Wi-Fi STA skipped: configure WIFI_SSID first");
    return;
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiAttemptStarted = true;
  lastWiFiRetry = millis();
  Serial.println("[WIFI] Trying router: " + String(WIFI_SSID));
}
void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!wifiConfigured()) return;
  if (!wifiAttemptStarted || millis() - lastWiFiRetry > 30000UL) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiAttemptStarted = true;
    lastWiFiRetry = millis();
    Serial.println("[WIFI] Retry started");
  }
}

bool isAuthenticated() {
  if (sessionToken.length() == 0) return false;
  String supplied = server.header("X-Session-Token");
  if (supplied.length() == 0 || supplied != sessionToken) return false;
  if (millis() - sessionLastSeen > SESSION_TIMEOUT_MS) {
    sessionToken = ""; sessionRole = "";
    addEvent("Session expired");
    return false;
  }
  sessionLastSeen = millis();
  return true;
}
bool isAdmin() { return isAuthenticated() && sessionRole == "admin"; }

void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", body);
}
void sendUnauthorized() { sendJson(401, "{\"ok\":false,\"error\":\"AUTH_REQUIRED\"}"); }

String statusJson() {
  String out = "{\"ok\":true,\"uptime\":\"" + jsonEscape(formatUptime()) + "\",";
  out += "\"millis\":" + String(millis()) + ",";
  out += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  out += "\"chip\":\"" + String(ESP.getChipId(), HEX) + "\",";
  out += "\"wifi_connected\":" + jsonBool(WiFi.status() == WL_CONNECTED) + ",";
  out += "\"ssid\":\"" + jsonEscape(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("(not connected)")) + "\",";
  out += "\"ip\":\"" + ipString(WiFi.localIP()) + "\",";
  out += "\"ap_ip\":\"" + ipString(WiFi.softAPIP()) + "\",";
  out += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
  out += "\"role\":\"" + jsonEscape(sessionRole) + "\",";
  out += "\"gpio\":[";
  for (uint8_t i=0;i<GPIO_COUNT;i++) {
    if (i) out += ",";
    out += "{\"n\":" + String(i+1) + ",\"pin\":\"" + GPIO_LABELS[i] + "\",\"state\":" + jsonBool(digitalRead(GPIO_PINS[i]) == HIGH) + "}";
  }
  out += "]}";
  return out;
}
String eventLogText() {
  String out;
  uint8_t start = (eventWrite + 32 - eventCount) % 32;
  for (uint8_t i=0;i<eventCount;i++) {
    out += events[(start+i)%32];
    out += "\n";
  }
  if (!out.length()) out = "No events recorded since boot.\n";
  return out;
}
String wifiScanText() {
  if (WiFi.status() != WL_CONNECTED && WiFi.getMode() == WIFI_AP) {
    // WiFi.scanNetworks works in AP+STA mode; mode is configured as AP+STA at boot.
  }
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) return "Wi-Fi scan failed or is busy. Try again.\n";
  String out = "Nearby Wi-Fi access points: " + String(n) + "\n";
  for (int i=0;i<n;i++) {
    out += String(i+1) + ". SSID=\"" + WiFi.SSID(i) + "\" RSSI=" + String(WiFi.RSSI(i)) +
           " dBm CH=" + String(WiFi.channel(i)) +
           " SECURITY=" + String(WiFi.encryptionType(i) == ENC_TYPE_NONE ? "OPEN" : "PROTECTED") + "\n";
  }
  WiFi.scanDelete();
  if (n == 0) out += "No access points found.\n";
  out += "NOTE: this is a nearby Wi-Fi scan, not a list of devices connected to your router.\n";
  return out;
}
String netInfoText() {
  String out = "MODE: AP+STA\n";
  out += "STA: " + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "NOT CONNECTED") + "\n";
  out += "STA SSID: " + String(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "(none)") + "\n";
  out += "STA IP: " + ipString(WiFi.localIP()) + "\n";
  out += "GATEWAY: " + ipString(WiFi.gatewayIP()) + "\n";
  out += "SUBNET: " + ipString(WiFi.subnetMask()) + "\n";
  out += "DNS: " + ipString(WiFi.dnsIP()) + "\n";
  out += "AP SSID: " + String(AP_SSID) + "\n";
  out += "AP IP: " + ipString(WiFi.softAPIP()) + "\n";
  out += "RSSI: " + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + " dBm\n";
  out += "LAN CLIENT INVENTORY: not provided by ESP8266 alone; check router or add server integration.\n";
  return out;
}

int parseChannel(String s) {
  s.trim();
  if (s.length() != 1 || s[0] < '1' || s[0] > '4') return -1;
  return s.toInt() - 1;
}
String gpioText() {
  String out;
  for (uint8_t i=0;i<GPIO_COUNT;i++) {
    out += "CH" + String(i+1) + " " + GPIO_LABELS[i] + " = ";
    out += digitalRead(GPIO_PINS[i]) == HIGH ? "ON\n" : "OFF\n";
  }
  return out;
}
String helpText() {
  return
  "NEXUS PHANTOM CORE :: MINI SHELL\n"
  "help                         show commands\n"
  "status                       system + Wi-Fi status\n"
  "netinfo                      IP, gateway, subnet, DNS\n"
  "wifi                         Wi-Fi connection status\n"
  "wscan  | scan                scan nearby Wi-Fi APs\n"
  "gpio list                    show four GPIO channels\n"
  "gpio <1-4> <on|off|toggle>   control a channel\n"
  "ch1 on/off/toggle            channel shortcut (repeat ch2..ch4)\n"
  "alloff                       switch all four outputs off\n"
  "scene safe                   safe scene: all outputs off\n"
  "beep                         test buzzer pattern\n"
  "log                          recent in-RAM event log\n"
  "clear                        clear terminal display\n"
  "uptime                       show uptime\n"
  "heap                         show free heap bytes\n"
  "ip                           show station and AP IPs\n"
  "whoami                       show current role\n"
  "echo <text>                  print text\n"
  "about                        build and capability notes\n"
  "reboot                       reboot board (admin only)\n"
  "logout                       invalidate current session\n"
  "help has 20+ commands; this is a device command interpreter, not a full Linux shell.\n";
}
String runCommand(String cmd) {
  cmd.trim();
  String original = cmd;
  String c = cmd; c.toLowerCase();
  if (c == "help" || c == "?") return helpText();
  if (c == "status") return "NEXUS PHANTOM CORE v1.0\n" + netInfoText() + "UPTIME: " + formatUptime() + "\nFREE HEAP: " + String(ESP.getFreeHeap()) + " bytes\n";
  if (c == "netinfo" || c == "ip") return netInfoText();
  if (c == "wifi") return String("Wi-Fi: ") + (WiFi.status()==WL_CONNECTED ? "CONNECTED\nSSID: "+WiFi.SSID()+"\nIP: "+ipString(WiFi.localIP())+"\nRSSI: "+String(WiFi.RSSI())+" dBm\n" : "NOT CONNECTED; fallback AP remains available at "+ipString(WiFi.softAPIP())+"\n");
  if (c == "wscan" || c == "scan" || c == "wifi scan") return wifiScanText();
  if (c == "gpio list" || c == "gpio") return gpioText();
  if (c == "alloff" || c == "scene safe") {
    for (uint8_t i=0;i<GPIO_COUNT;i++) digitalWrite(GPIO_PINS[i], LOW);
    addEvent("All GPIO outputs set OFF (safe scene)");
    return "SAFE SCENE applied: all four GPIO outputs OFF.\n";
  }
  if (c == "beep") { beepTest(); addEvent("Buzzer test requested"); return "Buzzer test sequence played.\n"; }
  if (c == "log") return eventLogText();
  if (c == "clear") return "\x1b[2J\x1b[H";
  if (c == "uptime") return "Uptime: " + formatUptime() + "\n";
  if (c == "heap") return "Free heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  if (c == "whoami") return "Current role: " + sessionRole + "\n";
  if (c == "about") return "NEXUS: PHANTOM CORE v1.0\nESP8266 local control console\nWi-Fi AP scan + 4 GPIO channels + event log + login alarm.\nNot a full OS. No router traffic capture or router client inventory without integration.\n";
  if (c == "logout") { sessionToken = ""; sessionRole = ""; addEvent("User logged out"); return "Session invalidated. Reload page to log in again.\n"; }
  if (c == "reboot") {
    if (sessionRole != "admin") return "ERROR: reboot requires admin role.\n";
    addEvent("Admin requested reboot");
    delay(250);
    ESP.restart();
    return "Rebooting...\n";
  }
  if (c.startsWith("echo ")) return original.substring(5) + "\n";

  if (c.startsWith("gpio ")) {
    int first = c.indexOf(' ');
    int second = c.indexOf(' ', first+1);
    if (second < 0) return "Usage: gpio <1-4> <on|off|toggle> OR gpio list\n";
    int ch = parseChannel(c.substring(first+1, second));
    if (ch < 0) return "ERROR: channel must be 1, 2, 3 or 4.\n";
    String action = c.substring(second+1); action.trim();
    if (action == "on") digitalWrite(GPIO_PINS[ch], HIGH);
    else if (action == "off") digitalWrite(GPIO_PINS[ch], LOW);
    else if (action == "toggle") digitalWrite(GPIO_PINS[ch], !digitalRead(GPIO_PINS[ch]));
    else return "ERROR: action must be on, off or toggle.\n";
    String state = digitalRead(GPIO_PINS[ch]) == HIGH ? "ON" : "OFF";
    addEvent("CH" + String(ch+1) + " set " + state + " by " + sessionRole);
    return "CH" + String(ch+1) + " (" + GPIO_LABELS[ch] + ") => " + state + "\n";
  }
  if (c.startsWith("ch") && c.length() >= 5 && c[2] >= '1' && c[2] <= '4') {
    int ch = c[2]-'1';
    String action = c.substring(3); action.trim();
    if (action == "on") digitalWrite(GPIO_PINS[ch], HIGH);
    else if (action == "off") digitalWrite(GPIO_PINS[ch], LOW);
    else if (action == "toggle") digitalWrite(GPIO_PINS[ch], !digitalRead(GPIO_PINS[ch]));
    else return "Usage: ch1 on|off|toggle (same for ch2..ch4)\n";
    String state = digitalRead(GPIO_PINS[ch]) == HIGH ? "ON" : "OFF";
    addEvent("CH" + String(ch+1) + " set " + state + " by " + sessionRole);
    return "CH" + String(ch+1) + " => " + state + "\n";
  }
  return "Unknown command: " + original + "\nType 'help' to list available commands.\n";
}

void handleRoot() {
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    server.send(200, "text/plain; charset=utf-8",
      "NEXUS PHANTOM CORE is running, but /index.html is missing. Upload the LittleFS data folder. AP: http://192.168.4.1");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(f, "text/html");
  f.close();
}
void handleLogin() {
  String user = server.arg("username");
  String pass = server.arg("password");
  user.trim();
  if ((user == "admin" && pass == ADMIN_PASSWORD) || (user == "user" && pass == USER_PASSWORD)) {
    sessionRole = user;
    sessionToken = String(ESP.getChipId(), HEX) + "-" + String(micros(), HEX) + "-" + String(random(0x7FFFFFFF), HEX);
    sessionLastSeen = millis();
    failedLogins = 0;
    addEvent("Successful web login: " + user);
    if (user == "admin") beepAdmin();
    sendJson(200, "{\"ok\":true,\"token\":\"" + jsonEscape(sessionToken) + "\",\"role\":\"" + user + "\"}");
  } else {
    failedLogins++;
    addEvent("Failed web login attempt for username: " + user);
    if (failedLogins >= 3) {
      addEvent("SECURITY ALERT: 3+ consecutive failed web logins");
      beepFailedTriple();
      failedLogins = 0;
    }
    delay(250);
    sendJson(401, "{\"ok\":false,\"error\":\"Invalid username or password\"}");
  }
}
void handleStatus() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  sendJson(200, statusJson());
}
void handleWifiScan() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  if (sessionRole != "admin") { sendJson(403, "{\"ok\":false,\"error\":\"ADMIN_REQUIRED\"}"); return; }
  String scan = wifiScanText();
  sendJson(200, "{\"ok\":true,\"text\":\"" + jsonEscape(scan) + "\"}");
  addEvent("Wi-Fi access-point scan completed by admin");
}
void handleCommand() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  String cmd = server.arg("cmd");
  String low = cmd; low.toLowerCase(); low.trim();
  if (low == "reboot" && sessionRole != "admin") {
    sendJson(403, "{\"ok\":false,\"output\":\"ERROR: reboot requires admin role.\\n\"}");
    return;
  }
  if ((low == "wscan" || low == "scan" || low == "wifi scan") && sessionRole != "admin") {
    sendJson(403, "{\"ok\":false,\"output\":\"ERROR: Wi-Fi scan requires admin role.\\n\"}");
    return;
  }
  String result = runCommand(cmd);
  sendJson(200, "{\"ok\":true,\"output\":\"" + jsonEscape(result) + "\"}");
}
void handleGpio() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  String chs = server.arg("channel");
  String action = server.arg("action"); action.toLowerCase();
  int ch = parseChannel(chs);
  if (ch < 0 || (action != "on" && action != "off" && action != "toggle")) {
    sendJson(400, "{\"ok\":false,\"error\":\"Use channel 1-4 and action on/off/toggle\"}");
    return;
  }
  if (action == "on") digitalWrite(GPIO_PINS[ch], HIGH);
  else if (action == "off") digitalWrite(GPIO_PINS[ch], LOW);
  else digitalWrite(GPIO_PINS[ch], !digitalRead(GPIO_PINS[ch]));
  String state = digitalRead(GPIO_PINS[ch]) == HIGH ? "ON" : "OFF";
  addEvent("CH" + String(ch+1) + " set " + state + " by " + sessionRole);
  sendJson(200, "{\"ok\":true,\"state\":\"" + state + "\"}");
}
void handleEvents() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  sendJson(200, "{\"ok\":true,\"text\":\"" + jsonEscape(eventLogText()) + "\"}");
}
void handleLogout() {
  if (!isAuthenticated()) { sendUnauthorized(); return; }
  addEvent("Web logout: " + sessionRole);
  sessionToken = ""; sessionRole = "";
  sendJson(200, "{\"ok\":true}");
}
void handleNotFound() { server.send(404, "text/plain", "404 :: route not found"); }

void setup() {
  Serial.begin(115200);
  delay(100);
  bootMillis = millis();
  randomSeed(ESP.getChipId() ^ micros());
  for (uint8_t i=0;i<GPIO_COUNT;i++) { pinMode(GPIO_PINS[i], OUTPUT); digitalWrite(GPIO_PINS[i], LOW); }
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  beepBoot();
  Serial.println("\n\nNEXUS: PHANTOM CORE v1.0 booting...");
  addEvent("System boot");
  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS mount failed. Upload data folder / check flash layout.");
    addEvent("ERROR: LittleFS mount failed");
  } else {
    Serial.println("[FS] LittleFS mounted");
    addEvent("LittleFS mounted");
  }
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname("NEXUS-PHANTOM-CORE");
  bool apOK = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.println(String("[AP] ") + (apOK ? "started" : "FAILED"));
  Serial.println("[AP] SSID: " + String(AP_SSID));
  Serial.println("[AP] Password: " + String(AP_PASSWORD));
  Serial.println("[AP] URL: http://" + ipString(WiFi.softAPIP()) + "/");
  addEvent(String("Fallback AP started: ") + AP_SSID);
  startWiFiStation();

  server.collectHeaders("X-Session-Token");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/wifiscan", HTTP_GET, handleWifiScan);
  server.on("/api/cmd", HTTP_POST, handleCommand);
  server.on("/api/gpio", HTTP_POST, handleGpio);
  server.on("/api/events", HTTP_GET, handleEvents);
  server.on("/api/logout", HTTP_POST, handleLogout);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Server started on port 80");
  addEvent("HTTP console started");
}
void loop() {
  server.handleClient();
  maintainWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    static bool announced = false;
    if (!announced) {
      announced = true;
      Serial.println("[WIFI] Connected to " + WiFi.SSID());
      Serial.println("[WIFI] STA URL: http://" + ipString(WiFi.localIP()) + "/");
      addEvent("Router Wi-Fi connected; STA IP " + ipString(WiFi.localIP()));
    }
  }
  yield();
}
