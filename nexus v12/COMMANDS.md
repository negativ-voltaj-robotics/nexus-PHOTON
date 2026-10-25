# NEXUS PHANTOM CORE — command reference

Commands are typed into the web console. Use `help` on the device for the live list.

| Command | Action |
|---|---|
| `help` / `?` | List commands |
| `status` | Combined system and network status |
| `netinfo` | Station IP, gateway, subnet, DNS, AP details |
| `ip` | Alias for `netinfo` |
| `wifi` | Router connection, SSID, IP and RSSI |
| `wscan` | Scan nearby Wi-Fi access points (admin) |
| `scan` | Alias for `wscan` |
| `wifi scan` | Alias for `wscan` |
| `gpio list` | Show all four output states |
| `gpio 1 on` | Turn channel 1 on (channels 1–4) |
| `gpio 1 off` | Turn channel 1 off |
| `gpio 1 toggle` | Toggle channel 1 |
| `ch1 on` / `ch1 off` / `ch1 toggle` | Short channel syntax (also `ch2`–`ch4`) |
| `alloff` | Turn all four outputs off |
| `scene safe` | Alias for `alloff` |
| `beep` | Play buzzer test sequence |
| `log` | Print recent event log |
| `clear` | Clear the terminal view |
| `uptime` | Show time since boot |
| `heap` | Show free heap memory |
| `whoami` | Show current role |
| `echo some text` | Print text |
| `about` | Version and capability notes |
| `logout` | Invalidate the current session |
| `reboot` | Restart ESP8266 (admin only) |

Some commands have aliases, so the list contains more than 20 usable command forms. GPIO control is available to both roles; Wi-Fi scan and reboot are admin-only.
