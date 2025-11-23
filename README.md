# Endgame Gear Mouse Battery Monitor

System tray battery monitor for Endgame Gear wireless mice.

## Features

- Real-time battery percentage display in system tray
- Customizable PNG icons for different battery levels
- Low battery notifications
- Configurable update interval
- Minimal resource usage (~2-5MB RAM)

## Prerequisites

### Installing Tools with Scoop

If you don't have the build tools installed, you can use [Scoop](https://scoop.sh/) to install them:

```powershell
# Install MinGW (includes GCC)
scoop install gcc

# Install Make
scoop install make
```

## Building

Requires GCC/MinGW with C++17 support.

```bash
make
```

Build with debug mode (shows console):
```bash
make DEBUG=1
```

The application runs in the system tray. Right-click the icon for options.

## Configuration

Edit `config.ini`:

- `update_interval_seconds` - How often to check battery (default: 10 seconds)
- `show_notifications` - Enable/disable notifications (default: true)
- `low_battery_threshold` - Battery % for low warning (default: 20%)
- `debug_mode` - Show console window and verbose logging (default: false)

## Supported Devices

- Endgame Gear OP1W (VID: 0x3367, PID: 0x1970/0x1972)

## Development

Build targets:
- `make` - Build release version (always clean build)
- `make DEBUG=1` - Build debug version
- `make clean` - Clean build artifacts
- `make run` - Build and run
- `make help` - Show all targets

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Icons are from the Bitsies icon pack by Recep Kutuk, licensed under CC BY 4.0 - see [ATTRIBUTION.md](ATTRIBUTION.md) for details.
