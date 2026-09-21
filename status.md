# Hardware-Rooted Supply Chain Attestation — Project Status

**Claim:** A verifier can determine whether the firmware executing on a specific
device is the artifact produced by a reproducible build of a source commit that
satisfied repository policy.

**Platform:** Nucleo-F411RE (STM32F411RETx) — Cortex-M4, 512 KB flash, 128 KB
SRAM, no TrustZone, no TRNG, no crypto accelerator, 16 lockable OTP blocks.

**Scope choice:** Non-TrustZone MCUs. Targets the deployed brownfield of
pre-Armv8-M parts with no attestation story, rather than parts where ARM already
ships PSA Initial Attestation.

---

## Reference configuration

Pin these — Phase 5 reproducibility depends on them.

| Item            | Value                                                       |
| --------------- | ----------------------------------------------------------- |
| Clock source    | HSE 8 MHz, BYPASS mode (ST-LINK MCO via PH0)                |
| PLL             | M=4, N=100, P=2                                             |
| Bus clocks      | HCLK 100 MHz, APB1 50 MHz, APB2 100 MHz                     |
| Flash latency   | 3 wait states                                               |
| CSS             | Enabled                                                     |
| Optimization    | `-Os -ffunction-sections -fdata-sections`, `--gc-sections`  |
| Bootloader slot | `0x08000000`, 16 KB (sector 0)                              |
| App slot        | `0x08004000`, 496 KB                                        |
| Measured range  | `0x08004000`, 4096 bytes (placeholder — see open decisions) |

**Known gotcha:** CubeMX overwrites the generated linker script on every
regenerate. The FLASH origin/length edits must be re-applied, or the script
should be copied out of the generated tree and CMake pointed at the copy.

---

## Done

### Toolchain and build

- [x] `arm-none-eabi-gcc` on macOS (Intel) and Arch Linux; CMake + Ninja
- [x] LL drivers only — no HAL anywhere in either image
- [x] Peripheral footprint trimmed: GPIOA + USART2 only in the bootloader
- [x] Two independent CMake projects (`bootloader/`, `firmware/`) with separate
      linker scripts and flash addresses
- [x] `.bin` generation via post-build objcopy
- [x] Signed commits (SSH signing) on both development machines

### Boot chain

- [x] Serial console over ST-LINK VCP (USART2, 115200, polling, no DMA)
- [x] Two-stage boot: bootloader at sector 0 jumps to app at `0x08004000`
- [x] Jump implementation: read SP and reset vector, set `VTOR`, set MSP, branch
- [x] Bootloader measures the app slot (SHA-256) and prints the digest
- [x] Measurement verified against host-side `st-flash read` + `shasum -a 256`

### Crypto primitives

All verified against published test vectors before use.

- [x] SHA-256 — vendored, verified against host `shasum`
- [x] HMAC-SHA256 — written from RFC 2104, verified against RFC 4231 Test Case 1
- [x] HKDF-Extract / HKDF-Expand — written from RFC 5869, verified against
      RFC 5869 Test Case 1 (PRK and OKM both match)
- [x] Ed25519 keypair derivation — Monocypher (`monocypher-ed25519.c`),
      verified against RFC 8032 Test 1

**Decision:** Constructions (HMAC, HKDF) written in-house; primitives (SHA-256,
Ed25519) vendored. Rationale — compositions have no secret-dependent branching
and have RFC test vectors; primitives require constant-time implementations that
should not be hand-rolled.

**Decision:** MCUboot rejected in favour of a hand-rolled minimal bootloader.
Rationale — full accountability for every instruction in the measured boot path,
and a smaller flash footprint. MCUboot's image format and update machinery are
not needed for the claim.

---

## In progress

- [ ] Wire the real derivation chain end to end:
      `placeholder UDS → HKDF-Extract → PRK → HKDF-Expand(measurement) → CDI →
    Ed25519 keypair`
- [ ] Print the derived alias public key
- [ ] Demonstrate avalanche: modify the firmware, confirm the alias public key
      changes completely

---

## Phase 2 — Device-side root of trust

