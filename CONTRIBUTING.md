# Contributing

Use a C++20 compiler and CMake 3.25 or newer. Follow the build instructions in
README.md and run `ctest --test-dir build --output-on-failure` before submitting
changes. The public test suite runs without firmware; tests requiring local
instrument data skip when that data is unavailable.

Keep DSP changes deterministic and allocation-free on the audio thread.
Describe the audible or UI behavior being changed and how you verified it.
Separate documented hardware behavior from hypotheses and measurements.

Contributions are submitted under GPL-3.0-or-later. Do not submit firmware,
factory banks, ROM dumps, disk images, private recordings, signing credentials,
or generated installers. Use synthetic fixtures for public tests.
