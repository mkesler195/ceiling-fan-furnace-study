# Ceiling Fan & Furnace Study


## Why this project exists

This project explores the real-world interaction between ceiling fan usage,
furnace runtime, and indoor temperature dynamics in a lived-in home environment.

Rather than relying solely on theoretical models, the goal is to collect
and analyze actual sensor data to better understand:
- how often and how long the furnace runs under different conditions
- whether ceiling fan operation meaningfully affects runtime or comfort
- how small environmental changes show up in logged data over time

The project emphasizes hands-on measurement, careful documentation,
and iterative analysis to surface practical insights rather than
perfect or universal conclusions.

## Overview

This repository documents an ongoing, non‑invasive study of **winter comfort, air circulation, and furnace runtime** in a residential open‑plan living space with a cathedral ceiling. The project explores how **ceiling fan mode (OFF, Counter Clockwise "CCW", or Clockwise "CW")** affects perceived comfort and air distribution *without materially changing measured room temperature*, and how those effects relate to **actual furnace operation**.

A key design principle is **separation of concerns**:

* A **basement listener** detects furnace activity acoustically (non‑invasive, no thermostat wiring).
* An **upstairs logger** records temperature, pressure, fan mode, and timestamps to understand how heat is experienced in the living space.

The goal is not to optimize HVAC controls directly, but to **observe, measure, and reason about comfort, efficiency, and air mixing** using inexpensive sensors and careful experimental design.

---

## System Architecture (High Level)

* **Basement Furnace Listener**

  * Microphone‑based acoustic detection
  * Identifies furnace ON/OFF states using peak‑to‑peak sound energy
  * Applies hysteresis to avoid false transitions
  * Sends state + acoustic data wirelessly

* **Upstairs Data Logger**

  * Receives furnace state and acoustic energy (p2p)
  * Logs indoor temperature, pressure, fan mode, and timestamps
  * Stores snapshot‑based samples to SD card

Importantly, the listener has **no awareness of ceiling fan operation**. All fan‑related effects are observed *only* in the living space data.

---

## Questions This Project Explores

* How stable is measured indoor temperature across different fan modes?
* Why can comfort change significantly even when temperature varies by only ~±1°F?
* How do FAN OFF, FAN CCW (downward airflow), and FAN CW (upward airflow) differ in perceived warmth?
* Can sample‑based logging yield useful insights without full event‑driven HVAC integration?
* What are the tradeoffs of acoustic detection and hysteresis in real homes?

---

## Repository Structure

```
ceiling-fan-furnace-study/
│
├── README.md                # This file
│
├── firmware/                # Arduino firmware
│   ├── listener/            # Basement furnace listener (mic + radio)
│   └── logger/              # Upstairs logger (temp, SD, RTC, fan mode)
│
├── hardware/                # Physical build artifacts
│   ├── fritzing/            # Fritzing diagrams and images
│   └── wiring-notes.md      # Grounding, caps, placement notes
│
├── docs/                    # Explanatory documentation
│   ├── room-layouts/        # Floor plans and sensor placement
│   ├── experimental-design/ # Assumptions, methodology, fan modes
│   ├── findings/            # Observations, interpretations, conclusions
│   └── figures/             # Charts and annotated plots
│
├── data/                    # Logged data
│   ├── raw/                 # Unmodified CSVs grouped by fan mode
│   ├── processed/           # Combined / filtered datasets
│   └── README.md            # Data naming and caveats
│
└── analysis/                # Scripts and notebooks (optional)
    ├── scripts/
    ├── notebooks/
    └── README.md
```

---

## Experimental Philosophy

* **Non‑invasive by design** — no thermostat wiring or HVAC control signals
* **Snapshot‑based sampling** rather than precise event timing
* **Relative changes matter more than absolute values**
* Comfort is treated as a **human experience**, not just a temperature number

This repository intentionally preserves *thinking*, not just results.

---

## Where to Start

1. `docs/experimental-design/` — understand the assumptions and methodology
2. `docs/room-layouts/` — see the physical context of the measurements
3. `docs/findings/` — read the emerging conclusions
4. `firmware/` — review how data is captured and transmitted

---

## Status

This is an **active, exploratory project**. Findings will evolve as additional data is collected across different times of day, outdoor temperatures, and fan configurations.

Contributions, questions, and thoughtful critique are welcome.
