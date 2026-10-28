# Skin packages

Silver-K loads JUCE skin packages without rebuilding the plugin. A package changes the editor's layout, palette, fonts and artwork. Parameter ranges, MIDI mappings, automation and sound processing come from Silver-K.

## Using a skin

1. Open Setup → Skins folder.
2. Put each package in its own subfolder of Skins. The entry file must be named `skin.json`.
3. Click Reload to refresh the list, then select the package in the skin selector.

Load skin also opens a package's `skin.json` directly, including outside Skins. Existing standalone Silver-K layout JSONs are supported by the same chooser. The selected package is remembered in preferences and host state. Keep its files available when reopening a session.

Reload reads JSON, images and the font again from disk. F5 invokes the same action from the editor. A rejected reload keeps the working editor; Setup displays the error. A missing or invalid package at startup uses the built-in skin and displays the error. If a managed package has moved with the data folder, Silver-K can locate it by its manifest ID.

The repository includes **Silver Flat** in `assets/skins/Silver Flat`. Copy that folder into Skins. It provides a flat blue/silver interface with three oscillators side by side, separate envelope strips and persistent editable dial values. Its vector controls scale without bitmap textures. Select Silver-K to return to the original interface.

## Creating a package

Setup → Export template creates a new `Silver-K Template` folder inside Skins. Repeated exports create separate folders. Export includes the complete default layout, palette and the five original PNG resources. Select the exported package and edit its files, then click Reload or press F5.

```text
Skins/
  My Skin/
    skin.json
    layout.json
    theme.json
    assets/
      knobBody.png
      knobGrip.png
      knobBase.png
      buttonOff.png
      buttonOn.png
```

The manifest is:

```json
{
  "schemaVersion": 1,
  "engine": "silver-k-juce",
  "id": "my-skin",
  "name": "My Skin",
  "author": "Your name",
  "layout": "layout.json",
  "theme": "theme.json"
}
```

Give each independent skin a distinct ID. It may contain letters, digits, dots, underscores and hyphens. Resource paths are relative to the package folder. Use forward slashes and ordinary files inside the folder; absolute paths, parent traversal and resource symlinks are rejected.

Layout and theme objects inherit built-in defaults. Nested objects merge; an array replaces the corresponding array in full. The exported files provide a starting point with all required fields present. A format version other than 1 is rejected.

## Layout

Coordinates use the skin's unscaled design pixels. GUI scaling and host DPI are applied afterward. Set `width` and `height` independently between 320 and 4096; the editor retains the resulting aspect ratio.

The exported layout includes:

| Field | Purpose |
| --- | --- |
| `name`, `header`, `title` | Panel title and header rectangles |
| `sections`, `studioRegions` | Panel geometry and Extra popup regions |
| `studioModels.K`, `studioModels.KS` | Model-specific Main controls |
| `studioHeader` | Firmware, parts, page buttons and Performance switch |
| `pwmSelectors`, `lfoSelectors`, `fmButton` | Main selector positions |
| `lcd`, `actions` | Program display, previous/next, browser and panic |
| `effects`, `effectSlots`, `effectMenuArea`, `effectSelect` | Effect controls, labels and positions |
| `advanced` | Extra page grid and viewport |
| `components` | Overrides for individual components throughout the editor |
| `performanceCanvas` | Internal Performance design size, default `[1254, 904]` |
| `drawPanels`, `decorations` | Native panel drawing and custom artwork/text |

Keep `layout: "studio"` for the current interface. `legacy` retains the previous Main layout. In each model's control array, an item binds an existing parameter:

```json
{
  "id": "cutoff",
  "label": "Cutoff",
  "kind": "panelKnob",
  "rect": [720, 150, 100, 110],
  "dark": false
}
```

Use the exported parameter IDs. `{lfo}` binds the selected LFO; `osc1_{pw}`, `osc2_{pw}` and `osc3_{pw}` follow the corresponding PWM selectors. A control's type can be `panelKnob`, `menu`, `inline`, `fader`, `lamps`, `waveButtons`, `led`, `rhythm`, `classic`, `miniKnob`, `row`, `extraRow`, `mixer`, `compact` or `knob`. Menu parameters still use their declared enum values.

