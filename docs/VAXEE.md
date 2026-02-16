# VAXEE - Battery Protocol Documentation

## Overview

VAXEE wireless mice use a command-based HID feature report protocol. A fixed header byte (0xA5) prefixes every command. Battery level and charging status are read using separate commands.

## HID Device Identification

| Property   | Value  |
| ---------- | ------ |
| Vendor ID  | 0x3057 |
| Usage Page | 0xFF05 |
| Usage      | 0x01   |
| Report ID  | 0x0E   |

## Supported Devices

### Dongles (Wireless Receivers)

| Device                            | PID    |
| --------------------------------- | ------ |
| VAXEE Dongle                      | 0x1001 |
| VAXEE 4K Dongle                   | 0x1002 |
| VAXEE Dongle                      | 0x0005 |
| VAXEE 4K Dongle (Dual-track)      | 0x2001 |

### Mice (Direct USB Connection)

| Device                             | PID    | Sensor  |
| ---------------------------------- | ------ | ------- |
| VAXEE XE Wireless                  | 0x1003 | PAW3395 |
| ZYGEN NP-01S Wireless              | 0x1004 | PAW3395 |
| VAXEE AX Wireless                  | 0x1005 | PAW3395 |
| ZYGEN NP-01 Wireless               | 0x1006 | PAW3395 |
| VAXEE XE-S Wireless                | 0x1007 | PAW3950 |
| VAXEE XE-S-L Wireless              | 0x1008 | PAW3950 |
| VAXEE x NINJUTSO Sora Wireless     | 0x1009 | PAW3950 |
| VAXEE E1 Wireless                  | 0x1010 | PAW3950 |
| ZYGEN NP-01S V2 Wireless           | 0x1011 | PAW3950 |
| VAXEE XE V2 Wireless               | 0x1012 | PAW3950 |
| ZYGEN NP-01S Ergo Wireless         | 0x1013 | PAW3950 |

## Command Structure

All commands use a 64-byte HID feature report buffer (1 byte Report ID + 63 bytes data).

### Send Format

| Byte | Description        |
| ---- | ------------------ |
| 0    | Report ID (0x0E)   |
| 1    | Header (0xA5)      |
| 2    | Command ID         |
| 3    | Read/Write flag    |
| 4    | Data length        |
| 5+   | Data payload       |

### Read/Write Flags

| Value | Meaning |
| ----- | ------- |
| 0x01  | Read    |
| 0x02  | Write   |

### Response Format

| Byte | Description           |
| ---- | --------------------- |
| 0    | Report ID (0x0E)      |
| 1    | Header (0xA5)         |
| 2    | Command ID echo       |
| 3    | ACK type              |
| 4    | Data length           |
| 5+   | Response data         |

### Response Validation

- `byte[2]` (Command ID echo) must be non-zero for a valid response.

## Battery Level Read

**Command ID:** 0x0B

### Request

| Byte | Value | Description      |
| ---- | ----- | ---------------- |
| 0    | 0x0E  | Report ID        |
| 1    | 0xA5  | Header           |
| 2    | 0x0B  | CMD: Battery Level |
| 3    | 0x01  | Read             |
| 4    | 0x01  | Data Length       |
| 5-63 | 0x00  | Padding          |

### Response

| Byte | Description                  |
| ---- | ---------------------------- |
| 5    | Battery level (0-20, multiply by 5 for percentage) |

Battery level is reported in 5% increments (0, 5, 10, ..., 95, 100).

### Timing

1. Send feature report
2. Wait **100ms**
3. Read feature report

## Battery Charging Status

**Command ID:** 0x10

### Request

| Byte | Value | Description           |
| ---- | ----- | --------------------- |
| 0    | 0x0E  | Report ID             |
| 1    | 0xA5  | Header                |
| 2    | 0x10  | CMD: Charging Status  |
| 3    | 0x01  | Read                  |
| 4    | 0x01  | Data Length            |
| 5-63 | 0x00  | Padding               |

### Response

| Byte | Value | Description        |
| ---- | ----- | ------------------ |
| 5    | 0x00  | Not charging       |
| 5    | ≠0    | Charging           |
