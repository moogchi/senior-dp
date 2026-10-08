# Hardware-Rooted Supply Chain Attestation — Project Status

**Last updated:** 2026-10-08

**Claim:** A verifier can determine whether the firmware executing on a specific
device is the artifact produced by a reproducible build of a source commit that
satisfied repository policy.

**Platform:** Nucleo-F411RE (STM32F411RETx) — Cortex-M4, 512 KB flash, 128 KB
SRAM, no TrustZone, no TRNG, no crypto accelerator, 16 lockable OTP blocks of
16 bytes.

**Scope choice:** Non-TrustZone MCUs. Targets the deployed brownfield of
pre-Armv8-M parts with no attestation story, rather than parts where ARM already
ships PSA Initial Attestation.

**Core design principle:** The bootloader is a drop-in module. The firmware is
never modified, never linked against anything, and never asked to cooperate.
Prepend the bootloader to any image and it becomes attestable.

---

## Reference configuration

Pin these — Phase 5 reproducibility depends on them.

| Item            | Value                                                                                |
| --------------- | ------------------------------------------------------------------------------------ |
| Clock source    | HSE 8 MHz, BYPASS mode (ST-LINK MCO via PH0)                                         |
| PLL             | M=4, N=100, P=2                                                                      |
| Bus clocks      | HCLK 100 MHz, APB1 50 MHz, APB2 100 MHz                                              |
| Flash latency   | 3 wait states                                                                        |
| CSS             | Enabled                                                                              |
| Optimization    | `-Os -ffunction-sections -fdata-sections`, `--gc-sections`                           |
| Bootloader slot | `0x08000000`, 32 KB (sectors 0–1)                                                    |
| App slot        | `0x08008000`, 480 KB                                                                 |
| Measured range  | Full app slot, `0x08008000`, 491,520 bytes                                           |
| OTP base        | `0x1FFF7800` — verify against RM0383                                                 |
| Dev machines    | macOS Intel (Homebrew gcc-arm-embedded 14.2) + Arch Linux (pacman arm-none-eabi-gcc) |

**Known gotcha:** CubeMX overwrites the generated linker script on every
regenerate. FLASH origin/length edits must be re-applied. Consider copying the
script out of the generated tree and pointing CMake at the copy.

**Known gotcha:** `set(CMAKE_C_FLAGS_DEBUG ...)` must come _after_ `project()`
or CMake resets it. Build-type flags otherwise append `-O0 -g3`.

**Toolchain drift:** The two dev machines run different `arm-none-eabi-gcc`
builds. Record both; expect this to matter in Phase 5.

---

## Done

### Toolchain and build

- [x] `arm-none-eabi-gcc` on both machines; CMake + Ninja
- [x] LL drivers only — no HAL anywhere
- [x] Bootloader peripheral footprint: GPIOA + USART2 only
- [x] Two independent CMake projects (`bootloader/`, `firmware/`)
- [x] `.bin` generation via post-build objcopy
- [x] Dual-build option: `-DUSE_BLAKE2S=ON` selects measurement primitive
- [x] Signed commits (SSH signing) on both machines

### Boot chain

- [x] Serial console over ST-LINK VCP (USART2, 115200, polling, no DMA)
- [x] Two-stage boot: bootloader at sectors 0–1 jumps to app at `0x08008000`
- [x] Jump: read SP and reset vector, set `VTOR`, set MSP, branch
- [x] Bootloader measures the full app slot and prints the digest
- [x] Measurement verified against host-side `st-flash read` + `shasum -a 256`
- [x] **Decided against MCUboot.** Hand-rolled minimal bootloader for full
      accountability of every instruction in the measured boot path.

### Crypto primitives — all verified against published test vectors

- [x] SHA-256 — vendored, verified against host `shasum`
- [x] HMAC-SHA256 — written from RFC 2104, verified against RFC 4231 TC1
- [x] HKDF-Extract/Expand — written from RFC 5869, verified against RFC 5869 TC1
- [x] Ed25519 keypair — Monocypher `monocypher-ed25519.c`, verified against RFC 8032
- [x] BLAKE2s — vendored `blake2s-ref.c` (needs KAT verification, see below)

