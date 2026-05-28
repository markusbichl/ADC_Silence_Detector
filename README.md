# ADC Silence Detector

A compact embedded device that monitors analog audio input and automatically cuts power to a downstream USB ADC when silence is detected - eliminating idle noise and unnecessary power draw.

---

## Overview

The ADC Silence Detector sits inline between your audio source and your USB ADC. It continuously monitors the analog audio signal on both left and right channels. As long as audio is present, the USB-A output port stays powered. Once silence is sustained, the device cuts USB power to the connected ADC, effectively turning it off without any manual intervention.

When audio resumes, power is restored and the downstream ADC comes back on automatically.

---

## Hardware

### Connectors

| Port | Type | Function |
|------|------|----------|
| Power In | Mini-USB | 5 V power supply input |
| Audio In/Out L | RCA (double) | Left channel analog passthrough |
| Audio In/Out R | RCA (double) | Right channel analog passthrough |
| Power Out | USB-A | Switched 5 V output for the controlled ADC |

### Signal Path

```
Audio Source ──► RCA In (L+R) ──► passthrough ──► RCA Out (L+R) ──► Downstream equipment
                      │
                      └──► ADC Level sensing (PA6 / PB0)
                                      │
                                      ▼
                              CH32X033 MCU
                                      │
                                      ▼
Mini-USB In ──────────────────► USB-A Out (switched via PB1)
```

The audio signal path is fully passive - the RCA inputs are wired straight through to the RCA outputs with no active components in the signal chain, preserving audio quality.

---

## Firmware

### Microcontroller

**WCH CH32X033** - 32-bit RISC-V (rv32imacxw) running at 48 MHz from the internal HSI oscillator.

- IDE: MounRiver Studio 2

### Detection Logic

The firmware polls both ADC channels every 500 ms and takes the higher of the two readings as the current audio level. A hysteresis comparator prevents rapid toggling around the threshold:

| Condition | 12-bit ADC value | Action |
|-----------|-----------------|--------|
| Output OFF, level rises above | 20 | Turn USB-A output **ON** |
| Output ON, level falls below | 10 | Turn USB-A output **OFF** |

**ADC channels:**

| Pin | ADC Channel | Signal |
|-----|-------------|--------|
| PA6 | CH6 | Left channel level |
| PB0 | CH8 | Right channel level |

**Output pin:** PB1 drives the USB-A power switch.

### Source Layout

```
SW
  User/
    main.c                      - application logic (ADC poll loop, hysteresis, GPIO control)
    system_ch32x035.c           - clock initialisation
    ch32x035_it.c               - interrupt handlers (unused stubs)
  Core/
    core_riscv.c/h              - RISC-V core support
  Debug/
    debug.c/h                   - SDI printf (debug UART over single wire)
  Peripheral/
    src/  inc/                  - WCH standard peripheral library
  Startup/
    startup_ch32x035.S          - reset vector and startup code
  Ld/
    Link.ld                     - linker script
PCB/
  ADC_Silence_detecor.kicad_pcb - KiCAD PCB
  ADC_Silence_detecor.kicad_sch - KiCAD Schematic
  jlcpcb/                       - Gerber files
CAD/
  case.stl                      - STL file of a simple Case
  ADC_Silence_detector.step     - STEP file of the populated PCB
  ADC_Silence_detector.stl      - STL file of the populated PCB
Demo
  ADC_Silence_detector.jpg      - Photo of the finished build
  PCB_populated.png             - Rendering of the populated PCB
```

---

## Building & Flashing

1. Open the project in **MounRiver Studio 2**.
2. Select the `obj` build configuration and click **Build** (or press `Ctrl+B`).
3. Connect a WCH-Link programmer to the SWD/SDI header.
4. Click **Download** to flash the `.hex` to the CH32X033.

Debug output is available over SDI (single-wire debug interface) at 115200 baud and prints the raw ADC level on every 500 ms poll cycle.

---

## License

Peripheral library files are copyright Nanjing Qinheng Microelectronics Co., Ltd. Application code is released under the same terms - see individual file headers for details.
