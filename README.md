# Senior Design — Hardware-Rooted Supply Chain Attestation

## 1. Problem

Secure boot verifies that a firmware image carries a valid signature. It proves that someone holding the signing key approved the image. It proves nothing about the image's origin — not which source it was built from, not whether that source was reviewed, not whether the build machine was honest.

Build-side provenance (in-toto / SLSA) records exactly that information, but it terminates at the artifact in a release pipeline. It never reaches the device, and no deployed device can report which artifact it is actually executing in a form a verifier can match against provenance.

The two halves exist and do not connect.

**Claim.** A remote verifier can determine whether the firmware executing on a specific device is the artifact produced by a reproducible build of a source commit that satisfied repository policy.

**Scope.** Non-TrustZone MCUs. Target is the deployed brownfield of pre-Armv8-M parts that have no attestation story at all, rather than parts where ARM already ships PSA Initial Attestation.

**Platform.** Nucleo-F411RE (Cortex-M4, 512 KB flash, 128 KB SRAM, no TrustZone, no TRNG, no crypto accelerator, 16 lockable OTP blocks of 32 bytes).

---

## 2. Threat model

**In scope**

- Compromised or malicious source contribution (unreviewed commit, bypassed policy, history rewrite)
- Compromised build environment producing a signed image that does not correspond to reviewed source
- Firmware signing key compromise
- Software compromise of the device application at runtime, including privilege escalation and DMA-based memory access

**Out of scope**

- Physical attacks: die decapping, microprobing, voltage and clock glitching
- Runtime compromise occurring after measurement (attestation reports boot state, not current state)
- Correctness of initial UDS provisioning (trusted, not proven)
- Compromise of the verifier host

---

## 3. Approach

### 3.1 Chain of attestations (host side)

Three signed statements, each an in-toto Statement, linked by digest:

1. **Source policy** — gittuf verifies the commit against signed repository policy before the build runs. The verification result is wrapped as an in-toto predicate. Answers: _should this code exist?_
2. **Build provenance** — SLSA provenance emitted by Witness. Answers: _where did this binary come from?_
3. **Device measurement** — the device reports what it booted. Answers: _is that binary what is actually running?_

**The binding problem.** The bootloader measures a flash region; the build system digests an output file. Same firmware, different numbers — different byte range, alignment, padding, header handling. Making these connect is the central engineering contribution.

**The solution.** An in-toto Statement's `subjects` field is a list. A build-time tool recomputes the digest exactly the way the bootloader will and emits it as a second subject alongside the artifact digest:

```
subjects: [
  { name: "firmware.bin",     digest: { sha256: <digest of build output> } },
  { name: "boot-measurement", digest: { sha256: <recomputed bootloader-style> } }
]
```

The verifier looks up by measurement, reads the commit off the predicate. No device-side changes, no circular dependency (embedding provenance in the image would change the image and therefore the provenance).

**Design principle: keep the device stupid.** It reports a measurement and signs a nonce. It knows nothing about in-toto, gittuf, JSON, or provenance. All chaining happens host-side.

### 3.2 Device-side root of trust (DICE)

No TrustZone means no secure world, so the attestation key cannot be isolated in space. DICE isolates it in time instead.

```
first stage (immutable, sectors 0-3, write-protected)
  ├── UDS in locked OTP block
  ├── measurement = SHA-256(application region)
  ├── CDI = HKDF(UDS, measurement)
  ├── alias_keypair = derive(CDI)
  ├── ERASE UDS and CDI, lock access          <- the guarantee
  └── jump to application (unprivileged, MPU configured)

application
  └── answers verifier nonce, signs with alias_keypair
```

**Why it holds.** A compromised application holds a key derived from its own measurement. Computing any other key requires the UDS, which no longer exists in the system. It can lie about anything except what it is. Wrong firmware produces a different key, and signature verification fails.

**Why no RNG is needed.** Every key is derived by hashing. Freshness comes from the verifier's nonce. Ed25519 signing is deterministic (RFC 8032), so no signature nonce is required — which also avoids the ECDSA `k`-reuse failure class.

### 3.3 Defense in depth, and what it is not

MPU regions over the UDS and derivation code, with the application dropped to unprivileged Thread mode, raise the cost of the easy attacks. They are **hardening, not the guarantee**, because:

