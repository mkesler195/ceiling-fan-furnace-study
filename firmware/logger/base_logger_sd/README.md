# Base Logger (SD Card + RTC)

## Role
This sketch runs on the **upstairs base / logger unit**.  
It collects environmental data and event messages from remote nodes and logs them to an SD card with timestamps.

## Physical Location
- Upstairs (tabletop during testing)
- Acts as the central aggregation point for the system

## Hardware
- Arduino (UNO or Nano)
- BMP280 (temperature + air pressure)
- RTC (DS3231)
- SD card module
- nRF24L01 radio

## Behavior
- Initializes RTC, SD card, and radio
- Periodically logs timestamped environmental data
- Receives radio packets from listener nodes
- Writes structured records to SD card for later analysis

## Data Output
- CSV-style log files on SD card
- Records include date/time, temperature, pressure, and event indicators

## Notes / Gotchas
- RTC must be set at least once before meaningful logging
- SD card initialization is critical; logging halts if it fails
- Log format and intervals are defined in the sketch
- Assumes listener nodes handle their own detection thresholds