`components` can override a component's bounds after its native layout runs. Coordinates are relative to its immediate parent. Match a component ID globally, or use its full ID path to distinguish Main from Extra:

```json
"components": {
  "mainPanel/control:cutoff": { "bounds": [720, 150, 100, 110] },
  "patchSearch": { "bounds": [20, 10, 300, 28] },
  "performance:part1:strip": { "bounds": [10, 66, 300, 838] }
}
```

Containers without an ID are omitted from the matching path. The `surface` ID addresses the editor background; it is omitted from descendant paths. Component overrides do not force visibility or enabled state: firmware availability, model and current page still control those states.

Useful IDs:

| Area | IDs |
| --- | --- |
| Navigation | `pageMain`, `pageExtra`, `pagePerf`, `pagePatches`, `pageSetup`, `firmwareSelect`, `partTab1`–`partTab4`, `performanceSwitch` |
| Main | `mainPanel`, `control:<parameter ID>`, `caption:<parameter ID>`, `<parameter ID>` for the inner slider/menu, `programDisplay`, `patchBrowserButton`, `previousPreset`, `nextPreset`, `panic`, `effectSelect` |
| Extra | `extra:osc1`–`extra:osc3`, `extra:mixer`, `extra:filter`, `extra:envelopes`, `extra:lfo`, `extra:voice`, `extra:arp`, `extra:fx`, `fmSettings`, `localExtra`, `extraContent`, `extraBack`, `programClock`, `clockSource`, `clockSourceLabel`, `advancedContent`, `advancedPanel:<section name>` |
| Setup | `setupStatus`, `setupLatency`, `setupHelp`, `loadFirmware`, `dataFolder`, `rescanRoms`, `waveRom`, `factoryData`, `skinSelect`, `loadSkin`, `reloadSkin`, `exportSkin`, `openSkins`, `controlChannel`, `audioBuffer`, `ksController`, `performanceDiagnostics`, `demoSong` |
| Browser | `patchBrowser`, `patchSearch`, `patchModel`, `patchTagFilter`, `patchSource`, `patchFavouritesOnly`, `showAllBankSlots`, `ksBank`, `patchBankHint`, `patchTable`, `addPatchFolder`, `addPatchFile`, `rescanPatches`, `removePatchSource`, `saveCurrentPatch`, `patchTags`, `patchFavourite`, `savePatchTags`, `exportPatch`, `patchInfo` |
| Performance | `performancePanel`, `performanceCanvas`, `performancePageSwitch`, `performanceMessage`, `performance:part1:strip`–`performance:part4:strip` |

Each Performance strip also exposes `performance:partN:active`, `:title`, `:controls`, `:preset`, `:previous`, `:next`, `:edit`, `:part_channel`, `:part_output`, `:part_low`, `:part_high`, `:part_transpose`, `:part_fine`, `:part_velocity` and `:volume`. Parameter children append `:caption`, `:slider`, `:value`, `:menu` or `:open` as applicable. Their coordinates are local to the corresponding parameter component.

## Theme and resources

The theme has `palette`, `font`, `images`, `components` and `juceColours` objects. Colours use `#RRGGBB` or `#AARRGGBB`.

Optional `renderStyle: "flat"` selects solid panels, vector dials, rectangular buttons and faders, and persistent editable values on Main dials. `"classic"` is the default and retains the original artwork. Explicit filmstrips and component button artwork still take precedence. The rendering style changes only the interface.

```json
{
  "palette": {
    "text": "#e8e8e8",
    "accent": "#ed8b32",
    "background": "#202428",
    "panelTop": "#30363c",
    "panelBottom": "#252b31"
  },
  "font": { "file": "fonts/Interface.ttf", "family": "", "scale": 1.0 },
  "images": { "knobBase": "assets/knob.png" },
  "components": {},
  "juceColours": {}
}
```

Other named palette entries are `lightText`, `dark`, `border` and `display`. Remaining entries in the export are named by their original opaque ARGB value, such as `ff148de3`. Editing them changes the corresponding gradients, LEDs, outlines and other painted details. Source opacity is retained. These entries also apply to custom Performance and browser drawing.

Leave `font.file` and `font.family` empty for platform fonts. A relative TTF/OTF file loads a custom typeface; `family` selects an installed font when no file is supplied. `scale` is between 0.5 and 2. Existing monospaced displays remain monospaced when no custom font is chosen.