- The MPU is software-configured; privileged code can reprogram it, and Armv7-M has no secure state, only a permission system
- Cortex-M privilege escalation has real surface (SVC handlers, VTOR relocation, exception return manipulation)
- **DMA is a bus master and is not subject to the MPU at all**

Key destruction is what survives all of these. An escalated attacker gains privileged access to a value that is no longer present.

---

## 4. Methodology and work items

### Phase 1 — Platform bring-up (Sept → mid-Oct)

Highest schedule risk in the project. Nothing downstream exists until the board reports a hash of what it booted. Start immediately.

- [x] Blink an LED — validate toolchain, flashing, debugger
- [x] Serial console over the Nucleo's onboard ST-LINK VCP (no extra hardware needed)
- [x] Import sha256 library and crosscheck using host machine's checksum
- [x] Build a small boot stage that hashes and passes onto firmware
- [ ] Read X-CUBE-SBSFU for how ST builds a root of trust without TrustZone
- [ ] **Do not set RDP Level 2. It is irreversible and permanently kills SWD.** RDP1 for development only
- [x] Bootloader hashes the application region and prints the measurement
- [x] Verify measurement against host-side sha256sum of the same flash region

**Exit criterion:** the board reports a hash of what it booted.

### Phase 2 — Device-side DICE (mid-Oct → Nov)

