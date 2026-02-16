# Endgame Gear - Battery Protocol Documentation

## Overview

Endgame Gear wireless mice use HID feature reports to communicate battery status. Communication happens either directly with the mouse (when connected via USB cable) or through the wireless dongle.

## HID Device Identification

| Property   | Value  |
| ---------- | ------ |
| Vendor ID  | 0x3367 |
| Usage Page | 0xFF01 |
| Usage      | 0x0002 |

## Supported Devices

| Device                    | PID    | Connection |
| ------------------------- | ------ | ---------- |
| Endgame Gear OP1W         | 0x1972 | Wired USB  |
| Endgame Gear XM2W v2      | 0x1982 | Wired USB  |
| Endgame Gear Dongle       | 0x1970 | Wireless   |

When a mouse is connected via USB cable, it is detected by its wired PID. When using wireless mode, communication goes through the dongle PID (0x1970).

## Battery Read Protocol

Battery status is read using HID feature reports with a 64-byte buffer.

### Request

| Byte | Value | Description      |
| ---- | ----- | ---------------- |
| 0    | 0xA1  | Report ID        |
| 1    | 0xB4  | Battery command  |
| 2-63 | 0x00  | Padding          |

The request is sent as a HID Set Feature Report.

### Response

The response is read as a HID Get Feature Report with Report ID 0xA1.

| Byte | Description              |
| ---- | ------------------------ |
| 0    | Report ID (0xA1)         |
| 1    | Status (0x01 or 0x08)    |
| 16   | Battery percentage (0-100) |

### Response Validation

- `byte[1]` must be `0x01` or `0x08` for a valid response.
- The battery percentage at `byte[16]` is clamped to 100.

### Timing

The protocol requires two consecutive read cycles. The first read is discarded as a wake-up, and the second contains actual data.

1. Send feature report (battery command)
2. Wait **350ms**
3. Read feature report (discard result)
4. Wait **100ms**
5. Send feature report (battery command)
6. Wait **350ms**
7. Read feature report (use this result)

### Charging Detection

Charging status is inferred from the connection mode:
- **Wired PIDs** (mouse connected via USB) → Charging
- **Dongle PID** (wireless connection) → Not charging
