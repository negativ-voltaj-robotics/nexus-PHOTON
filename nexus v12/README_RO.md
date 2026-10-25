# NEXUS: PHANTOM CORE v1.0

A red/black, terminal-first local control console for an ESP8266 NodeMCU.

## Package contents

- `NEXUS_PHANTOM_CORE.ino` — firmware
- `data/index.html` — web console served from LittleFS
- `README_RO.md` — Romanian setup guide
- `COMMANDS.md` — command reference
- `HARDWARE.md` — pins and electrical notes

## Implemented features

1. Web login with `admin` and `user` roles.
2. Session token for API requests.
3. Fallback Wi-Fi access point, active alongside station Wi-Fi.
4. Router Wi-Fi connection attempt with retry.
5. Current SSID, IP, RSSI, uptime and free-heap telemetry.
6. Nearby Wi-Fi access-point scan: SSID, signal, channel, open/protected.
7. Four external GPIO outputs.
8. Web ON/OFF/TOGGLE controls for each output.
9. Mini command interpreter with 20+ commands/aliases.
10. RAM event journal.
11. Boot buzzer pattern.
12. Admin-success buzzer pattern.
13. Buzzer alarm after 3 consecutive failed web logins.
14. Safe scene that turns all four outputs off.
15. Admin-only Wi-Fi scan and reboot commands.
16. Responsive terminal-style UI with bright red accent and dark contrast.
17. No ArduinoJson dependency.
18. LittleFS fallback message if the HTML file was not uploaded.

## Defaults — change these before serious use

In the first lines of `NEXUS_PHANTOM_CORE.ino`, edit:

```cpp
const char* WIFI_SSID     = "CHANGE_WIFI_NAME";
const char* WIFI_PASSWORD = "CHANGE_WIFI_PASSWORD";
const char* ADMIN_PASSWORD = "ChangeAdminPassword";
const char* USER_PASSWORD  = "ChangeUserPassword";
```

Login usernames are `admin` and `user`. The fallback AP is `NEXUS-PHANTOM-CORE`, password `NexusCore8266`.

## Upload

1. Extract the ZIP.
2. Open `NEXUS_PHANTOM_CORE.ino` in Arduino IDE.
3. Select a NodeMCU / ESP8266 board and the correct COM port.
4. Click **Verify**, then **Upload**.
5. Upload the entire `data` folder to LittleFS using the ESP8266 LittleFS Data Upload tool that matches your Arduino IDE installation.
6. Open Serial Monitor at **115200 baud**. The firmware prints the AP URL and, if connected to the router, its station URL.

Fallback console: `http://192.168.4.1/`

If connected to your router, use the station IP printed in Serial Monitor or shown in your router's DHCP/client list.

## Hardware map

- CH1: D1 / GPIO5
- CH2: D2 / GPIO4
- CH3: D5 / GPIO14
- CH4: D6 / GPIO12
- Buzzer: D7 / GPIO13

GPIO outputs are 3.3 V logic. Do not connect a relay coil, motor, lamp or other high-current load directly to an ESP8266 pin. Use a suitable relay module / transistor driver and a suitable power supply. Verify whether your relay module is active-high or active-low before connecting a real load. Use a transistor driver for buzzers that draw more than a few mA.

## Network/security limitations

- The Wi-Fi scan lists nearby wireless access points. It is not a list of devices connected to your router.
- The ESP8266 alone cannot provide a reliable full LAN client inventory, capture all router traffic, or read the router's Wi-Fi authentication logs. Add router API/log integration or use a server for those capabilities.
- The login alarm detects failed logins to this web console only; it does not detect people failing to join the router's Wi-Fi.
- HTTP is unencrypted and the session system is intentionally lightweight. Keep it on a trusted LAN, change default passwords, and never port-forward it directly to the internet.
- Event log is held in RAM and clears on reboot.
- The shell is a small command interpreter, not a real Linux shell. It cannot execute arbitrary OS commands.

## Troubleshooting

- **COM port access denied:** close Serial Monitor/Plotter and other serial tools, unplug/replug the board, select the port again. This is a Windows port-lock issue, not a firmware compile error.
- **Page says `index.html` missing:** firmware uploaded, but LittleFS assets have not been uploaded. Upload `data/index.html`.
- **Fallback AP works but router Wi-Fi doesn't:** confirm SSID/password are correct and the router offers 2.4 GHz Wi-Fi. ESP8266 does not support 5 GHz.
- **No buzzer sound:** check active/passive buzzer type, polarity and driver circuit. This firmware drives D7 HIGH for a beep; some buzzer modules are active-low and need wiring or code adjustment.
- **Outputs behave inverted:** your relay module may be active-low; change output logic only after testing safely with the loads disconnected.

Firmware and UI are provided as a starter project. The source has not been compiled or tested on the user's physical board in this environment.