`images` maps resource names to PNG/JPEG/GIF files. Built-in resource names are `knobBody`, `knobGrip`, `knobBase`, `volumeBase`, `buttonOff`, `buttonOn`, `faderTrack` and `faderThumb`. Omitted resources use the built-in artwork. Omitting `volumeBase` retains the generated blue Volume cap. Referenced external files must exist and decode successfully.

To use a filmstrip for rotary knobs, add an image resource and this optional theme object:

```json
"knobStrip": {
  "image": "myKnobStrip",
  "frames": 128,
  "orientation": "vertical"
}
```

Frames must be equal-sized, with 2–1024 frames arranged vertically or horizontally. The first frame represents the minimum and the last the maximum displayed position. This overrides rotary artwork; faders use their slider renderer, with optional external `faderTrack` and `faderThumb` images. Track artwork fills the slider area; thumb artwork uses its authored pixel size, limited to that area.

Theme `components` uses the same ID/path matching as layout overrides:

```json
"components": {
  "surface": { "backgroundImage": "myBackground" },
  "caption:cutoff": { "fontSize": 14, "align": "centre", "palette": { "text": "#ffdd88" } },
  "pageSetup": { "buttonOff": "myButtonOff", "buttonOn": "myButtonOn" },
  "cutoff": { "rotaryDiameter": 90 }
}
```

Available style properties are `fontSize`, label `align` (`left`, `centre`, `right`), `palette`, `backgroundImage`, `hideDecoration`, `knobStrip`, `buttonOff`, `buttonOn`, `faderTrack`, `faderThumb`, `rotaryStyle`, `rotaryDiameter`, `standKnob`, `blueVolume` and `captionWidth`. A component `knobStrip` names another image using the global filmstrip frame count and orientation.

Background images and `hideDecoration` apply to the surface, Extra panels and custom Performance panels. For transparent native containers, attach an image decoration instead. Per-component palettes affect the component's own drawing; style its labels/controls separately to change their appearance.

`juceColours` optionally overrides standard JUCE colours directly. Supported names are `menuBackground`, `menuText`, `menuHighlight`, `menuHighlightText`, `editorBackground`, `editorText`, `editorOutline`, `editorFocus`, `editorSelection`, `editorSelectionText`, `tableBackground`, `tableHeaderBackground`, `tableHeaderText`, `tableHeaderOutline`, `tableHeaderHighlight`, `scrollbarThumb`, `scrollbarBackground`, `sliderTrack`, `sliderThumb`, `sliderBackground`, `tooltipBackground`, `tooltipText`, `tooltipOutline`, `toggleText`, `toggleTick`, `toggleDisabledTick` and `comboText`.

## Replacing panel artwork

Set layout `drawPanels` to `false` to suppress native decorative panels and painted headings. Functional controls retain their bindings. Use a background image and `decorations` to provide the panel design:

```json
"decorations": [
  {
    "id": "filter-heading",
    "parent": "mainPanel",
    "kind": "text",
    "rect": [720, 100, 240, 30],
    "text": "FILTER",
    "colour": "#eeeeee",
    "fontSize": 18,
    "bold": true
  }
]
```

Decorations may be `text`, `image` (with an `image` resource name) or `panel` (with `colour` and optional `radius`). Text supports `align`. They are drawn behind controls and do not intercept input. An optional `page` limits a decoration to `Main`, `Extra`, `Perf`, `Patches` or `Setup`.

Valid parents are the surface (empty or `surface`), `mainPanel`, `patchBrowser`, `performancePanel`, `performanceCanvas`, `localExtra`, `advancedContent`, and each Performance strip or its `:controls` container. IDs within `decorations` must be unique. Array order is front to back.

## Checking a skin

Check both K and KS layouts, Main, every effect, Program Options, the browser, Setup and Performance. Try several GUI scales. Reload after changing JSON, images and fonts. Confirm controls still fit their parent components and all desired controls remain reachable.

The loader rejects malformed JSON, invalid required geometry, unknown parameter IDs, duplicate control IDs within a control array, invalid colours, missing assets and unsupported format versions before switching skins. A package can deliberately omit controls or place them outside the visible canvas, so visual and functional review is still necessary.