**Decision:** Constructions (HMAC, HKDF) written in-house; primitives (SHA-256,
Ed25519, BLAKE2s) vendored. Compositions have RFC test vectors and no
secret-dependent branching; primitives need constant-time implementations that
should not be hand-rolled.

### DICE derivation — working end to end

- [x] OTP read path confirmed (all 16 blocks read `0xFF`, addressing correct)
- [x] `measure()` / `derive()` abstraction — two implementations behind one interface
- [x] Full chain: OTP → `measure` → `derive` → CDI → `crypto_ed25519_key_pair`
- [x] UDS wiped after derive; CDI wiped by Monocypher as a side effect
- [x] `clear_key` uses volatile-pointer wipe
- [x] **Wipes verified present in disassembly at `-Os`**
- [x] DMA1/DMA2 clock gates cleared before OTP read; verified via `RCC->AHB1ENR`
- [x] Avalanche confirmed: different firmware → different measurement → different keypair

**Note:** Monocypher's `secret_key` format is `CDI || public_key` (64 bytes).
The CDI survives inside the secret key. The UDS is what's destroyed.

### Measurement primitive comparison — measured at 100 MHz, same code path

| Primitive       | Cycles     | Time   | Bootloader flash | Ecosystem                 |
| --------------- | ---------- | ------ | ---------------- | ------------------------- |
| SHA-256 + HKDF  | 56,056,861 | 561 ms | 15,792 B         | Native to in-toto/SLSA    |
| BLAKE2s (keyed) | 27,258,675 | 273 ms | 20,928 B         | Needs dual-digest subject |

Optimization level barely moves this: `-Os` 565 ms, `-O2` 568 ms, `-O3` 505 ms.
BLAKE2s's size cost is one function, `blake2s_compress` at 5,652 bytes — the
reference implementation fully unrolls all 10 rounds. Ed25519 (`fe_mul`,
`sha512_compress`, etc.) is a ~5 KB fixed cost in both builds.

**Open:** 273–561 ms boot delay vs. CAN/peripherals expected up by ~100 ms.
Email sent to Prof. Campisi asking about realistic startup-time requirements.
Fallback is measuring only the used image region via a length header, which
reopens the leftover-code gap.

### Design decisions settled

- [x] **MPU: not used as a guarantee.** Firmware is arbitrary and may need
      privileged mode, so privilege can't be dropped. MPU would be advisory
      only. Key destruction is the guarantee. One sentence in the writeup.
- [x] **Attestation happens in the bootloader, before the jump.** Not in the
      app. Keeps the firmware untouched and means no secret key survives into
      application execution. Flash triggers reset, so attestation naturally
      follows every flash.
- [x] **Asymmetric (Ed25519), not symmetric.** A MAC would require the verifier
      to share the device secret — removes third-party verifiability and
      prevents publishing the device identity in supply chain metadata.
- [x] **No `SHARED` linker region.** Was added for app-side key handoff; now
      unnecessary. Revert it in both linker scripts.
- [x] **The system proves provenance, not benignity.** Malicious code that
      passes review produces a valid attestation. Out of scope by design.

---

## In progress — nonce protocol (bootloader side)

Design: bootloader derives keys, listens on USART2 for a challenge, signs
`measurement || nonce`, sends response, wipes secret key, jumps.

- [ ] Revert `SHARED` region from both linker scripts
- [ ] Remove `secret_key` UART print and wipe-test block from `main.c`
      (or wrap in `#ifdef DERIVE_DEBUG`)
- [ ] Listen window on USART2 — gate on a byte already present or a GPIO
      strap so an unattended boot doesn't wait
