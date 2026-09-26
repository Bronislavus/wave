# Waldorf Wave reference-capture protocol

This directory intentionally contains no copyrighted factory data or hardware recordings.
It defines the repeatable measurements used to calibrate the behavioural oscillator-chip
and analogue models.

Record at 48 kHz / 24 bit with all effects bypassed, direct from the Wave main outputs,
and leave at least 500 ms of pre-roll and post-roll. Record the same program twice so
deterministic behaviour can be separated from analogue noise and tolerance.

The canonical first capture is a single MIDI note C4 (note 60), velocity 102, held for
500 ms. Use a static single wave, zero modulation, centred pan, open filter, zero
resonance, minimum attack/release, and record the MIDI event stream alongside audio.
Further captures should sweep one variable at a time: note, wavetable position, scan,
cutoff, resonance, VCA level, pan, and voice number.

Run the tests with `WAVE_REFERENCE_CAPTURE=/absolute/path/to/capture.wav`. The harness
reports alignment lag, fitted gain, normalized correlation, RMS error, and peak error.
Metrics are reported rather than silently retuning the model; acceptance thresholds can
only be fixed after a matching real-Wave capture set is supplied.
