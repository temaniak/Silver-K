# SoulTrain experimental interface

Approved K-Station and KS interface by SoulTrain, with native Silver-K parameters and patch library. The macOS experimental build embeds the local React interface in WebKit. It requires no running web server or internet connection. The original interface is available through Setup; select SoulTrain experimental in its skin menu to return.

The Browser uses the existing patch database, model/source/tag filters, KS program, Performance and Drum Map banks, favourites, source management and SysEx save/export. Stars edit the same favourites as the original interface. Removing a source only removes its indexed library data; it does not delete the source file. Performance uses the Browser's current program filters.

Build the frontend with `npm ci` and `npm run build:native`. Configure the regular desktop CMake project with `-DSILVER_SOULTRAIN_PREVIEW=ON`, then build `silverK_Standalone`, `silverK_AU` and `silverK_VST3`. CMake copies the compiled resources into each bundle before signing. The option is off by default and currently requires macOS. AU/VST3 identifiers are unchanged, so this is an alternative test build of Silver-K rather than a second plugin product.

Run `silverSoulTrain_test` in a macOS graphical session, passing the compiled `dist/client` directory. Optional arguments are local K and KS firmware, KS test flash and Wave ROM. The test checks the actual WebKit bridge, browser data and all model/part parameter routes. Firmware and banks are not distributed with this interface.

Inter is distributed under the included OFL. Pixelmix and artwork are the supplied author assets. Skin credit remains visible in the interface. The design workspace also contains synthetic presets for the standalone HTML preview; the native plugin always uses the real library.