- [ ] First stage located in sectors 0–3, write-protected via WRP (16 KB granularity — the small sectors are at the bottom of the map, which is exactly what's wanted)
- [ ] Integrate Monocypher (single file, public domain, Ed25519, no RNG dependency)
- [ ] HKDF-SHA256 derivation — reuses the SHA-256 already needed for measurement
- [ ] Burn UDS into an OTP block and lock it — **16 blocks available, so 16 attempts; do not burn until derivation code works**
- [ ] Derive CDI from `HKDF(UDS, measurement)`, derive alias keypair
- [ ] **Erase UDS and CDI from RAM, lock OTP access, before jumping**
- [ ] Reset all DMA streams and gate DMA peripheral clocks in first stage before touching the UDS
- [ ] Configure MPU regions over UDS and derivation code; MPU-protect RCC to raise the cost of re-enabling DMA
- [ ] Drop to unprivileged Thread mode before jump
- [ ] Application answers a nonce challenge with the alias key
- [ ] **Nonce must be inside the signed message** — deterministic keys make replay trivial otherwise
- [ ] Token format: raw signed struct (measurement, nonce, alias pubkey). Skip X.509 and DeviceID key for DP1; both are formatting problems, not conceptual ones

### Phase 3 — Build pipeline (Nov)

- [ ] Containerize the build, pin by digest
- [ ] gittuf verification as a pre-build CI gate
- [ ] Wrap the gittuf verification result as a signed in-toto predicate
- [ ] Emit SLSA provenance via Witness
- [ ] **Write the measurement descriptor tool** — recomputes the digest exactly as MCUboot does (same region, alignment, padding). This is the original engineering contribution
- [ ] Emit both digests as subjects on one in-toto Statement

### Phase 4 — Verifier (Nov, Rust, host side)

- [ ] Send nonce over serial, receive token
- [ ] Verify signature, check nonce freshness
- [ ] Extract measurement
- [ ] Look up the in-toto Statement whose subject matches the measurement
- [ ] Verify statement signature against trusted builder identity
- [ ] Read source repo and commit from the SLSA predicate
- [ ] Verify the gittuf attestation for that commit
- [ ] Print the full chain and a verdict

**Exit criterion:** one command takes a live device and prints commit, reviewers, pass/fail.

### Phase 5 — Reproducible builds (Nov → Dec)

Whack-a-mole; no single fix. Timebox it — partial results are still a finding worth writing up.

- [ ] Build identical firmware in two environments, diff the binaries
- [ ] **Document every source of nondeterminism found** — standalone DP1 result
- [ ] `-ffile-prefix-map` for absolute paths in debug info
- [ ] `SOURCE_DATE_EPOCH`; strip `__DATE__` / `__TIME__`
- [ ] Deterministic archives (`ar D`)
- [ ] Sorted, pinned link order; pinned linker script and toolchain version
- [ ] Confirm signing determinism (Ed25519 — already satisfied)
- [ ] Measure reproducibility rate across N builds

### Phase 6 — DP1 evaluation and writeup (Dec)

- [ ] Threat model written explicitly
- [ ] **Attack table demonstrated live**, especially row 2 — the entire justification for the project:

| Attack                                               | Plain secure boot | This system                                               |
| ---------------------------------------------------- | ----------------- | --------------------------------------------------------- |
| Unsigned image                                       | caught            | caught                                                    |
| Signed image built from an unreviewed commit         | **missed**        | **caught**                                                |
| Signed image, source matches, build machine tampered | missed            | caught (reproducibility check)                            |
| Signed image, signing key stolen                     | missed            | caught (no provenance chain to a policy-compliant commit) |

- [ ] Cost measurements: flash delta, RAM delta, boot time delta, token size, verification latency. Note that boot overhead is dominated by software SHA-256 — no crypto accelerator on this part
- [ ] Reproducibility rate
- [ ] Limitations section (§5)

---

## 5. Known limitations — state these before anyone asks

- Boot-time attestation reports what was measured at boot, not what is running now. Post-boot runtime compromise defeats it.
- **No forward secrecy.** Deterministic derivation means identical firmware yields the same key on every boot. This is precisely why the verifier nonce is mandatory.
- **Rollback is not addressed.** Old firmware attests correctly as old firmware; the verifier must reject by policy. Binding a monotonic counter (RTC backup registers) is out of scope for DP1.
- **DMA is not subject to the MPU.** Key destruction, not the MPU, is the mitigation.
- **Glitching the erase step is the primary unaddressed attack.** A fault injected at the key-destruction instruction leaves the UDS live. Single point of failure; mitigations exist and none are complete on a part without glitch detection.
- UDS provisioning is trusted, not proven.
- Reproducibility is only as strong as toolchain pinning.
- Key revocation is prospective — the RSL gives ordering, but "was this signed before or after the compromise" is not solved here.

**Properties worth claiming, not apologising for:**

- UDS lives in **locked OTP**, not write-protected flash — extractable only by physical attack on the die
- **Internal flash means no probeable memory bus.** No external address or data lines to tap, unlike designs with external QSPI flash
- Porting to a part with a factory-fused UDS is a **substitution, not a redesign**

---

## 6. DP2 — Attacking the design (Spring)

Write this list during DP1 and let it shape the design. Attacks must target the **architecture**, not the prototype's known shortcuts — dumping a key that was never protected proves nothing.

- [ ] Privilege escalation from the unprivileged application; confirm the UDS is unrecoverable regardless of outcome
- [ ] DMA-based read of the UDS region during the window before erase
- [ ] **Voltage/clock glitch the key-destruction step** — determine whether the UDS survives. Cheap gear, real attack on the actual claim, publishable either way
- [ ] Get an alias key certified for firmware that is not what executed
- [ ] Replay a token across a reflash
- [ ] Find a gap between the measured region and the executed region
- [ ] Find a build input that changes behaviour without changing the measurement
- [ ] Bypass the gittuf gate — commit signed by a since-revoked key, RSL manipulation
- [ ] Attempt UDS extraction from locked OTP (documents the anchor's real strength)
- [ ] Write up what held, what did not, and what a stronger anchor would have prevented

---

## 7. Open decisions

- [ ] Does the verifier fetch the gittuf attestation live, or is it bundled with the provenance?
- [ ] Is a Nucleo-L552 comparison run worth it in spring to quantify what a hardware anchor actually buys? (L552 is far closer to F411 money than a U585 kit)
- [ ] Does the DeviceID key get added in DP2, or stay out entirely?

**Dropped:** FPGA as external boot observer. Internal flash means there is no bus to tap — nothing to observe.

---

## 8. Risks

| Risk                                         | Mitigation                                                |
| -------------------------------------------- | --------------------------------------------------------- |
| MCUboot / toolchain bring-up overruns        | Start in September; most likely source of surprise        |
| Reproducibility becomes an endless hunt      | Timebox; partial results are a publishable finding        |
| gittuf metadata ingestion not ready          | Scope DP1 to what exists today; treat the rest as stretch |
| OTP burned before derivation code is correct | 16 blocks available, but do not burn until tested         |
| RDP2 set accidentally                        | Never set it; RDP1 only                                   |
