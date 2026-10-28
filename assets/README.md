# Synthetic KS user flash test fixture

`ks-init-flash.bin` is a reproducible 524288-byte **user flash substitute** created by Silver-K. It is not a dump of a physical instrument. It contains our own simple `Silver Init` saw programs and four-part `Silver Multi` initial performances, plus startup settings.

No OS code, DSP firmware, sampled wave data or factory patches are included. Reserved/OS regions remain erased (`0xff`). The emulator inserts the user's separately supplied OS into its in-memory flash when booting.

This file is retained only for explicit low-level regression tests. It is not embedded, auto-created or distributed as startup Data. Setup rejects this fixture, including renamed copies. Production KS requires user KS All Data, compatible Data MIDI files or a full user Flash BIN.

Release archives also contain a separate `ks-wave-zero.bin`, generated as 129024 zero bytes by `tools/package_release.py`. The zero Wave ROM remains required and is created and selected automatically when no bank was selected. It is independent of required user KS Data and OS firmware. See [English installation instructions](../doc/INSTALL.md).

Regenerate without any ROM or factory MIDI:

```sh
c++ -std=c++17 tools/create_init_flash.cpp -o /tmp/silver-create-init-flash
/tmp/silver-create-init-flash /tmp/ks-init-flash.bin
cmp assets/ks-init-flash.bin /tmp/ks-init-flash.bin
```

The generator uses the control catalog's field layout and numeric defaults, then sets our own sound and part settings. Its only input is the output pathname.
