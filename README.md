# CANBridge

A cross-platform desktop application for analyzing and simulating CAN bus traffic over TCP/IP networks. CANBridge acts as a software bridge — no CAN hardware required — letting you send, receive, and inspect CAN frames between machines or processes using standard TCP connections.

**Version:** 0.3.0 — © 2026 Ismail KARROUD

---

## Features

### Connection
- **Server mode** — listen on a configurable TCP port, accept multiple clients simultaneously
- **Client mode** — connect to a remote CANBridge server
- **CAN bus type** — Classic (8 B payload), FD (up to 64 B), XL (up to 2048 B)
- **ID format** — Standard (11-bit) or Extended (29-bit)
- **Auto settings sync** — server pushes bus type and ID format to clients on connect or change
- **Downgrade protection** — warns about conflicting frames in Analyzer/Simulator before applying a type downgrade

### Simulator
- Compose and send CAN frames with configurable ID, DLC, and data payload
- **One-shot** or **periodic** transmission per frame (configurable interval, min 10 ms)
- Manage multiple frames simultaneously; hide/show or remove individual frames
- Automatic DLC snap to nearest valid CAN-FD value
- Duplicate CAN ID detection
- **Send All / Stop All** global controls

### Analyzer
- Real-time capture of transmitted and received frames (TX/RX)
- Filter by direction, CAN ID (hex substring), or data payload (hex substring)
- Three timestamp formats: Time Only, Date & Time, Raw Milliseconds
- Configurable frame history cap (500 to unlimited, default 10 000)
- **CSV export and import** (7-column format including FrameType and IDFormat)
- Auto-scroll toggle

### Log
- Categorized event log: Server, Client, Frame events
- Filter by category, export to `.txt`, configurable max entries (default 100)

### Project Files
- Save and restore the full simulator state and connection settings as a `.cbp` JSON file
- Preserves frame IDs, data, DLC, periodic state, interval, and visibility

### UI
- Dark / Light / Auto theme support
- Tabbed interface: Connection, Log, Simulator, Analyzer
- Keyboard shortcuts for file operations (Cmd/Ctrl + N/O/S)

---

## Requirements

| Requirement | Version |
|---|---|
| Qt | 6.5 or later |
| Qt modules | Core, Widgets, Network |
| CMake | 3.19 or later |
| C++ standard | C++17 |

---

## Building

The CMake commands are identical on all platforms — only the Qt installation path differs.

**macOS**
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/macos
cmake --build build
cmake --install build   # produces a .app bundle
```

**Windows** (run in a Developer Command Prompt or PowerShell)
```bash
cmake -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64
cmake --build build --config Release
cmake --install build   # produces a standalone .exe with version metadata
```

**Linux**
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
cmake --build build
cmake --install build
```

The `--install` step runs the Qt deployment script that bundles all required Qt libraries alongside the binary.

---

## Project Structure

```
CANBridge/
├── src/
│   ├── core/
│   │   ├── CANFrame.h / .cpp          # Frame data structure, wire format, FD DLC table
│   ├── network/
│   │   ├── tcpserver.h / .cpp         # TCP server — broadcast, periodic relay
│   │   └── tcpclient.h / .cpp         # TCP client — connect, two-phase frame parsing
│   ├── models/
│   │   ├── messagemodel.h / .cpp      # Qt table model for captured frames
│   │   └── framefilterproxymodel.h / .cpp  # Filter proxy (direction, ID, data)
│   └── ui/
│       ├── mainwindow.h / .cpp        # Main window, tab logic, settings management
│       ├── mainwindow_menubar.cpp     # Menu bar, theme, file operations
│       ├── mainwindow.ui              # Qt Designer layout
│       ├── framewidget.h / .cpp       # Per-frame simulator widget
│       └── aboutdialog.h / .cpp       # About dialog
├── resources/
│   ├── logo.png / .icns / .ico        # Application icons
├── CMakeLists.txt
├── Info.plist.in                      # macOS bundle metadata template
└── version.rc.in                      # Windows version resource template
```

---

## Wire Protocol

### CAN Frame packet (18-byte header + variable payload)

```
Byte offset   Field       Size   Description
0             CanType     1 B    0=Classic  1=FD  2=XL
1             IdFormat    1 B    0=Standard 1=Extended
2–5           CAN ID      4 B    Big-endian
6–7           DLC         2 B    DLC code
8–9           DataLen     2 B    Actual payload byte count
10–17         Timestamp   8 B    Milliseconds since epoch
18+           Data        N B    Payload (N = DataLen)
```

### Settings packet (3 bytes)

Sent by the server to every client on connect and on any bus-type/format change.

```
[0xFF][CanType][IdFormat]
```

---

## CSV Format

Frames exported from the Analyzer use a 7-column format:

```
# CANBridge Frames v1
Timestamp,Dir,ID,DLC,Data,FrameType,IDFormat
12:34:56.789,TX,0x123,8,01 02 03 04 05 06 07 08,Classic,Standard
```

5-column files from older versions are still importable (defaulting to Classic/Standard).

---

## Project File Format (`.cbp`)

```json
{
  "version": 1,
  "connection": {
    "canType": 0,
    "idFormat": 0,
    "serverPort": 5555,
    "clientHost": "127.0.0.1",
    "clientPort": 5555
  },
  "simulatorFrames": [
    {
      "id": 256,
      "data": "00 01 02 03",
      "dlc": 4,
      "periodicEnabled": true,
      "intervalMs": 100,
      "visible": true
    }
  ]
}
```

---

## License

Copyright © 2026 Ismail KARROUD. All rights reserved.
