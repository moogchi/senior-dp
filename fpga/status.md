# FPGA hardware security instrument — status

**Board:** iCESugar v1.5 (iCE40UP5K)  
**Accessories:** PMOD-AUDIO, PMOD-LED, PMOD-VGA, PMOD-RGBLCD  
**Status:** Planning; no hardware or RTL milestones verified yet  
**Last updated:** 2026-09-26

## Goal

Build a small, reusable instrument for observing and testing digital interfaces on hardware I own: **logic analyzer → protocol decoder → programmable trigger → fault-injection timing controller → Python CLI**. The first complete demo captures an STM32 UART transaction, decodes it on the host, and exports it for inspection. Later, a precisely timed output can trigger an external, separately designed test fixture.

## Board constraints and design choices

- iCE40UP5K: 5,280 logic cells, 128 Kbit dual-port block RAM and 1 Mbit single-port RAM. Start with block RAM and modest captures; validate actual inferred memory and timing in synthesis reports.
- iCELink supplies a 12 MHz clock and a USB CDC serial port to the FPGA. Use CDC serial for control/readout first; do not assume the separate FPGA-facing Micro-USB port is a ready-made data link.
- A 12 MHz sampler resolves samples about 83 ns apart. It is a starting clock, not a guarantee of 12 MHz usable input bandwidth. First target slow UART and SPI; measure practical limits and consider PLL-based sampling after the baseline works.
- Inputs must remain within the board's documented electrical limits. Confirm the v1.5 schematic, pin constraints, PMOD orientation, target voltage, common ground, and any level shifting before connecting a target. Never connect 5 V signals directly on an assumption.
- Keep the capture core, protocol decoders, and output timing engine separate. The analyzer captures raw levels; the host can decode offline without resynthesizing.

## Progress board

| Phase | Deliverable | Done when | Status |
| --- | --- | --- | --- |
| 0. Bring-up | Build and load a bitstream; verify serial echo and PMOD pin mapping | Board LED blinks, host sends/receives a known message, PMOD-LED displays a counter | ☐ |
| 1. Logic analyzer | Four inputs, synchronous sampling, ring buffer, manual/edge trigger, serial dump | Capture a known GPIO square wave and UART waveform; samples and channel order match a reference trace | ☐ |
| 2. Protocol decoder | UART first; SPI next; I²C after electrical setup | Python decodes controlled test vectors and real captures with expected bytes/timing; raw traces remain exportable | ☐ |
| 3. Trigger engine | Channel edge, level pattern, delay, pre/post-trigger capture | Trigger at a programmed event; verify offsets in captured samples across repeated runs | ☐ |
| 4. Timing output | Programmable pulse delay and width from a trigger | Scope or second analyzer verifies pulse position and width; default output stays inactive until armed | ☐ |
| 5. Host tooling | Python CLI, metadata, VCD export, repeatable demo | One command configures, arms, captures, decodes, and saves a trace with sample rate and pin map | ☐ |
| 6. Integration | STM32 test fixture and documented experiment | Capture a known event, drive a timed test output, record outcome and timing without changing the analyzer core | ☐ |

## Next session: get the first waveform

- [ ] Identify the exact iCESugar v1.5 revision and compare its schematic with the shipped constraints file. Record FPGA pin → PMOD connector/pin → channel labels in `docs/pinmap.md`.
- [ ] Confirm the clock jumper and iCELink CDC device on the host. Install/check Yosys, nextpnr-ice40, IceStorm, Verilator or Icarus Verilog, and Python serial tooling.
- [ ] Build a blink/counter design with the onboard LED and PMOD-LED. Save build logs and a known-good bitstream.
- [ ] Loop a slow FPGA-generated square wave into one analyzer input through an appropriate connection. Sample at 12 MHz into a small BRAM buffer and dump hexadecimal samples over CDC serial.
- [ ] Plot/export the captured samples on the host. Check period, polarity, channel order, and whether the reported sample count is exact.

## Implementation notes

**Capture v0:** Four sampled input bits packed into one byte per sample. A 4,096-sample buffer occupies 4 KiB if packed as bytes, or 2 KiB if packed as nibbles. At 12 MHz it spans roughly 341 µs. Preserve a sample index or trigger offset and capture metadata. Use dual-port BRAM where one port writes samples and the other reads them after capture; stop or safely arbitrate writes during readout.

**Clock-domain handling:** The target's inputs are asynchronous to the FPGA clock. Define how they are sampled, account for metastability, and test any synchronizer's timing tradeoff. A UART decoder should tolerate a small edge timing uncertainty; never present the tool as a high-speed oscilloscope.

**Serial protocol v0:** `PING`, `CONFIG rate channels depth`, `ARM`, `STATUS`, `READ`. Frame binary data with a magic/version, sample count, sample rate, trigger index, and checksum. Start with simple commands if needed, but avoid an unframed stream that the CLI cannot recover after a dropped byte.

**Trigger v1:** edge on one selected channel; then mask/value pattern; then programmable delay and post-trigger length. Test ring-buffer wraparound and the cases where a trigger never arrives or arrives immediately.

**Timing output:** Begin with an FPGA GPIO into a scope or LED-compatible test fixture. A GPIO alone is not a voltage glitcher: power/clock fault injection requires external switching hardware, voltage and current limits, and measurement of the real pulse at the target. Keep the output disarmed on reset and require an explicit arm command.

## Where the PMODs fit

| Module | Useful role | Priority |
| --- | --- | --- |
| PMOD-LED | Capture state, trigger indication, channel activity, pulse-armed indicator | Early |
| PMOD-VGA | Optional local waveform display after the serial/VCD path works; consumes pins and timing budget | Later |
| PMOD-RGBLCD | Optional compact status/menu UI; inspect its interface and pin use first | Later |
| PMOD-AUDIO | Optional audible trigger feedback or test waveform; not needed for initial digital capture | Later |

Use plain PMOD GPIO pins for analyzer inputs and the timing output. Reserve accessory pins only after drawing a pin budget; display hardware should not delay the core instrument.

## Suggested repository layout

```text
rtl/            sampler, buffer, trigger, UART command/readout, pulse timer
sim/            RTL testbenches and stimulus
host/           Python CLI, parsers, UART/SPI/I2C decoders, VCD export
constraints/    verified v1.5 pin assignments
examples/       STM32 known-pattern firmware and captured reference traces
docs/           pin map, wiring, timing, limitations, experiment notes
status.md       this checklist and measured progress
```

## Measurements to record after each milestone

| Metric | Record |
| --- | --- |
| Build | tool versions, board revision, commit, clock source, configured sample rate |
| Resources | LUTs, flip-flops, BRAM/SPRAm used, routed maximum frequency |
| Capture | channels, depth, pre/post-trigger count, effective time span, dropped/overrun flags |
| Accuracy | expected versus observed edge index, UART bytes, trigger delay and pulse width |
| Wiring | voltage, ground, PMOD connector/pins, target device and test signal |

## References

- [iCESugar board README and hardware summary](https://github.com/wuxx/icesugar/blob/master/README_en.md)
- [iCESugar v1.5 board schematic](https://github.com/wuxx/icesugar/blob/master/schematic/iCESugar-v1.5.pdf)
- [iCESugar repository (constraints and examples)](https://github.com/wuxx/icesugar)

Update the status table only after a hardware or simulation check, and paste the relevant command, trace, or measurement into `docs/` so the next session starts from a known state.
