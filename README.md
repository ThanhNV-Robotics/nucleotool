# nucleotool

Configure and diagnose MAB MD drives through the NUCLEO-H755ZI-Q leg bridge
(project `STM32H755_WS_FDCan_UDP`) instead of a CANdle.

A fork of MAB Robotics' [CANdle-SDK](https://github.com/mabrobotics/CANdle-SDK) v1.5.1 with a UDP
bus added. The board emulates one CANdle per FDCAN bus:

| Leg | Bus | MD IDs |
|---|---|---|
| L (left) | FDCAN1 | 10, 20, 30, 40, 50, 60 |
| R (right) | FDCAN2 | 70, 80, 90, 100, 110, 120 |

The binary is named `nucleotool`, so it does not clash with an installed `candletool` or `mdtool`.

## Requirements

- Wired Ethernet to the board: board `192.168.10.10`, PC `192.168.10.1/24`, UDP port 9998.
- Board in MAINTENANCE mode (the default after boot); in CONTROL mode tool requests are refused.
- Legs run 1 Mbps / 8 Mbps CAN FD, so `-d 8M` (the default). Other datarates are refused.

## Build

```bash
git submodule update --init candletool/3rd_party/CLI11   # + commons/googletest for -DMAKE_TESTS=ON
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ln -s "$PWD/build/candletool/nucleotool" ~/.local/bin/nucleotool   # once: run it from any folder
```

The commands below are written as `./nucleotool` (run inside `build/candletool`); with the link
above, plain `nucleotool` works anywhere.

## Common commands

Without `--device`, `md discover` scans both legs and `md -i <id> ...` finds the leg itself.
Add `--device L` or `--device R` to use one leg only (faster, skips the lookup).

| Task | Command |
|---|---|
| Find all drives (both legs, ~5 s) | `./nucleotool md discover` |
| Find drives on one leg | `./nucleotool --device R md discover` |
| Full info incl. controller gains | `./nucleotool md -i 10 info` |
| Firmware version | `./nucleotool md -i 10 version` |
| Live position / velocity / torque (Ctrl+C stops) | `./nucleotool md -i 10 encoder` |
| Blink LED (find the drive) | `./nucleotool md -i 10 blink` |
| Read a register (name or 0x address) | `./nucleotool md -i 10 register read motorImpPidKp` |
| Write a register (not saved) | `./nucleotool md -i 10 register write motorImpPidKp 100` |
| Save configuration to flash | `./nucleotool md -i 10 save` |
| Change CAN ID / timeout | `./nucleotool md -i 10 can --new_id 11 --new_timeout 200 --save` |
| Download config to a file | `./nucleotool md -i 10 config download ./md10.cfg` |
| Upload config from a file | `./nucleotool md -i 10 config upload ./md10.cfg` |
| Calibrate (motor moves!) | `./nucleotool md -i 10 calibration` |
| Set current position as zero (home) | `./nucleotool md -i 10 zero`, then `md -i 10 save` to keep it after power-off |
| Clear errors / warnings | `./nucleotool md -i 10 clear` |
| Reboot a drive | `./nucleotool md -i 10 reset` |
| Board / emulated CANdle version | `./nucleotool candle version` |
| More output for debugging | add `-v2` before `md`, e.g. `./nucleotool -v2 md -i 10 info` |

Cautions:
- Keep each leg's IDs as in the table above and the datarate at 8M (`md can --new_datarate`),
  otherwise the board's control loop will not find the drive.
- `calibration` and `test` move the motor: secure the leg first.
- Firmware update (`md update`) is not supported through the board yet.

## Changes against upstream CANdle-SDK

- `candlelib/src/communication_interface/UDP.hpp/.cpp`: new bus, one datagram per CANdle
  transfer, header `[0xCD][bus|status][seq u16 LE]`.
- `busTypes_t::UDP` in `attachCandle` / `CandleBuilder`; `--bus UDP` (default),
  `--device L|R|<ip>:L|<ip>:R`, `-d` default 8M.
- `md discover` scans both legs in parallel; `md -i <id>` finds the leg itself.
- `MD::discoverMDs` waits 2 ms per probed ID instead of 10 ms.
- Our drives run MD firmware 2.4.0–2.5.4, which lacks 12 registers this SDK reads
  (0x02A, 0x02B, 0x200, 0x201, 0x701, 0x7FB–0x7FF, 0x813, 0x814). Reading one makes the drive
  stop answering for seconds, so `MD` refuses them on firmware 2.x before sending.
- Register writes use the legacy frame 0x40 on MD firmware 2.x (2.x ignores 0x42, so every
  write — blink, zero, save, register write, test — failed silently).
- `md info` also prints controller gains (position/velocity PID, impedance kp/kd).
- New `md encoder` (like mdtool `encoder`): prints position, velocity, torque every 100 ms; reads registers only.
- Null checks in `md info` and `MDCli::getMd`.
- Removed what this project does not use: candlegui, pycandle (and pybind11), examples, docs,
  docker images, launch scripts, cross toolchains, MAB's CI, the Windows CANdle USB driver.

## License

Unchanged from upstream: MIT for the SDK (`LICENSE.txt`), LGPL-3 for candlelib (`candlelib/LICENSE`).
