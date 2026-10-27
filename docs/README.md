# docs/ — the Noctis Arcanum web flasher

The page published at **https://mfaltys.github.io/midnight_d20/**. It talks
serialUPDI to the ATtiny1614 through the Web Serial API, so firmware can be
flashed from a browser with nothing installed.

## Layout

| Path | What it is |
|---|---|
| `index.html`, `styles.css`, `noctis-ui.ts` | the page: markup, theme, controller |
| `devices.ts`, `intel-hex-parser.ts`, `serialupdi/` | the UPDI stack, ported from WebUPDI (see `serialupdi/README.md` and `serialupdi/LICENSE`) |
| `public/board.svg` | the hero glyph: a technical drawing of the panel, gold on transparent |
| `public/firmware/<env>/firmware.hex` | staged by CI, gitignored, copied into the build by Vite |
| `dist/` | Vite output, gitignored — this is what gets deployed |

## Local development

```sh
cd docs
npm install
npm run dev      # http://localhost:3000
npm run build    # -> docs/dist
```

`public/firmware/` is empty in a fresh checkout, so the picker falls back to the
built-in catalogue and says so in the log. To exercise the real path, stage a
build by hand:

```sh
mkdir -p docs/public/firmware/dice
cp .pio/build/dice/firmware.hex docs/public/firmware/dice/
```

## Deployment

`.github/workflows/pages.yml` builds every `[env:*]`, stages each
`.pio/build/<env>/firmware.hex` as `public/firmware/<env>/firmware.hex`, writes a
`manifest.json` listing what it published and when each program was built, runs
`npm run build`, and deploys `docs/dist` with `actions/deploy-pages`.

Firmware is served from the site's own origin, so there is no bucket and no CORS
rule to maintain. `noctis-ui.ts` reads `manifest.json` to decide what to offer, so
a new `[env:*]` in `platformio.ini` shows up in the picker on the next deploy.

Two settings have to match that: the repository must be public, and **Settings →
Pages → Source** must be **GitHub Actions**. Do not point Pages at `main /docs` —
that is the TypeScript source, not the built site.

## Why the flasher writes fuses

Besides the flash image, the page sets two fuses to match `platformio.ini`:

- **OSCCFG = 0x01** (16 MHz). A blank chip ships on the other setting, and every
  `millis()`-derived threshold — the 400 ms long press, the 300 ms double-click
  window, the 45 s sleep — would run 25% fast. 0x01 is megaTinyCore's own default
  for this board (`atxy4.bootloader.OSCCFG`) and what `platformio.ini` declares,
  so it matches what `pio run -t fuses` writes.
- **BODCFG = 0x00** (brown-out detector off), matching `board_fuses.BODCFG` in
  `platformio.ini`, to keep the CR2032 sleep current down.

The fuse step runs last and is never fatal: writing a fuse leaves a reset pending,
and by then the flash has already been written and read back.