### DICE derivation

- [ ] Replace placeholder UDS with an OTP read
- [ ] Burn a real UDS (`openssl rand`) into a locked OTP block —
      **last step, after everything else works. 16 blocks available.**
- [ ] Derive DeviceID key from UDS alone (optional; skip for DP1)
- [ ] Explicit volatile-pointer wipe of `prk` and `secret_key`
      (`cdi` is wiped by `crypto_ed25519_key_pair` as a side effect)
- [ ] Verify-after-erase: read back wiped regions and confirm zero
- [ ] Zero derivation SRAM on entry as well as before jump (warm-reset remnants)

### Isolation

- [ ] Reset all DMA streams and gate DMA1/DMA2 clocks before touching the UDS
- [ ] MPU regions over UDS and derivation code; MPU-protect RCC to raise the
      cost of re-enabling DMA
- [ ] Drop to unprivileged Thread mode before the jump
- [ ] Bootloader resets peripherals it configured before handing off

### Attestation protocol

- [ ] Verifier sends a nonce over serial; device signs and responds
- [ ] **Nonce must be inside the signed message** — deterministic derivation
      makes replay trivial otherwise
- [ ] Token format: raw signed struct (measurement, nonce, alias pubkey).
      X.509 and CBOR/COSE deferred.

### Flash protection

- [ ] First stage in sectors 0–3, write-protected via WRP
- [ ] Verify a write to sector 0 is rejected once WRP is on
- [ ] **Never set RDP Level 2 — irreversible, permanently disables SWD.**
      RDP1 only, and only if needed.

---

## Phase 3 — Build pipeline

- [ ] Containerize the build, pin the image by digest
- [ ] gittuf verification as a pre-build CI gate
- [ ] Wrap the gittuf verification result as a signed in-toto predicate
- [ ] Emit SLSA provenance via Witness
- [ ] **Measurement descriptor tool** — recomputes the digest exactly as the
      bootloader does (same region, alignment, padding). This is the original
      engineering contribution.
- [ ] Emit both digests as separate subjects on one in-toto Statement:
      `firmware.bin` (build output) and `boot-measurement` (bootloader-style)

---

## Phase 4 — Verifier (Rust, host side)

- [ ] Send nonce over serial, receive token
- [ ] Verify signature and nonce freshness
- [ ] Extract the measurement
- [ ] Look up the in-toto Statement whose subject matches that measurement
- [ ] Verify statement signature against the trusted builder identity
- [ ] Read source repo and commit from the SLSA predicate
- [ ] Verify the gittuf attestation for that commit
- [ ] Print the full chain and a verdict

**Exit criterion:** one command takes a live device and prints commit,
reviewers, and pass/fail.

---

## Phase 5 — Reproducible builds

No single fix; timebox it. Partial results are still a publishable finding.

- [ ] Build identical firmware in two environments, diff the binaries
- [ ] **Document every source of nondeterminism found** — standalone result
- [ ] `-ffile-prefix-map` for absolute paths in debug info
- [ ] `SOURCE_DATE_EPOCH`; strip `__DATE__` / `__TIME__`
- [ ] Deterministic archives (`ar D`), sorted and pinned link order
- [ ] Pin linker script, toolchain version, CubeMX version
- [ ] Confirm signing determinism (Ed25519 — already satisfied)
- [ ] Measure reproducibility rate across N builds

**Known risk:** the two development machines currently run different
`arm-none-eabi-gcc` versions (macOS Homebrew vs. Arch `pacman`). Record both and
expect this to matter.

---

## Phase 6 — DP1 evaluation and writeup

### Attack table — demonstrate row 2 live

| Attack                                               | Plain secure boot | This system                                               |
| ---------------------------------------------------- | ----------------- | --------------------------------------------------------- |
| Unsigned image                                       | caught            | caught                                                    |
| **Signed image from an unreviewed commit**           | **missed**        | **caught**                                                |
| Signed image, source matches, build machine tampered | missed            | caught (reproducibility)                                  |
| Signed image, signing key stolen                     | missed            | caught (no provenance chain to a policy-compliant commit) |

