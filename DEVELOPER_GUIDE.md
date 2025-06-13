# A9G GPS Tracker - Developer Guide

This guide provides an overview of the firmware architecture, focusing on the main application files, functional blocks, and their interactions.

---

## 1. Application File Overview

| File                        | Description |
|-----------------------------|-------------|
| **system.h / system.c**     | System initialization, status flags, macros, and main event loop. |
| **gps_tracker.h / .c**      | GPS hardware control, data parsing, tracker task, and location reporting. |
| **network.h / .c**          | GSM network management, cell info, LBS (cell-based location), and watchdog. |
| **config_store.h / .c**     | Persistent configuration storage and access. |
| **config_commands.h / .c**  | UART command parsing, command table, and command handlers. |
| **http.h / .c**             | HTTP/HTTPS client for server communication. |
| **sms_service.h / .c**      | SMS command processing and location reporting via SMS. |
| **debug.h / .c**            | Logging utilities with tags and timestamps. |
| **utils.h / .c**            | Utility functions (string, time, etc). |
| **led_handler.h / .c**      | (If present) LED status indicator logic. |

---

## 2. Main Functional Blocks

### 2.1 System Initialization
- Initializes hardware and software modules.
- Sets up main tasks and event handlers.
- Maintains system status flags (e.g., GSM active, GPS on).

### 2.2 GPS Tracker
- Controls GPS hardware and parses NMEA data.
- Maintains a `GpsTrackerData_t` struct with the latest location, speed, bearing, altitude, and accuracy.
- Periodically sends location data to the server if network is available.

### 2.3 Network Management
- Handles GSM registration, attach/activate, and network watchdog.
- Requests and stores cell info for LBS (Location Based Service) fallback.
- Provides blocking LBS location retrieval for UART commands.

### 2.4 Configuration Management
- Loads and saves configuration parameters (APN, server, device name, etc) to flash.
- Provides get/set access for other modules and command handlers.

### 2.5 UART Command Interface
- Receives and parses commands from UART.
- Uses a command table (`uart_cmd_table`) with command strings, handler pointers, and help text.
- Sorts commands by length for unambiguous matching.
- Routes commands to handler functions (e.g., `HandleSetCommand`, `HandleLbsCommand`).

### 2.6 HTTP Client
- Sends location and status data to a remote server using HTTP or HTTPS.
- Handles SSL configuration if required.

### 2.7 SMS Service
- Processes incoming SMS commands.
- Sends location or status via SMS on request.

### 2.8 Logging
- Provides tagged, timestamped logs for debugging and monitoring.
- Output can be sent to UART or stored as needed.

---

## 3. Block Interactions & Data Flow

```
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│  UART/CLI    │<---->│ config_cmds  │<---->│ config_store │
└──────────────┘      └──────────────┘      └──────────────┘
      │                        │
      ▼                        ▼
┌──────────────┐      ┌──────────────┐
│ gps_tracker  │<---->│   network    │
└──────────────┘      └──────────────┘
      │                        │
      ▼                        ▼
┌──────────────┐      ┌──────────────┐
│   http       │      │ sms_service  │
└──────────────┘      └──────────────┘
```

- **UART/CLI**: User interface for configuration and control.
- **config_cmds**: Parses commands, updates config, triggers actions.
- **config_store**: Loads/saves persistent settings.
- **gps_tracker**: Collects and processes GPS data.
- **network**: Manages GSM, cell info, and LBS.
- **http**: Sends data to server.
- **sms_service**: Handles SMS-based commands and notifications.

---

## 4. UART Command Handling (Internal Details)

### Data Structures
- **uart_cmd_entry**: Structure holding command string, length, handler pointer, syntax, and help text.
- **uart_cmd_table**: Array of all supported commands.
- **uart_cmd_sorted_idx**: Array of indices, sorted by command length (longest first) for unambiguous matching.

**Why is a sorted index used?**

The sorted index (`uart_cmd_sorted_idx`) ensures that longer (more specific) commands are matched before shorter ones. This prevents ambiguous matches when one command is a prefix of another (for example, `net` and `net activate`). By sorting the command table by command length (descending), the command parser always checks for the most specific match first, resulting in correct and predictable command handling.

### Flow Diagram

```
[UART RX]
   │
   ▼
[HandleUartCommand(cmd)]
   │
   ├─► [If not sorted: InitUartCmdTableSortedIdx()]
   │
   ├─► For each index in uart_cmd_sorted_idx:
   │      ├─► Compare input with uart_cmd_table[i].cmd
   │      ├─► If match:
   │      │      └─► Call uart_cmd_table[i].handler(param)
   │      └─► (Continue)
   │
   └─► If no match: print "Unknown command"
```

- **Command handlers** are regular C functions that receive the parameter string and perform the requested action (e.g., get/set config, trigger LBS, restart, etc).
- The command table and sorting ensure that longer (more specific) commands are matched before shorter ones (e.g., `net activate` before `net`).

---

### Example: `set` Command Flow

The `set` command allows the user to update configuration parameters via UART. Here is how it works internally:

#### Data Structures
- **uart_cmd_entry**: Each command, including `set`, is represented in the `uart_cmd_table` as an entry:
  ```c
  struct uart_cmd_entry {
      const char* cmd;           // Command string (e.g., "set")
      int cmd_len;               // Length of the command string
      UartCmdHandler handler;    // Pointer to the handler function
      const char* syntax;        // Usage syntax
      const char* help;          // Help text
  };
  ```
- **uart_cmd_table**: Array of all supported commands, including `set`.
- **ConfigStore_t**: Structure holding all configuration parameters (see `config_store.h`).

#### Flow Diagram

```
[UART RX: "set apn myapn"]
   │
   ▼
[HandleUartCommand(cmd)]
   │
   ├─► Finds "set" in uart_cmd_table (using sorted index)
   │
   └─► Calls HandleSetCommand(param)
           │
           ├─► Parses parameter name and value (e.g., "apn", "myapn")
           ├─► Validates parameter name
           ├─► Updates the corresponding field in ConfigStore_t
           ├─► Optionally validates value (format, range)
           └─► Prints confirmation or error to UART
```

#### Handler Logic (Simplified)
- Trims whitespace from the input.
- Extracts the parameter name and value.
- Looks up the parameter in the configuration structure.
- Updates the value if valid, or prints an error if not.
- Does not persist to flash until the `save` command is issued.

This approach ensures that configuration changes are robust, validated, and only applied when explicitly requested by the user.

---

