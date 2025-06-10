# A9G GPS Tracker

A GPS tracking solution built on the A9G module, implementing the OsmAnd tracking protocol for compatibility with Traccar and other tracking servers.

## Overview

The A9G GPS Tracker is a compact, reliable GPS tracking solution built on the A9G module, which integrates GSM and GPS capabilities. The system offers real-time location tracking with data reporting via HTTP/HTTPS to a configurable server. It implements the OsmAnd tracking protocol, making it fully compatible with Traccar and other open-source tracking server platforms.

## Features

- **Real-time GPS Tracking**: Provides accurate location data with satellite information
- **GSM/GPRS Connectivity**: Transmits location data to a remote server
- **OsmAnd Protocol Support**: Compatible with Traccar and other tracking servers
- **HTTP/HTTPS Support**: Secure data transmission options
- **SMS Control**: Receive commands and send location data via SMS
- **Battery Monitoring**: Reports battery level with location data
- **Configurable Settings**: Customizable server settings, reporting intervals, and more
- **UART Command Interface**: Interactive command-line interface for configuration and diagnostics
- **Status LEDs**: Visual indicators for GPS fix and GSM connectivity
- **Cell Tower Information**: Reports cell tower data when available

## Hardware Requirements

- A9G GPS+GSM module
- SIM card with data plan
- Power supply (battery or external)
- Antenna for GSM and GPS

## Documentation

- [Developer Guide](doc/DEVELOPER_GUIDE.md) - Technical documentation for developers

## Building the Application

The application is located in the `app` folder. To build it, follow these steps:

### 1. Download and Install the Toolchain

- Toolchain link: [http://test.ai-thinker.com/csdk/CSDTK42.7z](http://test.ai-thinker.com/csdk/CSDTK42.7z)
- Decompress the archive to a folder, for example: `C:\CSDTK`
- Run the `config_env_admin.bat` file in the CSDTK directory to set the environment variables.

### 2. Get the SDK with the Application

Clone the gps tracker app repository:

```bash
git clone https://github.com/NewoPL/A9G-GPS-Tracker.git
```

### 3. Build the Application

Navigate to the project directory and run:

```bash
./build.bat app
```

After compilation, a build folder will be generated with the target files for flashing to the A9G board.

### 4. Flash to the A9G Board
Inside the build/hex folder there are two .lod files: one with _B*.lod and one with *_flash.lod.
Flash the larger file the first time or when updating the SDK version.
For subsequent uploads, you can flash the smaller file to reduce download time.

## Getting Started

### First-time Setup

1. Insert a valid SIM card into the module
2. Power on the device
3. Wait for the system to initialize (GSM LED will start blinking)
4. Use UART commands to configure the device (see Configuration section)

### Quick Start

1. Build and flash the firmware as described above
2. Configure the tracker:
   - Connect to the device via UART (115200 baud)
   - Set required parameters:
     ```
     set device_name YOUR_DEVICE_ID
     set server demo.traccar.org
     set port 5055
     set protocol http
     set apn YOUR_APN_NAME
     ```

3. Configure your Traccar server:
   - Add a new device with protocol "OsmAnd"
   - Use the same device identifier as configured on the tracker

### Status Indicators

The device has two status LEDs:

- **GPS Status LED (GPIO27)**:
  - OFF: GPS not active or no fix
  - Blinking: GPS active and has a valid fix

- **GSM Status LED (GPIO28)**:
  - OFF: GSM not registered or not active
  - Blinking: GSM registered and active (connected to network)

## Configuration

### UART Command Interface

Connect to the device via UART at 115200 baud rate to access the command interface.

### Available Commands

| Command         | Syntax                              | Description                                                      |
|-----------------|-------------------------------------|------------------------------------------------------------------|
| help            | help                                | Show help message with available commands                        |
| set             | set <param> [value]                 | Set a configuration parameter                                    |
| get             | get [param]                         | Get configuration parameter(s)                                   |
| ls              | ls [path]                           | List files in directory                                          |
| rm              | rm <file>                           | Remove a file                                                    |
| tail            | tail <file> [bytes]                 | Show last bytes of file                                          |
| net activate    | net activate                        | Activate (attach and activate) the network                       |
| net deactivate  | net deactivate                      | Deactivate (detach and deactivate) the network                   |
| net status      | net status                          | Show network status                                              |
| sms             | sms                                 | Show SMS storage info                                            |
| sms ls          | sms ls <all\|read\|unread>         | List SMS messages (all/read/unread)                              |
| sms rm          | sms rm <index\|all>                 | Remove SMS message by index or all messages                      |
| location        | location                            | Show the last known GPS position                                 |
| restart         | restart                             | Restart the system immediately                                   |

### Configuration Parameters

| Parameter    | Description                         | Example Values                      |
|--------------|-------------------------------------|-------------------------------------|
| device_name  | Device identifier                   | tracker_01                          |
| server       | Server hostname or IP               | demo.traccar.org                    |
| port         | Server port                         | 80, 443, 5055, 5055                 |
| protocol     | Server protocol                     | http, https                         |
| apn          | Cellular APN                        | internet, wap                       |
| apn_user     | APN username (if required)          | user                                |
| apn_pass     | APN password (if required)          | password                            |
| log_level    | Logging detail level                | none, error, warn, info, debug      |
| log_output   | Where logs are written              | uart, trace, file                   |
| gps_uere     | GPS accuracy multiplier             | 3.0, 5.0                            |
| gps_logging  | Enable GPS NMEA logging             | true, false                         |

## Data Format

### Server Data Format (OsmAnd Protocol)

When reporting to the server, the device sends an HTTP/HTTPS POST request using the OsmAnd tracking protocol format with the following parameters:

```
id=DEVICE_NAME&valid=1&timestamp=1717667421&lat=37.7749&lon=-122.4194&speed=0&bearing=90.0&altitude=10.0&accuracy=15.0&cell=310410,12345,67890,-85&batt=85
```

This format is compatible with Traccar server (https://www.traccar.org) and other tracking platforms that support the OsmAnd protocol.

| Parameter  | Description                                      |
|------------|--------------------------------------------------|
| id         | Device identifier (as configured)                 |
| valid      | GPS fix validity (1=valid, 0=invalid)             |
| timestamp  | Unix timestamp from GPS data                      |
| lat        | Latitude in decimal degrees                       |
| lon        | Longitude in decimal degrees                      |
| speed      | Speed in knots                                    |
| bearing    | Direction of travel in degrees                    |
| altitude   | Altitude in meters                                |
| accuracy   | Estimated accuracy in meters                      |
| cell       | Cell tower info (MCC+MNC,LAC,CellID,RxLev)        |
| batt       | Battery level percentage                          |

## Advanced Features

### Traccar Server Integration

This device is fully compatible with Traccar, an open-source GPS tracking server:
- To use with Traccar, set your `server` parameter to your Traccar server address
- Set the `port` parameter to match your Traccar server's port (default: 5055)
- In Traccar, add a new device with the protocol set to "OsmAnd"
- Use the same device identifier in both Traccar and the tracker configuration

For more details on Traccar setup, visit: https://www.traccar.org/documentation/

### APN Re-activation Workaround

The firmware includes a workaround for a known issue with APN re-activation:
- The system uses a dummy APN cycle to work around firmware limitations when re-activating the network
- This process is automatic and handled internally by the device

### Cell Tower Information

When available, the device reports cell tower information alongside GPS data:
- Format: MCC+MNC,LAC,CellID,RxLev
- This can be used for approximate location when GPS fix is unavailable

## License

[GNU General Public License (GPL)](LICENSE)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
