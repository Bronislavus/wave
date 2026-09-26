# Public source release

The private development repository contains historical ROM and factory-bank
blobs. Removing files in a later commit does not remove those historical blobs.
Do not push this development repository or any of its branches/tags publicly.

Create a separate source snapshot with no inherited Git history:

```sh
python3 scripts/export-public-source.py public-release/wave
cd public-release/wave
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
git init -b main
git add .
git commit -m "Initial public source release"
```

The exporter includes current working-tree edits and approved source directories.
It excludes the old logo artwork, factory-table payload header, firmware,
Wave sound libraries, ROM archives, disk images, installers, build directories,
and all existing Git history. It refuses to overwrite a destination.
Review new source files before exporting; a file allowlist is not a substitute
for reviewing newly added data or secrets.

Public builds default to `WAVE_EMBED_PRIVATE_ASSETS=OFF` and
`WAVE_SIGN_RELEASE_ARTIFACTS=OFF`, even if private assets are present locally.
The approved decoded PPG sample bank in `data/` is included.
The upper Wave factory tables use the procedural fallback unless explicitly enabled
in a private development build. Users may import their own wavetable data.

Publish only the new snapshot repository after reviewing its files. No remote
is configured by the exporter. Existing private installers and DMGs may embed
private data; do not attach those binaries to the public source release.
Rebuild distributable binaries from the public snapshot, retain dependency
notices, and provide the corresponding source under the applicable licenses.

The packaging script takes release account configuration from environment
variables. Set `TEAM_ID`, `PACE_ACCOUNT`, `PACE_WCGUID`, and your notarization
profile for signed AAX releases. A local packaging check can use:

```sh
SKIP_WRAP=1 SKIP_SIGN=1 SKIP_NOTARIZE=1 SKIP_DMG=1 ./Installer/build_installer.sh
```

Project-authored source and artwork use GPL-3.0-or-later. JUCE and its bundled
SDKs retain their own licenses; see THIRD_PARTY_NOTICES.md and LICENSES/JUCE.md.
The repository includes no permission to redistribute third-party instrument data.
