# Installation

## macOS

Universal 2 builds include Apple Silicon and Intel code for Standalone, AU, VST3 and Wave ROM Builder. Use a 64-bit host. The build target is macOS 12.0 or later; runtime validation on older macOS versions and physical Intel Macs is still required. Older arm64-only archives require a native Apple Silicon host.

1. Extract the archive and close the DAW.
2. Copy the complete AU component to ~/Library/Audio/Plug-Ins/Components/.
3. For VST3, copy the complete bundle to ~/Library/Audio/Plug-Ins/VST3/.
4. Place the Standalone application in Applications or another permanent folder.
5. Restart the DAW and rescan plugins. Load Silver-K on an instrument track.

Keep only one AU copy across the user and system Components folders. Replace the whole bundle when updating.

Mac builds are ad hoc signed and not notarized. If macOS blocks a verified build, use its per-application Privacy & Security approval.

## Windows

Use a 64-bit host.

1. Extract the archive to a permanent folder and close the DAW.
2. Copy the complete VST3 bundle to C:\Program Files\Common Files\VST3\.
3. Run Standalone from the extracted folder.
4. Restart the DAW and rescan plugins.

A missing Visual C++ runtime must be installed separately. AU is available only on macOS.

## Required data

Select a compatible firmware BIN in Setup. KS also requires Wave ROM and a MIDI file containing the complete program and performance data, or several compatible MIDI files selected together. The required zero Wave ROM is created automatically.

Required assets belong in ROMs. Additional patch libraries belong in Patches; custom skins belong in Skins. Firmware and original sound data are not included.

[First startup and plugin operation](USER_GUIDE.md)
