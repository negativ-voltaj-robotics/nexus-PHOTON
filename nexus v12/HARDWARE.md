# Hardware notes

## Pin allocation

| Function | NodeMCU label | ESP8266 GPIO | Notes |
|---|---|---:|---|
| Channel 1 | D1 | 5 | 3.3 V logic |
| Channel 2 | D2 | 4 | 3.3 V logic |
| Channel 3 | D5 | 14 | 3.3 V logic |
| Channel 4 | D6 | 12 | 3.3 V logic |
| Buzzer signal | D7 | 13 | HIGH = beep in this firmware |

## Safe wiring

- Power down before changing wiring.
- Connect signal ground and controller ground when using an external driver, following that module's instructions.
- Never power motors, relay coils or lamps directly from ESP8266 GPIO.
- Use a suitable transistor / relay module and power supply.
- Test outputs with loads disconnected first.
- Check if relay inputs are active-high or active-low; modules differ.
- A buzzer module may need a transistor if its current exceeds the ESP8266 pin's safe output current.
