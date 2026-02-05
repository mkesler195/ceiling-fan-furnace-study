# Basement Furnace Listener (Mic + Radio)

## Role
This sketch runs on the **basement listener node** near the furnace.  
It detects furnace activity via microphone input and reports events upstream via radio.

## Physical Location
- Basement, near furnace

## Hardware
- Arduino (UNO / Nano — note actual board used)
- Microphone module (analog input)
- nRF24L01 radio

## Behavior
- Continuously samples microphone input
- Detects sustained sound above threshold
- Sends ON/OFF or heartbeat events to base logger

## Notes / Gotchas
- Detection thresholds are environment-dependent
- Mic sensitivity and placement matter a lot
- False positives can occur during other basement noise
