# Silver-K 0.4.4a

K-Station and KS firmware emulation for desktop hosts and Standalone use.

Based on the open-source code of [Gearmulator](https://github.com/dsp56300/gearmulator). Plugins appear under NK Stuff in host lists.

| Platform | Formats |
| --- | --- |
| Windows x64 | VST3, Standalone |
| macOS Apple Silicon and Intel (Universal 2) | AU, VST3, Standalone |

- [Releases](https://github.com/temaniak/Silver-K/releases)
- [Installation](doc/INSTALL.md)
- [User guide](doc/USER_GUIDE.md)
- [Wave ROM Builder](doc/WAVE_ROM_BUILDER.md)

## Setup

1. Choose a permanent data folder in Setup.
2. For KS, select a MIDI file containing the complete program and performance data, or select several compatible MIDI files together. The required zero Wave ROM is created automatically.
3. Load a compatible firmware BIN.
4. Add further patch libraries on Patches.

Firmware and original sound data are not included. KS requires firmware, Wave ROM and program data to start. K-Station requires its own firmware.

## Data folders

| Folder | Contents |
| --- | --- |
| ROMs | Required firmware, Wave ROM and KS program data |
| Patches | Additional banks and presets |
| Skins | Custom skins |

Setup remembers the selected root. Required files selected elsewhere are copied into ROMs; originals are retained.

## Development

Alpha. Performance and latency remain under development. KS external audio input is not implemented.

- [Build instructions](doc/BUILD.md)
- [Custom skins](doc/SKINS.md)
- [Licenses and provenance](THIRD_PARTY.md)

Licensed under [GPLv3](LICENSE.md).