- [ ] Receive nonce (fixed length, e.g. 32 bytes)
- [ ] Sign `measurement || nonce` with `crypto_ed25519_sign`
- [ ] Response: signature (64) + public_key (32) + measurement (32)
- [ ] Wipe `secret_key` before jump
- [ ] Host-side Python: send nonce, verify with `cryptography` library
- [ ] **Nonce must be inside the signed message** — deterministic derivation
      makes replay trivial otherwise

---

## Phase 2 remaining

- [ ] Verify BLAKE2s against `blake2s-kat.h` known-answer tests
- [ ] Zero derivation SRAM on entry as well as before jump (warm-reset remnants)
- [ ] Verify-after-erase: read back wiped regions, confirm zero
- [ ] Decide UDS size: 16 bytes (one OTP block) or 32 (two blocks). 128 bits
      is sufficient; one block is simpler to provision.
- [ ] `#ifdef USE_OTP_UDS` / placeholder switch (currently reads OTP directly,
      which is all `0xFF` — effectively a known placeholder)
- [ ] **Burn real UDS** (`openssl rand`) into OTP — last step. Any OTP write
      is permanent, lock or no lock. Develop with placeholder, burn once.
- [ ] WRP on sectors 0–1; verify a write is rejected
- [ ] **Never set RDP Level 2.** Irreversible, kills SWD.

---

## Phase 3 — Build pipeline

- [ ] **Measurement descriptor tool** — host-side, reads `firmware.bin`, pads
      to slot size with `0xFF`, hashes exactly as the bootloader does, emits
      in-toto subject JSON. Validate: output must match device's printed
      digest. **This is the original engineering contribution.**
- [ ] Emit both digests as subjects on one in-toto Statement:
      `firmware.bin` (build output) and `boot-measurement` (bootloader-style).
      If BLAKE2s is adopted, add it as a second algorithm in the digest set.
- [ ] Containerize the build, pin by digest
- [ ] gittuf verification as a pre-build CI gate
- [ ] Wrap the gittuf verification result as a signed in-toto predicate
- [ ] Emit SLSA provenance via Witness
- [ ] Include device public key in provenance (enables device-binding check)

---

## Phase 4 — Verifier (Rust, host side)

- [ ] Send nonce over serial, receive token
- [ ] Verify signature and nonce freshness
- [ ] Extract the measurement
- [ ] Look up the in-toto Statement whose subject matches
- [ ] Verify statement signature against trusted builder identity
- [ ] Read source repo and commit from SLSA predicate
- [ ] Verify the gittuf attestation for that commit
- [ ] Print full chain and verdict

**Exit criterion:** one command takes a live device and prints commit,
reviewers, pass/fail.

---

## Phase 5 — Reproducible builds

Timebox it. Partial results are a finding.

- [ ] Build identical firmware on both machines, diff the binaries
- [ ] Document every source of nondeterminism found
- [ ] `-ffile-prefix-map`, `SOURCE_DATE_EPOCH`, strip `__DATE__`/`__TIME__`
- [ ] Deterministic archives, sorted link order, pinned linker script
- [ ] Pin toolchain and CubeMX versions
- [ ] Measure reproducibility rate across N builds

---

## Phase 6 — DP1 evaluation and writeup

### Attack table — demonstrate row 2 live

| Attack                                         | Plain secure boot | This system                  |
| ---------------------------------------------- | ----------------- | ---------------------------- |
| Unsigned image                                 | caught            | caught                       |
| **Signed image from unreviewed commit**        | **missed**        | **caught**                   |
| Signed, source matches, build machine tampered | missed            | caught (reproducibility)     |
| Signed, signing key stolen                     | missed            | caught (no provenance chain) |

### Measurements

- [x] Boot measurement cost: 561 ms SHA-256, 273 ms BLAKE2s
- [x] Flash cost per primitive (table above)
- [ ] RAM delta, token size, verification latency
- [ ] Reproducibility rate

### Limitations — state before anyone asks

- Boot-time attestation reports what was measured at boot, not what's running now
- **No forward secrecy** — deterministic derivation yields the same CDI every
  boot. Verifier nonce is mandatory.