### Measurements

- [ ] Flash delta, RAM delta, boot time delta, token size, verification latency
- [ ] Note that boot overhead is dominated by software SHA-256 — no accelerator
- [ ] Reproducibility rate across N builds

### Limitations — state before anyone asks

- Boot-time attestation reports what was measured at boot, not what is running now
- **No forward secrecy** — deterministic derivation means identical firmware
  yields the same CDI every boot. This is why the verifier nonce is mandatory.
- **Rollback not addressed** — old firmware attests correctly as old firmware;
  the verifier must reject by policy. Monotonic counter (RTC backup registers)
  out of scope for DP1.
- **DMA is not subject to the MPU** — key destruction, not the MPU, is the
  mitigation
- **Glitching the erase step is the primary unaddressed attack.** A fault at the
  key-destruction instruction leaves the UDS live. Single point of failure;
  mitigations exist and none are complete without glitch-detection hardware.
- UDS provisioning is trusted, not proven
- Reproducibility is only as strong as toolchain pinning
- Key revocation is prospective — the RSL gives ordering, but "signed before or
  after the compromise" is not solved here
- CDI is held in RAM by the application and inherits the application's attack
  surface. Blast radius is limited to the current firmware version, not the
  device's permanent identity.

### Properties worth claiming

- UDS in **locked OTP**, not write-protected flash — extractable only by
  physical attack on the die
- **Internal flash means no probeable memory bus** — no external address/data
  lines to tap, unlike designs with external QSPI flash
- Porting to a part with a factory-fused UDS is a **substitution, not a
  redesign**

---

## DP2 — Attacking the design (Spring)

Write this list during DP1 and let it shape the design. Attacks must target the
**architecture**, not the prototype's known shortcuts.

- [ ] Privilege escalation from the unprivileged app; confirm the UDS is
      unrecoverable regardless of outcome
- [ ] DMA-based read of the UDS region during the pre-erase window
- [ ] **Voltage/clock glitch the key-destruction step** — determine whether the
      UDS survives. Cheap gear, real attack on the actual claim.
- [ ] Get an alias key certified for firmware that is not what executed
- [ ] Replay a token across a reflash
- [ ] Find a gap between the measured region and the executed region
- [ ] Find a build input that changes behaviour without changing the measurement
- [ ] Bypass the gittuf gate — commit signed by a since-revoked key, RSL
      manipulation
- [ ] Attempt UDS extraction from locked OTP
- [ ] Write up what held, what did not, and what a stronger anchor would have
      prevented

---

## Open decisions

- [ ] **Measured byte range.** Currently 4096 bytes at `0x08004000` — arbitrary,
      and the firmware is larger than that. Options: fixed length, length field
      written by the build tool, or the whole slot padded to `0xFF`. Whole-slot
      is where this lands, because it closes the leftover-code gap (flashing a
      short image over a longer one leaves stale bytes past the image end that a
      size-bounded measurement never covers). Cost: hashing ~200 KB on every
      boot, roughly 60–100 ms in software.
- [ ] Does the verifier fetch the gittuf attestation live, or is it bundled with
      the provenance?
- [ ] Is an L552 comparison run worth it in spring to quantify what a hardware
      anchor actually buys?
- [ ] Does the DeviceID key get added in DP2, or stay out entirely?
- [ ] Project name — `senior-dp` is the placeholder

**Dropped:** FPGA as external boot observer. Internal flash means there is no
bus to tap; nothing to observe.

---

## Risks

| Risk                                         | Mitigation                                                 |
| -------------------------------------------- | ---------------------------------------------------------- |
| Reproducibility becomes an endless hunt      | Timebox; partial results are a finding                     |
| gittuf metadata ingestion not ready          | Scope DP1 to what exists today                             |
| OTP burned before derivation code is correct | 16 blocks available; develop with a placeholder, burn last |
| RDP2 set accidentally                        | Never set it; RDP1 only                                    |
| Toolchain version drift between dev machines | Pin and record both; containerize in Phase 5               |
