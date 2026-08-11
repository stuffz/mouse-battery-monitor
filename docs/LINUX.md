# Linux Notes

## How device access works

Battery reads use HID feature reports. On Linux those go through `hidraw`:

- `HIDIOCSFEATURE` sends the request (report ID in byte 0)
- `HIDIOCGFEATURE` reads the response

which map one-to-one onto the Windows `HidD_SetFeature` / `HidD_GetFeature` pair, so the protocol code in `src/devices/` is shared between both platforms.

### Picking the right node

A mouse or dongle exposes several `/dev/hidraw*` nodes. Only the one carrying the vendor collection answers battery commands:

| Vendor        | Usage page | Usage |
| ------------- | ---------- | ----- |
| Endgame Gear  | `0xFF01`   | `0x02`|
| VAXEE         | `0xFF05`   | `0x01`|

Windows exposes each top-level HID collection as its own device interface and the device code filters on those values. Linux exposes one node per USB interface with every collection inside it, so `hid_device_linux.hpp` parses each node's report descriptor from sysfs and reports one entry per top-level application collection. The selection logic in `src/devices/` is shared with Windows.

To see this by hand:

```bash
# find the nodes belonging to your device
grep -l 'HID_ID=0003:00003057' /sys/class/hidraw/hidraw*/device/uevent

# dump a node's report descriptor
xxd /sys/class/hidraw/hidraw2/device/report_descriptor
```

`06 05 FF 09 01 A1 01` in that dump is Usage Page `0xFF05`, Usage `0x01`, Collection (Application) — the VAXEE vendor interface.

## Permissions

`/dev/hidraw*` is `root:root 0600` by default, so an unprivileged process gets `Permission denied` and the monitor shows "No device connected".

```bash
sudo make install-udev-rules
```

installs `/etc/udev/rules.d/70-mouse-battery-monitor.rules`, which tags the Endgame Gear (`3367`) and VAXEE (`3057`) nodes with `uaccess`. systemd-logind then adds an ACL for the user logged in at the seat. Verify with:

```bash
getfacl /dev/hidraw2
./build/release/mouse-battery-monitor --once --debug
```

If the tag has not taken effect, replug the device or run:

```bash
sudo udevadm trigger --subsystem-match=hidraw --action=change
```

On a system without logind, swap `TAG+="uaccess"` in the rule for `MODE="0660", GROUP="plugdev"` and add yourself to that group.

## Tray icon

The tray icon is a `QSystemTrayIcon`, which Qt implements over the StatusNotifierItem D-Bus protocol on Linux. KDE Plasma, XFCE, Waybar, i3status and similar hosts support it directly. GNOME needs an AppIndicator extension — without a host the process still runs and logs normally, it just has no icon.

## Hotplug

A libudev monitor watches the `hidraw` subsystem. Any add/remove is debounced (1500 ms for arrivals, 100 ms for removals) and then triggers a rescan, with up to three retries three seconds apart while a freshly-plugged device is still settling. This mirrors the `WM_DEVICECHANGE` handling on Windows.

Because the monitor uses the `udev` netlink group rather than the raw kernel group, events arrive after udev has applied the access rules, so a newly plugged dongle is already readable.

## Single instance

Tray mode takes an exclusive `flock` on `$XDG_RUNTIME_DIR/mouse-battery-monitor.lock` (falling back to the state directory when `XDG_RUNTIME_DIR` is unset), so enabling the systemd unit *and* the desktop autostart entry cannot produce two tray icons.

A second instance prints `Mouse Battery Monitor is already running (pid N)` and exits **0**, not non-zero — the desired state already holds, and failing would send a `Restart=on-failure` unit into a restart loop on every double-start.

`flock` is tied to the open file description, so the kernel releases it when the process dies for any reason, including `SIGKILL`. There is no stale lock to clean up and no pid-liveness guess that can go wrong; a leftover lock *file* is harmless and the next instance simply takes ownership.

`--once` is deliberately **not** guarded, so a status bar can poll while the tray is running. The two do send HID feature reports to the same device independently, so a poll that lands exactly between the tray's request and its response can occasionally read the wrong value and report a miss. Harmless, but if you rely on `--once` prefer running it *instead of* the tray rather than alongside it.

## Sleeping mice

A wireless mouse that has gone to sleep keeps its `hidraw` node but stops answering feature reports. The monitor keeps the last known percentage instead of flickering to "disconnected"; move the mouse and the next poll picks it up.

## Paths

| What      | Location                                                     |
| --------- | ------------------------------------------------------------ |
| Config    | `$XDG_CONFIG_HOME/mouse-battery-monitor/config.ini`, then next to the binary |
| Log       | `$XDG_STATE_HOME/mouse-battery-monitor/battery_monitor.log`   |
| Resources | `MBM_RESOURCE_DIR`, then `<bindir>/resources`, then `<prefix>/share/mouse-battery-monitor/resources` |

`MBM_CONFIG` and `MBM_RESOURCE_DIR` override the config file and resource directory respectively.

## Why not just run the Windows build under Wine?

It does not work, for three independent reasons:

1. **Wine hides HID mice and keyboards.** `winebus.sys` gates hidraw devices behind `is_hidraw_enabled` and logs `Ignoring unsupported VVVV:PPPP hidraw mouse/keyboard`. A mouse or dongle is never surfaced to the Windows side unless it is added to Wine's per-VID/PID allowlist under the `winebus` service parameters.
2. **Wine does not split top-level collections into separate device interfaces.** Windows HIDCLASS creates one device interface per top-level collection, which is why the Windows code can filter on usage page `0xFF05`. Wine creates a single device per hidraw node carrying one usage pair, which for these devices is the mouse collection `0001:0002` — so the vendor interface the battery protocol needs is never matched.
3. **The permission problem is unchanged.** Wine runs as your user, so `/dev/hidraw*` still needs the udev rule above.

The tray icon itself would likely work (Wine's XEmbed tray bridged through Plasma's `xembedsniproxy`), but the device never appears, so there is nothing to display.

## Status bar integration

`--once` prints a single reading and exits, which is enough for a polled bar widget:

```bash
$ mouse-battery-monitor --once
VAXEE 4K Dongle (Dual-track) (Wireless): 75%
```

Exit status is 0 on a successful read and 1 when no device was found or the device did not answer.
