# Waldorf Wave Firmware Availability and Its Relevance to Sample-Accurate Emulation

_Research status: 23 August 2026_

## Executive summary

The Waldorf **Wave**, rather than the Microwave, is the target instrument. Its firmware is unusually promising for an emulation project because the original system is split across a main CPU board and as many as three 16-voice WDV digital voice boards. Each side runs 68000-family code, and the main system loads a separate program into the WDV boards during startup.

The two important files are:

- `w2sys.bin`: the Wave's main operating system, executed by the main CPU and responsible for the user interface, storage, MIDI, voice allocation, system management, and WDV-board startup.
- `wdv.sys`: the executable downloaded to each WDV voice board. It is the most direct surviving software description of the boundary between high-level synthesis parameters and the Wave's oscillator ASICs, waveform memory, control-voltage path, and per-voice analogue hardware.

Waldorf currently provides a public `System.zip` download from its official [Legacy Wave page](https://waldorfmusic.com/legacy-wave/). The archive contains Waldorf OS releases 1.668, 1.671, 1.680, and 1.700. Inspection of that archive shows that all four releases contain the **same 12,840-byte `wdv.sys`**, while `w2sys.bin` changes between versions. That is a high-value reverse-engineering result: changes such as the known 1.700 voice-allocation problem are probably on the main-OS side, while the public Waldorf voice-board program remained stable across these releases.

Public download does not automatically grant permission to redistribute the binaries. The emulator should therefore download or ask the user to supply the files, authenticate them by hash, and keep them out of the source repository.

Firmware is necessary but not sufficient for sample-accurate emulation. It can recover control algorithms, timing, tables, voice allocation, board communication, and ASIC/DAC writes. It cannot by itself reveal undocumented ASIC internals or the exact analogue response of the multiplexers, sample-and-hold stages, DACs, CEM3387 filters/VCAs, panning path, and component tolerances.

## 1. Why the Wave firmware split matters

The Wave is not simply a Microwave with a larger panel. The service documentation describes a main CPU board and separate WDV digital voice boards; diagnostics can reset, start, and communicate with WDV boards 1 through 3. A WDV board handles 16 voices, with two eight-voice Waldorf oscillator ASICs on the board. The system can therefore scale to 48 voices by installing three WDV boards.

The effective control topology is:

```text
MIDI / keyboard / panel / disk
               |
               v
       Main 68000 CPU board
             w2sys.bin
               |
      system bus / shared state
               |
        +------+------+------+
        |             |      |
      WDV 1         WDV 2  WDV 3
     16 voices     16       16
        |
   WDV 68000 running wdv.sys
        |
  two 8-voice Waldorf ASICs
        |
 waveform conversion, CV DACs,
 S/H, CEM3387 filter/VCA/panning
        |
       audio
```

The [Wave service manual](https://www.waldorf-wave.de/Waldorf-WAVE-Service-Manual.pdf) is the primary hardware map. Its schematics expose the WDV CPU, 32 MHz board oscillator and 16 MHz CPU clock, two ASIC interfaces, waveform RAM buses, S-bus interface, reset/status logic, and downstream voice circuitry. The [unofficial Wave hardware page](https://www.waldorf-wave.de/wavetech.html) independently summarizes the same main/WDV 68000 and 16-voice-board structure.

The [official Waldorf Microwave 1 plug-in technical page](https://waldorfmusic.com/microwave/) explicitly states that the first-generation Microwave and the Wave use the same custom Waldorf ASIC, and identifies the original internal synthesis rate as 250 kHz. Microwave 1 service schematics and circuit measurements are therefore valuable independent evidence for the ASIC's external clocking, waveform-RAM/DAC interface, quantization and reconstruction path. They cannot replace Wave-specific WDV traces, since the surrounding voice count, control protocol and analogue board topology differ.

The current behavioural proxy now clocks its oscillator lookup, signed eight-bit two-oscillator mixer, ASIC high-pass and reconstruction input at 250 kHz. The ES2 mix stage wraps rather than clips when its weighted signed output crosses the eight-bit boundary, matching Waldorf's documented “ASIC Mix Bug” test at combined oscillator levels above 128. The following VCF input has a separate mild saturation curve beginning near 70% mixer output. These are evidence-backed boundaries, not a claim that the unknown ASIC's internal multiply/accumulate gate ordering has been recovered; controlled Wave/Microwave captures remain necessary to tune the final bit-level hypothesis.

This separation is useful because it gives the reverse-engineering work two comparatively clean targets:

1. recover the main OS's loader, voice allocation, parameter calculation, and WDV protocol from `w2sys.bin`; and
2. recover the WDV scheduler and hardware-facing writes from `wdv.sys`.

## 2. Current public availability

### Official Waldorf archive

As of the research date, Waldorf's [Legacy Wave page](https://waldorfmusic.com/legacy-wave/) exposes a **System** download. It resolves to a public Waldorf-hosted Nextcloud share named [`System.zip`](https://downloads.waldorfmusic.com/cloud/index.php/s/7zzTnmzK2pYY9Ks). The direct download used by this project is:

```text
https://downloads.waldorfmusic.com/cloud/index.php/s/7zzTnmzK2pYY9Ks/download
```

The outer archive contains:

```text
System/wave_sys1_700.zip
System/wave_old_systems.zip
```

The first archive contains OS 1.700. The old-systems archive contains 1.668, 1.671, and 1.680 in several period-appropriate packaging formats. The actual boot files are ordinary firmware images and must not be treated as archives merely because one is named `.bin`. The [unofficial Wave boot-disk instructions](https://unofficial.waldorf-wave.de/waveif.html) likewise identify `w2sys.bin` and `wdv.sys` as the two required system files.

The project already provides `scripts/fetch-official-firmware.sh`, which fetches the Waldorf archive and extracts OS 1.700. Keeping acquisition in a script is preferable to committing the firmware.

### Verified Waldorf releases and file identities

The following values were measured from Waldorf's official archive on the research date:

| OS | `w2sys.bin` size | `w2sys.bin` SHA-256 | `wdv.sys` size | `wdv.sys` SHA-256 |
|---|---:|---|---:|---|
| 1.668 | 296,376 | `42319929e0ec55fd84d2d247e57ccd5321fde610ba936adf114db48ee999d3e3` | 12,840 | `bdf379b07785af313068191961ee5ef408e267740c1b598290732477bf9d3f68` |
| 1.671 | 296,408 | `e429bfe8d3e1f00425a5c86d06d219d650644d212cb6dda4349e9b2f19aed0ed` | 12,840 | `bdf379b07785af313068191961ee5ef408e267740c1b598290732477bf9d3f68` |
| 1.680 | 296,092 | `fa1d5ea2d090206246ca4df33cecfb812854e25bb72018b48e673f4552f24494` | 12,840 | `bdf379b07785af313068191961ee5ef408e267740c1b598290732477bf9d3f68` |
| 1.700 | 302,768 | `4282457d9bf7d70da2e2aa4d6a1e69467c178d0d8d8be0a27f524ab8d62e3286` | 12,840 | `bdf379b07785af313068191961ee5ef408e267740c1b598290732477bf9d3f68` |

The identical `wdv.sys` hash is important enough to restate: the WDV image in 1.668, 1.671, 1.680, and 1.700 is byte-for-byte identical. The public version history is therefore mainly a differential-analysis corpus for `w2sys.bin`, not four revisions of the voice-board program.

### Other known versions

- The Wave System Exclusive description names **OS 1.400** as its target. This establishes an earlier software generation and is useful for interpreting parameter and dump structures, but a 1.400 firmware pair is not present in Waldorf's current `System.zip`. See the [Wave SysEx manual](https://unofficial.waldorf-wave.de/os/OS_1-900_WAVE_Sysex_manual.pdf).
- The unofficial support page recommends **1.680** for many original instruments and reports a voice-allocation bug in **1.700**. The later OS 1.8/1.9 documentation also says that 1.8 fixed the 1.700 missing-note/voice-allocation problem. This is a valuable hint that the affected code is in `w2sys.bin`, because the official 1.680 and 1.700 WDV images are identical.
- Later **1.8xx and 1.9xx** releases are community-developed successors rather than files in Waldorf's current official system archive. Their documentation describes personalization and key handling. For example, the [OS 1.911 manual](https://unofficial.waldorf-wave.de/os/OS_1-911.pdf) shows a larger 15,112-byte `WDV.SYS` and a keyed installation flow. Those images should be treated as a separate provenance and licensing case, not assumed to be interchangeable or freely redistributable.

For the first emulation milestone, OS 1.700 is the sensible canonical target because it is the latest Waldorf-authored version in the current official download, has known hashes, and matches the project's existing firmware loader. OS 1.680 should be retained as the primary comparison build and possibly as a user-selectable compatibility target.

## 3. Roles of the two files

### `w2sys.bin`: main Wave operating system

`w2sys.bin` is a raw, big-endian 68000-family program image for the main CPU board. The 1.700 image starts with absolute 68k jump instructions whose targets fall within the image, which gives a useful initial anchor for discovering the load base and startup path.

Static strings in OS 1.700 show that the main program:

- locates and reads `WDV.SYS` from the boot disk;
- rejects it if it is not a recognized executable object;
- resets, detects, starts, and checks multiple voice boards;
- reports old- versus new-style voice hardware;
- initializes oscillators and loads wavetables to voice boards;
- contains the system monitor and WDV diagnostic functions; and
- owns user-facing voice allocation and bad-voice handling.

This makes `w2sys.bin` essential for faithful instrument behaviour. It is likely to contain or coordinate:

- MIDI and keyboard event processing;
- panel editing and modulation parameter normalization;
- voice allocation, stealing, sustain, and performance layers;
- patch, wavetable, tuning, and machine-specific data management;
- envelope/LFO setup or control-rate generation, depending on which work is delegated to WDV;
- commands and data transfers to each WDV board; and
- boot-time transfer and launch of `wdv.sys`.

It is not safe to assume that envelopes or LFOs run entirely on one CPU. Static and dynamic tracing must determine whether the main CPU sends high-level parameters, control-rate values, or a mixture of both.

### `wdv.sys`: WDV voice-board executable

`wdv.sys` is small enough to reverse engineer thoroughly and close enough to the sound-generating hardware to be exceptionally valuable.

Preliminary binary inspection gives several strong anchors:

- It is a 12,840-byte big-endian m68k object with a 32-byte a.out-style header. Common `file` implementations identify it as a SunOS m68k demand-paged executable. That label describes the object container; it is **not** evidence that the Wave contains a 68020. The WDV schematic and clocking point to a 68000-class CPU.
- The first four bytes are `01 02 01 0b`, and the header contains `0x00003228`, equal to 12,840 decimal. The precise a.out variant and Waldorf loader rules should be confirmed from the `w2sys.bin` loader rather than assumed from a host tool's guess.
- Immediately after the 32-byte header is a plausible 68000 vector table. Its first two longwords are a candidate initial stack pointer of `0x00008FFE` and reset/program counter of `0x00000400`.
- The remaining early longwords resemble exception-vector targets, giving a natural way to distinguish vectors, startup code, interrupt handlers, and data.

The likely high-value contents are:

- the WDV reset and interrupt handlers;
- the board's command loop or mailbox consumer;
- per-voice state updates and scheduling;
- ASIC command/register writes and waveform-RAM access;
- pitch-to-increment or tuning conversions;
- wavetable position, wave selection, interpolation, and modulation handling;
- CV DAC update scheduling for filters, VCAs, and panning;
- envelope, LFO, glide, and control-rate tables if those functions execute on WDV; and
- board status, acknowledgements, error reporting, and synchronization with the main CPU.

### Why `wdv.sys` is the prize

The oscillator ASIC is undocumented. A schematic can show address lines, data lines, chip selects, clocks, and attached RAM, but not the semantic meaning of every write. `wdv.sys` must drive that interface correctly. Consequently, every hardware access made by the WDV code is evidence about the ASIC protocol.

Once the executable is running in an instrumented CPU emulator, the project can record a timestamped trace such as:

```text
WDV CPU cycle -> address -> value -> read/write -> decoded device/voice
```

Correlating those traces with controlled note, pitch, wavetable, envelope, and modulation changes can reveal:

- which address ranges select ASIC A versus ASIC B;
- how voices 1-8 and 9-16 are selected;
- the format and width of pitch and wavetable-position values;
- whether updates are immediate, double-buffered, latched, or interrupt-driven;
- the control-rate cadence and ordering of oscillator and CV updates; and
- how the firmware synchronizes shared data with the main CPU.

That is much stronger evidence than reconstructing the ASIC solely from audible output. It still does not disclose the ASIC's internal oscillator arithmetic, interpolation, aliasing, or truncation rules; those require hardware measurements, logic traces, patent/design evidence if available, and hypothesis testing against real output.

## 4. Relevance to sample-accurate emulation

“Sample-accurate” should be used narrowly. At a minimum, the same input event and initial state must produce deterministic hardware-side events at the same emulated times, and those events must affect the rendered host sample at the correct fractional-sample position.

A credible Wave model therefore needs:

1. **Firmware-accurate control behaviour.** The original voice allocation, modulation rules, quantization, tables, update order, and edge cases should come from executed firmware where practical.
2. **Clock-domain scheduling.** Main-CPU execution, WDV execution, interrupts, shared-bus handshakes, ASIC clocks, and control updates need one monotonic emulated timeline. Host audio block boundaries must not change the result.
3. **Timestamped side effects.** ASIC, waveform-RAM, DAC, multiplexer, and gate writes must be timestamped rather than collapsed into one value per host block.
4. **Digital voice modelling.** The ASIC must remain a black box. A behavioural proxy for wavetable addressing, phase accumulation, interpolation/truncation, mixing, and output quantization can only be inferred and validated from its external bus and real-hardware measurements; this cannot establish the undocumented internal implementation.
5. **Mixed-signal modelling.** The time-multiplexed conversion, sample-and-hold behaviour, 12-bit control-voltage stepping, reconstruction stages, CEM3387 filter/VCA/panning response, and output path need explicit models.

Executing both firmware images without those hardware models would reproduce the control computer but not the instrument's sound. Conversely, a good DSP approximation without the firmware can sound Wave-like while still getting voice allocation, timing, modulation, and obscure behaviours wrong. The goal is to join the two at the same observable hardware boundary.

Cycle-exact CPU emulation may not be necessary everywhere, but it is the safest initial reference. After traces show which timings are audible or state-significant, noncritical regions can be optimized while preserving the same timestamped I/O sequence.

## 5. Recommended reverse-engineering workflow

### Step 1: preserve provenance and build a manifest

- Acquire `System.zip` only from Waldorf's official Legacy Wave link.
- Record retrieval date, source URL, byte size, and SHA-256 for the outer and nested archives.
- Extract every Waldorf version into a local research area excluded from version control.
- Record the per-file sizes and hashes shown above.
- Make the emulator accept user-supplied files and identify exact known revisions. Never silently accept a different image as 1.700.

The existing `FirmwareBundle` already authenticates the 1.700 pair. Extend its manifest to recognize the official 1.668, 1.671, and 1.680 main images while recording that their WDV image is shared.

### Step 2: confirm CPU architecture and object formats

- Trace the CPU part number, clock, reset, interrupt, bus, RAM, and chip-select nets in the service schematics for both the main and WDV boards.
- Configure analysis as big-endian Motorola 68000 initially. Do not select 68020 merely because a host utility uses that label for the a.out header.
- Parse the `wdv.sys` header explicitly. Confirm magic, text/data/BSS fields, relocation assumptions, entry semantics, and the loader's copy/zero/launch sequence in `w2sys.bin`.
- Determine whether the WDV vector table begins at file offset `0x20`, whether it is copied to board address `0x000000`, and whether reset begins at `0x000400`.
- Determine the main image base by following its opening jump stubs, absolute references, reset mapping, and the main-board memory decode.

The load address should be proven from both code and hardware decode. A disassembly that happens to look plausible at one base is not enough.

### Step 3: construct the memory maps

For each CPU, create a table containing:

- ROM or downloaded code;
- local SRAM/DRAM and BSS;
- shared/global RAM windows;
- system-bus address windows;
- ASIC A and B interfaces;
- waveform RAM;
- DAC, timer, interrupt, reset, status, and watchdog registers; and
- any programmable-logic-controlled aliases or mirrors.

Start from schematic chip-select equations and then validate each range against absolute-address references in the disassembly. Name addresses by evidence level: `confirmed`, `probable`, or `unknown_io_x`.

### Step 4: recover the main OS's WDV loader

The main OS contains excellent string anchors around reading, validating, starting, and checking `WDV.SYS`. Work outward from those references to recover:

1. disk read and object validation;
2. header parsing;
3. WDV reset assertion and release;
4. transfer destination and length;
5. BSS clearing or relocation processing;
6. start address/vector handling;
7. board-presence and ready polling; and
8. timeout, retry, and error paths.

Implementing this loader behaviour first provides a concrete success criterion: an emulated WDV should reach the same ready state that the main OS expects.

### Step 5: recover main-to-WDV communication

The service schematics and diagnostics indicate an S-bus/shared-state relationship, but the exact protocol must be derived. Identify:

- per-board address selection and board numbering;
- reset, fault, setup, status, and interrupt lines;
- shared-memory or mailbox layout;
- ownership and bus-arbitration rules;
- command and acknowledgement fields;
- sequence counters, ready flags, and timeout rules;
- voice-to-board mapping; and
- wavetable bulk-transfer paths.

Use cross-references from the main OS's voice-board error strings and from WDV interrupt handlers. Then run both CPUs under one deterministic scheduler with all shared-memory reads, writes, interrupts, and status transitions logged.

### Step 6: identify ASIC and CV side effects

On WDV, enumerate every access outside ordinary code/RAM. Group accesses by address, instruction site, interrupt context, and repetition rate. For each candidate register or port:

- vary one high-level parameter at a time;
- compare the resulting write sequence;
- test voice 1 versus 8, 9, and 16 to expose chip and voice selection;
- test minimum, midpoint, maximum, negative, and wrapped values;
- measure write cadence and phase relative to WDV interrupts; and
- determine whether values are raw, table-transformed, split across bytes/words, or latched by a later write.

Do not name a register from a single correlation. Require at least two independent experiments or a matching schematic/code clue.

### Step 7: locate envelope, LFO, pitch, and control-rate tables

Search both binaries because responsibilities may be divided between CPUs. Useful methods include:

- locating monotonic or symmetric word/longword arrays;
- testing candidate tables as big-endian signed and unsigned values;
- finding multiply/divide or indexed-load code near pitch, envelope, glide, and LFO parameter paths;
- comparing 1.668/1.671/1.680/1.700 main images to isolate changed routines and tables;
- tracing from SysEx/patch fields through normalized parameters to shared-memory writes; and
- observing table indices and outputs dynamically.

Extracted tables should carry their source image hash, file offset, interpreted type, and evidence. Avoid committing copyrighted bulk data unless redistribution rights are clear; generating derived tests or asking the user to supply firmware at runtime is safer.

Initial OS 1.700 result: the WDV executable body contains a 128-entry big-endian signed-word modulation table at offset `0x25e8`. Its entries follow `sign(n) * 8 * n^2` for signed amount `n = -64...+63`, with the negative endpoint saturated to `-32767`. General modulation destinations apply this table once. The oscillator-pitch routines at `0x254e` and `0x25a6` apply the table twice, producing a signed fourth-power depth curve. Both oscillator callers then apply `asr.l #3` at `0x0e34` and `0x0eea` before adding the result to their pitch accumulators. Pitch accumulation uses 256 internal units per semitone. Including that caller scaling makes the factory value `+12` approximately ±0.020 semitone at a full-scale LFO, rather than the ±2.25 semitones produced by a linear interpretation.

### Step 8: build a deterministic dual-CPU execution harness

The first harness should prioritize observability over real-time speed:

- one main 68000 instance;
- one WDV 68000 instance initially, expandable to three;
- a single emulated-time scheduler based on device clock cycles;
- instrumented memory and I/O handlers;
- interrupt and reset tracing;
- reproducible disk/boot inputs; and
- trace checkpoints for boot, note-on, note-off, pitch bend, wavetable movement, and modulation.

The service schematic's 32 MHz oscillator divided to a 16 MHz WDV CPU clock supplies a starting timing anchor. Other device rates must be derived from the schematics and measured behaviour rather than inferred from host sample rate.

### Step 9: turn traces into the ASIC contract

Define an internal interface at the observable WDV hardware boundary, for example:

```text
write(time, device, address, value)
read(time, device, address) -> value
interrupt(time, source, level)
```

Keep the firmware-facing register model separate from the oscillator algorithm. This allows multiple ASIC hypotheses to be tested against the same authoritative firmware trace and lets a future gate-level or measured model replace an approximation without changing CPU emulation.

### Step 10: validate against real hardware

Use several independent oracles:

- service diagnostics and expected boot/status messages;
- logic-analyser captures of WDV bus, ASIC selects, clocks, DAC updates, and shared-bus activity where safe;
- audio captures for controlled patches at multiple notes, velocities, wavetable positions, filter settings, and modulation rates;
- repeated captures to separate deterministic behaviour from analogue tolerance/noise; and
- cross-version tests, especially 1.680 versus 1.700 voice allocation under identical MIDI sequences.

Validation should compare event traces before comparing audio. If the ASIC/DAC write stream is wrong, tuning the analogue model to hide it will produce a fragile imitation.

## 6. Legal and practical caveats

### Publicly downloadable is not necessarily redistributable

Waldorf publicly hosts the archive, which is strong evidence that owners and researchers can legitimately obtain the historical system files from the manufacturer. It is not a clear software license granting third parties permission to mirror, bundle, modify, or commercially redistribute the binaries.

Recommended project policy:

- do not commit `System.zip`, `w2sys.bin`, `wdv.sys`, factory wavetables, or derived bulk firmware data;
- direct users to Waldorf's official Legacy Wave page or provide an opt-in fetch script;
- authenticate user-supplied files by hash;
- store only hashes, sizes, format metadata, addresses, annotations, and independently written emulation code in the repository;
- keep clean-room boundaries available if the project may become commercial; and
- obtain legal advice before distributing firmware-derived tables or translated code.

### Version provenance matters

Do not call every pair of correctly named files “Waldorf OS 1.700.” Verify both images together. A mixed pair may boot but invalidate research results. Later community OS images have different sizes, features, and key/personalization behaviour and need separate approval, manifests, and test expectations.

### Hardware revisions matter

The main OS detects old- and new-style voice hardware. A single software model may therefore need multiple WDV/analogue-board configurations, timing variations, or compatibility behaviours. Machine-specific calibration and filter-adjustment data should not be mistaken for firmware or shared blindly between instruments.

### Firmware does not guarantee bit-perfect sound

The ASIC design and analogue voice chain remain only partly documented. Component variation means that even two physical Waves will not necessarily null against each other. The defensible goal is deterministic, traceable digital/control behaviour plus a measured analogue model with documented tolerances—not an unsupported “bit-perfect” claim.

## 7. Recommended next-step checklist

### Acquisition and evidence

- [ ] Run `scripts/fetch-official-firmware.sh` and verify the 1.700 hashes against this document.
- [ ] Extend the acquisition script to optionally extract the old-systems archive without committing any binaries.
- [ ] Add a machine-readable manifest for 1.668, 1.671, 1.680, and 1.700.
- [ ] Record archive provenance and retrieval date in a research log.
- [ ] Add firmware and analysis-project directories to ignore rules if they are not already excluded.

### Static analysis

- [ ] Create separate big-endian 68000 analysis projects for `w2sys.bin` and `wdv.sys`.
- [ ] Parse and document the 32-byte WDV object header.
- [ ] Confirm WDV vector-table offset, initial stack, entry point, and copy address from the main loader.
- [ ] Confirm the main image base and startup vectors from schematic address decode.
- [ ] Import known strings, schematic net names, and service-diagnostic labels as symbols.
- [ ] Diff all four official main images and classify changed functions/data.

### Hardware and protocol recovery

- [ ] Produce reviewed main-CPU and WDV memory maps with evidence levels.
- [ ] Recover the WDV reset/download/start handshake.
- [ ] Recover the shared-memory/mailbox structures and interrupt protocol.
- [ ] Identify ASIC A/B, waveform-RAM, DAC, timer, and status address ranges.
- [ ] Trace one complete note-on/note-off path from MIDI event to WDV hardware writes.
- [ ] Locate pitch, envelope, LFO, glide, and control-rate transforms on whichever CPU owns them.

### Emulator integration

- [ ] Integrate an instruction-correct 68000 core behind testable memory/I/O callbacks.
- [ ] Run one main CPU and one WDV CPU under a deterministic common scheduler.
- [ ] Timestamp every hardware side effect independently of host audio block size.
- [ ] Reproduce Wave boot, WDV ready, diagnostics, and failure cases before enabling audio.
- [ ] Expand from one WDV board to two and three only after the one-board protocol is stable.
- [ ] Keep the firmware CPU/ASIC bridge explicit; the UI must not report genuine firmware execution while only the procedural model is active.

### Measurement and validation

- [ ] Design a minimal set of hardware-safe logic captures for clock, reset, bus, ASIC select, and DAC timing.
- [ ] Record calibrated single-voice audio fixtures from a real Wave.
- [ ] Compare event traces before spectral/audio comparisons.
- [ ] Test 1.680 and 1.700 with the same polyphonic MIDI sequence to isolate the known allocation difference.
- [ ] Publish an accuracy matrix stating what is confirmed, inferred, approximated, or not yet implemented.

## 8. Suggested milestone definition

The next meaningful milestone should be called **firmware-driven control emulation**, not complete Wave emulation. It is achieved when:

1. authentic `w2sys.bin` and `wdv.sys` execute on emulated 68000 CPUs;
2. the main OS downloads and starts WDV using the recovered hardware protocol;
3. a controlled MIDI note produces a deterministic, timestamped series of WDV ASIC/DAC writes;
4. the trace is stable across host buffer sizes and runs; and
5. at least one part of that trace is confirmed against a real Wave or an independently documented schematic/diagnostic behaviour.

Only after that milestone should the project claim that original firmware is actively driving the sound model. Sample-accurate Wave emulation then becomes a measurable hardware-modelling problem rather than a speculative reimplementation of the instrument's control software.

## References

- [Waldorf Music: Legacy Wave](https://waldorfmusic.com/legacy-wave/)
- [Waldorf-hosted Wave `System.zip` share](https://downloads.waldorfmusic.com/cloud/index.php/s/7zzTnmzK2pYY9Ks)
- [Waldorf Wave Service Manual](https://www.waldorf-wave.de/Waldorf-WAVE-Service-Manual.pdf)
- [Unofficial Wave boot and OS notes](https://unofficial.waldorf-wave.de/waveif.html)
- [Unofficial Wave hardware overview](https://www.waldorf-wave.de/wavetech.html)
- [Wave System Exclusive description](https://unofficial.waldorf-wave.de/os/OS_1-900_WAVE_Sysex_manual.pdf)
- [Wave OS 1.911 manual](https://unofficial.waldorf-wave.de/os/OS_1-911.pdf)
