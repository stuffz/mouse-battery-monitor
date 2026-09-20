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
| Endgame Gear XM2w 4K V1   | 0x1968 | Wired USB  |
| Endgame Gear Dongle       | 0x1970 | Wireless   |
| Endgame Gear OP1W         | 0x1972 | Wired USB  |
| Endgame Gear XM2W v2      | 0x1982 | Wired USB  |

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

| Byte  | Description                          |
| ----- | ------------------------------------ |
| 0     | Report ID (0xA1)                     |
| 1     | Status (see below)                   |
| 16    | Battery percentage (0-100, steps of 5) |
| 17-18 | Battery voltage in mV, little endian |

The percentage is the firmware's own estimate from the cell voltage. It is not
computed the same way on both paths: the same battery read 75-80 wired while
charging at 4.15 V and 85 via the dongle at 4.02 V a minute later. Endgame's
configuration tool hides the wired value and shows the text "Charging" instead
whenever it is below 100.

### Status Byte

| Value | Meaning                                                              |
| ----- | -------------------------------------------------------------------- |
| 0x01  | Fresh reply, payload valid                                           |
| 0x03  | Busy, retry after a longer wait                                      |
| 0x07  | Command not supported on this path (e.g. dongle-only command, wired) |
| 0x08  | Mouse unreachable (asleep or switched off), payload is stale         |

On 0x08 the dongle returns its reply buffer unchanged from the last successful
command, so `byte[16]` still holds the previous percentage. The mouse sleeps
after roughly four minutes idle, after which every poll gets 0x08 until it is
moved. Treating 0x08 as valid froze the tray at the last live value for as long
as the mouse was idle.

### Response Validation

- `byte[1]` must be `0x01` for a valid response.
- `0x08` is a sleeping mouse: keep the last known status, do not parse the payload.
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