- **Rollback not addressed** — monotonic counter out of scope for DP1
- **DMA is not subject to the MPU** — key destruction is the mitigation
- **Glitching the erase step is the primary unaddressed attack**
- **A privileged application can read OTP directly.** No software mechanism on
  this chip prevents it. This is the fundamental limit of non-TrustZone scope.
- UDS provisioning is trusted, not proven
- Reproducibility only as strong as toolchain pinning
- Key revocation is prospective
- **Provenance, not benignity.** Reviewed malicious code attests validly.
- Boot delay (273–561 ms) may exceed real-time startup requirements

### Properties worth claiming

- UDS in **locked OTP** — extractable only by physical attack on the die
- **Internal flash, no probeable memory bus**
- **Bootloader is a drop-in module** — firmware never modified
- **No secret survives into application execution**
- Porting to a part with factory-fused UDS is a substitution, not a redesign

### Design alternatives considered and rejected (one sentence each)

- MCUboot — too heavy, update machinery not needed
- TrustZone parts — scope choice; brownfield is the gap
- MPU-based isolation — advisory only against privileged app
- App-side attestation — requires firmware cooperation
- Symmetric MAC — verifier shares secret, no third-party verifiability
- Trap-and-emulate hypervisor — massive trusted base, out of scope
- FPGA external observer — internal flash, nothing to tap

---

## DP2 — Attacking the design (Spring)

Attacks must target the **architecture**, not the prototype's shortcuts.

- [ ] Privilege escalation from app; confirm UDS unrecoverable regardless
- [ ] DMA read of UDS region during pre-erase window
- [ ] **Voltage/clock glitch the key-destruction step** — FPGA instrument
      (iCESugar) is the intended tool; needs PLL-clocked timing output for
      sub-83 ns placement
- [ ] Alias key certified for firmware that isn't what executed
- [ ] Replay a token across a reflash
- [ ] Gap between measured region and executed region
- [ ] Build input that changes behaviour without changing measurement
- [ ] Bypass gittuf gate — revoked key, RSL manipulation
- [ ] UDS extraction from locked OTP
- [ ] Write up what held, what didn't, what a stronger anchor would prevent

---

## Open decisions

- [ ] SHA-256 vs BLAKE2s — have the data; need Campisi's input on startup time
      and Cappos's input on whether dual-digest is acceptable
- [ ] Does the verifier fetch the gittuf attestation live, or bundled?
- [ ] DeviceID key (UDS-only, firmware-independent) — needed for device-binding
      encryption idea; otherwise skip
- [ ] Project name — `senior-dp` is the placeholder
- [ ] L552 comparison run in spring?

### Extensions designed but not implemented

- **Flash-step attestation** — record who flashed which device in an in-toto
  layout; second OTP block for provisioning metadata
- **Device-binding encryption** — encrypt firmware to device's X25519 DeviceID
  key; device that can't decrypt isn't the target. Requires DeviceID key.
  Low priority given physical flash access.

---

## Pending communications

- [ ] Meeting with Cappos + Patrick — supposed to happen at end of a gittuf
      meeting; never scheduled. **Chase this.**
- [ ] Prof. Campisi — email sent re: startup-time requirements
- [ ] Questions for Patrick: does gittuf emit machine-readable verification
      output wrappable as an in-toto predicate? How much GitHub metadata
      ingestion works today?
- [ ] Questions for Cappos: working demo or publishable? Non-TrustZone scope
      right? Dual-digest acceptable?

---

## Risks

| Risk                                      | Mitigation                                                      |
| ----------------------------------------- | --------------------------------------------------------------- |
| Reproducibility becomes endless           | Timebox; partial results are a finding                          |
| gittuf metadata ingestion not ready       | Scope DP1 to what exists                                        |
| OTP burned before derivation correct      | Develop with placeholder, burn last                             |
| RDP2 set accidentally                     | Never set it                                                    |
| Toolchain drift between machines          | Pin and record both; containerize                               |
| Boot delay unacceptable for target domain | Length-header fallback; or scope claim to non-real-time devices |
