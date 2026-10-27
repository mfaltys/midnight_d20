# serialupdi

A browser-side serialUPDI programmer, ported to TypeScript by Manuel Kasper as
part of [WebUPDI](https://github.com/manuelkasper/webupdi), which in turn is a
port of Microchip's [pymcuprog](https://github.com/microchip-pic-avr-tools/pymcuprog)
`serialupdi` module. The code in this directory is a verbatim copy of the
`serialupdi/` directory from WebUPDI, with only comment changes.

The ATtiny1614 in Noctis Arcanum uses NVM revision P:0, so the stack drives
`nvmp0.ts` through the 16-bit datalink in `link.ts`.

Licensed under the MIT License, Copyright (c) 2025 Microchip Technology Inc. and
its subsidiaries, and Copyright (c) 2025 Manuel Kasper. The full text is in
[LICENSE](LICENSE) beside this file; keep it with any copy or redistribution of
this directory, as that licence requires.

`devices.ts` and `intel-hex-parser.ts` in the parent directory come from the same
upstream project.
