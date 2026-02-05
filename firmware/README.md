# Firmware Overview: Logger ↔ Listener Message Flow

This project currently has two active firmware roles:

- **Logger (upstairs)**: aggregates data and writes timestamped logs to SD card.
- **Listener (basement)**: detects furnace activity (microphone thresholds) and reports events upstream by radio.

## Where the code lives

- Logger sketch:
  - `firmware/logger/base_logger_sd/base_logger_sd.ino`

- Listener sketch:
  - `firmware/listener/basement_listener_mic/basement_listener_mic.ino`

## System topology

Basement Listener (Node)  --->  Radio (nRF24L01)  --->  Upstairs Logger (Base)
         |                                                     |
   Mic detects sound                                     RTC timestamps
   decides ON/OFF                                        SD logs records
   sends events + heartbeat                              logs Wx + events

## What gets sent over radio (conceptually)

The listener sends small messages that fall into these categories:

1. **Heartbeat** (periodic “I’m alive”)
   - Helps confirm the basement node is powered and in range

2. **Furnace ON / OFF events**
   - Listener detects sustained “loud” audio above a threshold (ON)
   - Listener detects sustained “quiet” audio below a threshold (OFF)
   - Thresholds and persistence logic live in the listener sketch

3. **(Optional / future) additional telemetry**
   - e.g., raw mic level summaries, battery voltage, local temp, etc.

> Note: The exact struct/fields are defined in the sketches. This README describes the flow and intent.

## Logger responsibilities (upstairs)

The logger is responsible for:

- Maintaining **time** (RTC) and using it for all logged records
- Writing to the **SD card** (CSV-style log files)
- Receiving radio messages and recording them as events
- (Often) also logging local environmental data (BMP280)

### Typical SD record types

- Periodic environmental log:
  - timestamp + temp + pressure (+ anything else you add later)

- Event log:
  - timestamp + event type (ON/OFF/HEARTBEAT) (+ node id / seq if used)

## Listener responsibilities (basement)

The listener is responsible for:

- Sampling microphone input
- Implementing detection logic (thresholds + persistence/hysteresis)
- Sending:
  - heartbeat messages
  - furnace ON/OFF transitions
- Remaining simple and reliable (avoid SD/logging complexity in basement)

### Detection notes

- Mic thresholds are **environment-dependent**
- Placement matters (distance to furnace, vibration coupling, etc.)
- The listener should prioritize:
  - stable classification (few false ON/OFF flips)
  - clear ON/OFF transitions over “perfect amplitude measurement”

## Packet / Protocol notes (recommended conventions)

If/when you standardize packet fields, these are useful:

- `node_id` (who sent it)
- `seq` (sequence counter; helps detect drops/repeats)
- `msg_type` (HEARTBEAT / FURNACE_ON / FURNACE_OFF)
- `payload` (optional: mic summary, battery, etc.)

If you later move shared structs into one place:
- create `firmware/common/protocol.h`
- include it from both sketches

## Quick troubleshooting checklist

- No events on SD:
  - confirm SD init succeeds in logger
  - confirm RTC time is set
  - confirm radio wiring + power (nRF24 needs solid 3.3V + capacitor)

- Listener seems dead:
  - verify heartbeat is enabled and being sent
  - check mic module power and analog pin wiring
  - confirm thresholds aren’t set so high it never triggers

- Radio flaky:
  - add / confirm local decoupling capacitor at nRF24
  - reduce data rate / adjust channel (if needed)
  - confirm CE/CSN pins match the sketch
