/**
 * Noctis Arcanum web flasher - UI controller.
 *
 * Talks serialUPDI to the ATtiny1614 through the Web Serial API: bind the
 * adapter, pick a firmware program, and inscribe it. The UPDI stack under
 * serialupdi/ is a TypeScript port of pymcuprog (see serialupdi/README.md).
 *
 * Firmware is served from this site's own origin, so there is no bucket, no
 * CORS rule and no versioning to keep in sync. .github/workflows/pages.yml
 * builds every [env:*] in platformio.ini and stages
 * .pio/build/<env>/firmware.hex as public/firmware/<env>/firmware.hex, plus a
 * manifest listing what was built. The env name is the program name, so adding
 * an environment to platformio.ini makes it appear here on the next deploy.
 */

import { UpdiApplication } from './serialupdi/application.js';
import { parseHexFile } from './intel-hex-parser.js';
import { UPDI_DEVICES, type DeviceInfo } from './devices.js';

const FIRMWARE_ROOT = './firmware';
const FIRMWARE_FILE = 'firmware.hex';
const MANIFEST_FILE = 'manifest.json';
const SERIAL_BAUD = 230400;

/* Fuse byte addresses are the FUSE base plus the fuse number, so BODCFG is the
   second byte and OSCCFG the third (see devices.ts for the base per part). */
const FUSE_BASE_FALLBACK = 0x1280;
const FUSE_BODCFG_OFFSET = 0x01;
const FUSE_OSCCFG_OFFSET = 0x02;

/* platformio.ini declares board_fuses.BODCFG = 0x00: with the brown-out
   detector off the chip sips its CR2032 between rolls. */
const BODCFG_DISABLED = 0x00;

/* megaTinyCore's default OSCCFG for this board (atxy4.bootloader.OSCCFG = 0x01),
   i.e. the 16 MHz internal oscillator the firmware is compiled for. A blank chip
   ships on the other fuse value, and every millis()-derived threshold - the
   600 ms long press, the 300 ms double-click window, the 45 s sleep - would run
   25% fast. */
const OSCCFG_16MHZ = 0x01;

/** What the flasher learned about the chip on the other end of the adapter. */
interface AttachedDevice {
  name: string;
  info: DeviceInfo;
}

interface FirmwareProgram {
  id: string;
  name: string;
  blurb: string;
  /** Bring-up firmware rather than the shipping application. */
  diagnostic?: boolean;
  /** When CI built this hex (ISO 8601, UTC), when the manifest knows. */
  built?: string;
}

/** Manifest written by CI next to the firmware it published. */
interface FirmwareManifest {
  generated?: string;
  commit?: string;
  programs?: string[];
  /**
   * Per-program build times, keyed by program id. Optional: manifests written
   * before this existed only carry `generated`, which stands in for all of them.
   */
  firmware?: Record<string, { built?: string } | undefined>;
}

/**
 * Curated names and descriptions, in the order the picker offers them. Program
 * ids must match the PlatformIO environment names in platformio.ini.
 */
const CATALOG: FirmwareProgram[] = [
  {
    id: 'dice',
    name: 'The Dice Roller',
    blurb:
      'Default dice firmware. Short press cycles d2 through d20, hold to wind up a roll, ' +
      'double-click to sleep.'
  },
  {
    id: 'hwtest',
    name: 'Hardware Test',
    blurb:
      'Hardware bench test for a freshly assembled PCB: walks D1 to D13 one LED at a time, then ' +
      'lights the whole ring while the button is held.',
    diagnostic: true
  }
];

let app: UpdiApplication | null = null;
let port: SerialPort | null = null;
let attached: AttachedDevice | null = null;
let programs: FirmwareProgram[] = [];
let firmware: { program: FirmwareProgram; data: Uint8Array } | null = null;
let busy = false;
/** Commit the archive was built from, for the picker's tooltips. */
let archiveCommit = '';
/* Set once from initializeUI(); updateButtons() keeps the Connect button dead on
   a browser that has no Web Serial, however many times it runs. */
let serialSupported = false;

/* --- Firmware picker --------------------------------------------------------
   A native <select> paints the selected option's text in the closed control as
   well, so there is nowhere to put a build date that shows only while the list
   is dropped. This is a listbox instead: the trigger carries the program name,
   the rows carry the name and the date. Focus never leaves the trigger - the
   active row is tracked with aria-activedescendant, per the combobox pattern. */
let pickerOpen = false;
let pickerActive = -1;
let selectedProgramId = '';

interface PickerParts {
  picker: HTMLElement | null;
  trigger: HTMLButtonElement | null;
  list: HTMLUListElement | null;
  value: HTMLSpanElement | null;
}

function pickerParts(): PickerParts {
  return {
    picker: getElement<HTMLElement>('firmware-picker'),
    trigger: getElement<HTMLButtonElement>('firmware-trigger'),
    list: getElement<HTMLUListElement>('firmware-list'),
    value: getElement<HTMLSpanElement>('firmware-value')
  };
}

/** Rows of the open list, in order. */
function pickerRows(list: HTMLUListElement): HTMLElement[] {
  return Array.from(list.children) as HTMLElement[];
}

function formatHex(value: number, padLength: number = 4): string {
  return `0x${value.toString(16).padStart(padLength, '0').toUpperCase()}`;
}

function getElement<T extends HTMLElement>(id: string): T | null {
  return document.getElementById(id) as T | null;
}

function handleError(error: unknown, fallback: string): string {
  return error instanceof Error ? error.message : fallback;
}

/**
 * Writes a line to the chronicle. `step` is for the coarse milestones of the
 * rite so they stand out from the page-by-page chatter.
 */
export function log(
  message: string,
  type: 'info' | 'success' | 'error' | 'warn' | 'step' = 'info'
): void {
  const logDiv = document.getElementById('log');
  if (!logDiv) return;

  const entry = document.createElement('div');
  entry.className = `log-entry ${type}`;
  entry.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;

  logDiv.appendChild(entry);
  logDiv.scrollTop = logDiv.scrollHeight;
}

function setStatus(state: 'disconnected' | 'connecting' | 'connected'): void {
  const status = document.getElementById('status');
  if (!status) return;

  status.className = `status ${state}`;

  const messages: Record<string, string> = {
    disconnected: 'Status: <strong>Unbound</strong>',
    connecting: 'Status: <strong>Binding&hellip;</strong>',
    connected: 'Status: <strong>Bound</strong>'
  };

  status.innerHTML = messages[state] ?? messages.disconnected;
}

/** Gold bar under the Inscribe button: `fraction` null hides it again. */
function setProgress(fraction: number | null, label: string = ''): void {
  const progress = getElement<HTMLDivElement>('progress');
  const fill = getElement<HTMLDivElement>('progress-fill');
  const labelEl = getElement<HTMLSpanElement>('progress-label');
  if (!progress || !fill || !labelEl) return;

  if (fraction === null) {
    progress.hidden = true;
    fill.style.width = '0';
    labelEl.textContent = 'Idle';
    return;
  }

  progress.hidden = false;
  fill.style.width = `${Math.round(Math.max(0, Math.min(1, fraction)) * 100)}%`;
  labelEl.textContent = label;
}

/**
 * Advances the bar and drops one chronicle line per quarter, so a 70-page write
 * does not bury everything else in the log. `from`/`to` let the write and verify
 * passes share one bar instead of each running 0 to 100%.
 */
function stepProgress(done: number, total: number, label: string, from = 0, to = 1): void {
  const fraction = total === 0 ? to : from + (to - from) * (done / total);
  setProgress(fraction, `${label} ${done}/${total}`);

  const milestone = Math.max(1, Math.floor(total / 4));
  if (done === total || done % milestone === 0) {
    log(`${label} ${done}/${total}`, 'info');
  }
}

function checkConnected(): void {
  if (!app) {
    throw new Error('Not bound to a device');
  }
}

function updateButtons(): void {
  const connectBtn = getElement<HTMLButtonElement>('btn-connect');
  const disconnectBtn = getElement<HTMLButtonElement>('btn-disconnect');
  const trigger = getElement<HTMLButtonElement>('firmware-trigger');
  const flashBtn = getElement<HTMLButtonElement>('btn-flash');

  if (connectBtn) connectBtn.disabled = busy || !serialSupported || app !== null;
  if (disconnectBtn) disconnectBtn.disabled = busy || app === null;
  if (trigger) {
    trigger.disabled = busy || programs.length === 0;
    if (trigger.disabled) closePicker();
  }
  if (flashBtn) flashBtn.disabled = busy || app === null || firmware === null;
}

function prettyName(id: string): string {
  return id
    .split(/[-_]+/)
    .filter(Boolean)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(' ');
}

/**
 * Loads the list of published programs.
 *
 * The manifest is the source of truth for what actually exists in this
 * deployment, so a program CI did not build is never offered. Curated catalogue
 * entries supply the names and descriptions; anything the manifest lists that
 * the catalogue does not know about is appended, prettified from its id, so a
 * new environment is usable before anyone writes copy for it.
 */
async function loadPrograms(): Promise<void> {
  let ids: string[] = [];
  /* Per-program build time when the manifest carries one, otherwise the single
     stamp CI wrote for the whole archive. */
  let builtFor = (_id: string): string | undefined => undefined;

  try {
    const response = await fetch(`${FIRMWARE_ROOT}/${MANIFEST_FILE}`, { cache: 'no-cache' });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status} ${response.statusText}`);
    }

    const manifest = (await response.json()) as FirmwareManifest;
    ids = Array.isArray(manifest.programs) ? manifest.programs.filter(Boolean) : [];

    if (ids.length === 0) {
      throw new Error('the manifest lists no programs');
    }

    archiveCommit = manifest.commit ?? '';
    builtFor = (id) => manifest.firmware?.[id]?.built ?? manifest.generated;

    log(
      `Archive holds ${ids.length} rite(s)${manifest.commit ? `, built from ${manifest.commit}` : ''}`,
      'success'
    );
  } catch (error) {
    // A checkout without CI-staged firmware still gets the catalogue, so the
    // page stays usable for local development.
    log(`Could not read the firmware manifest: ${handleError(error, 'unknown error')}`, 'warn');
    log('Falling back to the built-in catalogue; flashing will 404 if firmware is not staged', 'warn');
    programs = [...CATALOG];
    populateFirmwarePicker();
    return;
  }

  const ordered = CATALOG.filter((program) => ids.includes(program.id)).map((program) => ({
    ...program,
    built: builtFor(program.id)
  }));
  const extra = ids
    .filter((id) => !CATALOG.some((program) => program.id === id))
    .sort()
    .map((id) => ({
      id,
      name: prettyName(id),
      blurb: 'Published by the build; no description on file.',
      built: builtFor(id)
    }));

  programs = [...ordered, ...extra];
  populateFirmwarePicker();
}

/** The name shown in the trigger and in the list, without any build date. */
function programLabel(program: FirmwareProgram): string {
  return program.diagnostic ? `${program.name} (bench test)` : program.name;
}

/** "2026-10-26T17:30:05Z" -> "26 Oct 2026", in UTC so it reads the same anywhere. */
function formatBuilt(iso: string | undefined): string {
  if (!iso) return '';
  const when = new Date(iso);
  if (Number.isNaN(when.getTime())) return '';
  return new Intl.DateTimeFormat('en-GB', {
    day: '2-digit',
    month: 'short',
    year: 'numeric',
    timeZone: 'UTC'
  }).format(when);
}

function populateFirmwarePicker(): void {
  const { list, value } = pickerParts();
  if (!list) return;

  list.innerHTML = '';

  for (const program of programs) {
    const row = document.createElement('li');
    row.className = 'picker-option';
    row.id = `firmware-option-${program.id}`;
    row.dataset.id = program.id;
    row.setAttribute('role', 'option');
    row.setAttribute('aria-selected', 'false');

    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = programLabel(program);
    row.appendChild(name);

    // The date only ever exists inside the open list: the trigger shows the name
    // alone, so the closed control stays clean.
    const when = document.createElement('span');
    when.className = 'when';
    when.textContent = formatBuilt(program.built);
    if (when.textContent) row.appendChild(when);

    if (program.built) {
      row.title = `Built ${program.built}${archiveCommit ? ` from ${archiveCommit}` : ''}`;
    }

    list.appendChild(row);
  }

  // The dice roller is what a device owner wants, so select it up front and the
  // Inscribe button lights up the moment the adapter is bound.
  const preferred = programs.find((program) => program.id === 'dice') ?? programs[0];
  if (preferred) {
    chooseProgram(preferred.id);
  } else if (value) {
    value.textContent = '\u2014 no firmware published \u2014';
  }
}

function setPickerActive(index: number): void {
  const { trigger, list } = pickerParts();
  if (!trigger || !list) return;

  const rows = pickerRows(list);
  if (rows.length === 0) return;

  pickerActive = Math.min(Math.max(index, 0), rows.length - 1);
  rows.forEach((row, i) => row.classList.toggle('active', i === pickerActive));
  trigger.setAttribute('aria-activedescendant', rows[pickerActive].id);
  rows[pickerActive].scrollIntoView({ block: 'nearest' });
}

function openPicker(): void {
  const { trigger, list } = pickerParts();
  if (!trigger || !list || trigger.disabled || pickerOpen) return;

  pickerOpen = true;
  list.hidden = false;
  trigger.setAttribute('aria-expanded', 'true');

  const selected = programs.findIndex((program) => program.id === selectedProgramId);
  setPickerActive(selected < 0 ? 0 : selected);
}

function closePicker(): void {
  const { trigger, list } = pickerParts();
  if (!trigger || !list || !pickerOpen) return;

  pickerOpen = false;
  list.hidden = true;
  trigger.setAttribute('aria-expanded', 'false');
  trigger.removeAttribute('aria-activedescendant');
  pickerRows(list).forEach((row) => row.classList.remove('active'));
}

function chooseProgram(id: string): void {
  const { list, value } = pickerParts();
  const program = programs.find((entry) => entry.id === id);

  selectedProgramId = id;
  if (value) value.textContent = program ? programLabel(program) : id;
  if (list) {
    pickerRows(list).forEach((row) =>
      row.setAttribute('aria-selected', String(row.dataset.id === id))
    );
  }

  closePicker();
  void loadFirmware(id);
}

function setLore(id: string, text: string, tone: '' | 'good' | 'bad' = ''): void {
  const el = getElement<HTMLElement>(id);
  if (!el) return;
  el.textContent = text;
  el.className = tone;
}

interface FuseReadout {
  wdtcfg: number;
  bodcfg: number;
  osccfg: number;
  tcd0cfg: number;
  syscfg0: number;
  syscfg1: number;
}

/**
 * Reads the fuse bytes so the chronicle (and the owner) can see how the chip is
 * actually configured. Call while NVM programming mode is active.
 */
async function readFuses(): Promise<FuseReadout> {
  const base = attached?.info.fuses_address ?? FUSE_BASE_FALLBACK;
  const bytes = await app!.readData(base, 7);

  const fuses: FuseReadout = {
    wdtcfg: bytes[0],
    bodcfg: bytes[1],
    osccfg: bytes[2],
    tcd0cfg: bytes[4],
    syscfg0: bytes[5],
    syscfg1: bytes[6]
  };

  log(
    'Fuses: ' +
      `WDTCFG=${formatHex(fuses.wdtcfg, 2)} ` +
      `BODCFG=${formatHex(fuses.bodcfg, 2)} ` +
      `OSCCFG=${formatHex(fuses.osccfg, 2)} ` +
      `TCD0CFG=${formatHex(fuses.tcd0cfg, 2)} ` +
      `SYSCFG0=${formatHex(fuses.syscfg0, 2)} ` +
      `SYSCFG1=${formatHex(fuses.syscfg1, 2)}`,
    'info'
  );

  const osccfgGood = fuses.osccfg === OSCCFG_16MHZ;
  setLore(
    'lore-osccfg',
    `${formatHex(fuses.osccfg, 2)} - ${osccfgGood ? '16 MHz' : 'not 16 MHz'}`,
    osccfgGood ? 'good' : 'bad'
  );

  const bodGood = fuses.bodcfg === BODCFG_DISABLED;
  setLore(
    'lore-bodcfg',
    `${formatHex(fuses.bodcfg, 2)} - ${bodGood ? 'disabled' : 'enabled'}`,
    bodGood ? 'good' : 'bad'
  );

  const lore = getElement<HTMLDListElement>('device-lore');
  if (lore) lore.hidden = false;

  return fuses;
}

/** Releases the adapter and forgets everything read from the chip. */
async function teardown(): Promise<void> {
  if (app) {
    try {
      await app.destroy();
    } catch {
      // The port is going away anyway; a failed teardown is not actionable.
    }
    app = null;
  }

  if (port) {
    try {
      await port.close();
    } catch {
      // Same: the browser may already have dropped the port.
    }
    port = null;
  }

  attached = null;
}

/**
 * Binds the serial adapter, brings up the UPDI datalink, and identifies the part
 * from its SIGROW device ID.
 */
async function connect(): Promise<void> {
  busy = true;
  updateButtons();
  setStatus('connecting');
  setProgress(null);

  try {
    port = await navigator.serial.requestPort();

    app = new UpdiApplication(port, SERIAL_BAUD);
    await app.init();
    setStatus('connected');
    log('Adapter bound', 'success');

    // Reading the SIB also picks the NVM driver: NVM revision "0" selects the
    // P:0 driver over the 16-bit datalink, which is what tinyAVR 1-series needs.
    try {
      const sib = await app.readDeviceInfo();
      if (sib) {
        log(`SIB: ${sib.family} | NVM ${sib.NVM} | OCD ${sib.OCD} | OSC ${sib.OSC}`, 'info');
      }
    } catch (error) {
      log(`Could not read the SIB: ${handleError(error, 'unknown error')}`, 'warn');
    }

    // The device ID and the fuses both live behind NVM programming mode.
    await app.enterProgmode();

    attached = await identifyDevice();
    log(
      `Device detected: ${attached.name} (ID ${formatHex(attached.info.device_id ?? 0, 6)})`,
      'success'
    );

    setLore('lore-device', attached.name);
    setLore('lore-id', formatHex(attached.info.device_id ?? 0, 6));
    const flashSize = attached.info.flash_size ?? 0;
    setLore('lore-flash', `${flashSize} bytes (${(flashSize / 1024).toFixed(0)} KiB)`);

    await readFuses();
  } catch (error) {
    await teardown();
    setStatus('disconnected');
    throw error;
  } finally {
    busy = false;
    updateButtons();
  }
}

async function disconnect(): Promise<void> {
  busy = true;
  updateButtons();

  try {
    await teardown();
    setStatus('disconnected');
    setProgress(null);
    log('Adapter released', 'info');
  } finally {
    busy = false;
    updateButtons();
  }
}

/** Matches the chip's device ID against the table the UPDI stack trusts. */
async function identifyDevice(): Promise<AttachedDevice> {
  // SIGROW base: identical across tinyAVR 0/1/2-series and megaAVR 0-series.
  const idBytes = await app!.readData(0x1100, 3);
  const deviceId = (idBytes[0] << 16) | (idBytes[1] << 8) | idBytes[2];

  const match = Object.entries(UPDI_DEVICES).find(([, info]) => info.device_id === deviceId);
  if (!match) {
    throw new Error(
      `Unknown device ID ${formatHex(deviceId, 6)} - Noctis Arcanum runs on an ATtiny1614`
    );
  }

  const [name, info] = match;
  // Hand the definition to the UPDI stack: without it the NVM driver has no
  // peripheral base addresses, and erasing or writing fails with a bare
  // "Cannot read properties of undefined (reading 'nvmctrlAddress')".
  app!.setDevice(info);

  return { name, info };
}

/** Writes the chosen program's size and description into the picker blurb. */
function describeFirmware(): void {
  const blurb = getElement<HTMLParagraphElement>('firmware-blurb');
  if (!blurb) return;

  blurb.textContent = '';
  if (!firmware) return;

  blurb.appendChild(document.createTextNode(firmware.program.blurb));

  const meta = document.createElement('span');
  meta.className = 'meta';
  const kib = (firmware.data.length / 1024).toFixed(1);
  meta.textContent =
    `${firmware.program.name} | ${firmware.data.length} bytes (${kib} KiB) | env ${firmware.program.id}`;
  blurb.appendChild(meta);
}

/**
 * Fetches one program's Intel HEX and flattens it into flash bytes.
 *
 * Never rejects: a program that cannot be fetched leaves the Inscribe button
 * disabled and the reason in the chronicle, which reads better than an unhandled
 * rejection behind a click handler.
 */
async function loadFirmware(id: string): Promise<void> {
  const program = programs.find((entry) => entry.id === id);

  if (!program) {
    firmware = null;
    describeFirmware();
    updateButtons();
    return;
  }

  const url = `${FIRMWARE_ROOT}/${id}/${FIRMWARE_FILE}`;

  try {
    const response = await fetch(url, { cache: 'no-cache' });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status} ${response.statusText}`);
    }

    // parseHexFile merges the records into one contiguous image whose address is
    // the lowest record address. AVR flash HEX starts at 0, so the image maps
    // straight onto the device's flash base (0x8000 in the data space).
    const { data, address } = await parseHexFile(await response.text());
    if (data.length === 0) {
      throw new Error('the HEX file carried no data records');
    }

    firmware = { program, data };
    log(`"${program.name}" unrolled: ${data.length} bytes from ${formatHex(address)}`, 'success');
  } catch (error) {
    firmware = null;
    log(`Could not unroll "${program.name}": ${handleError(error, 'unknown error')}`, 'error');
  }

  describeFirmware();
  updateButtons();
}

/**
 * Writes one fuse, skipping it when the chip already holds the wanted value.
 * Returns true when a write was actually issued (which leaves a reset pending).
 */
async function applyFuse(address: number, wanted: number, label: string): Promise<boolean> {
  const before = (await app!.readData(address, 1))[0];

  if (before === wanted) {
    log(`${label} already correct at ${formatHex(before, 2)}, leaving it alone`, 'info');
    return false;
  }

  log(`${label}: ${formatHex(before, 2)} -> ${formatHex(wanted, 2)}`, 'step');
  await app!.writeFuse(address, new Uint8Array([wanted]));

  try {
    const after = (await app!.readData(address, 1))[0];
    if (after === wanted) {
      log(`${label} set, read back as ${formatHex(after, 2)}`, 'success');
    } else {
      log(
        `${label} read back as ${formatHex(after, 2)} rather than ${formatHex(wanted, 2)}`,
        'warn'
      );
    }
  } catch {
    // A fuse write leaves a reset pending, so the chip can reboot under us in
    // the middle of the read-back. That is a completed write, not a failure.
    log(`${label} written; the chip reset before it could be read back`, 'warn');
  }

  return true;
}

/**
 * Brings the fuses in line with platformio.ini.
 *
 * Runs after the flash is written and verified, because a fuse write leaves a
 * reset pending and that reset must not land in the middle of programming.
 *
 * Never fatal: by this point the firmware is in place and verified, so a fuse
 * that refuses to take is worth a warning rather than a failed rite.
 */
async function tuneDevice(): Promise<void> {
  if (!app || !attached) return;

  const fusesBase = attached.info.fuses_address ?? FUSE_BASE_FALLBACK;
  const bodAddress = fusesBase + FUSE_BODCFG_OFFSET;
  const osccfgAddress = fusesBase + FUSE_OSCCFG_OFFSET;

  log('Tuning the device', 'step');

  try {
    await app.enterProgmode();
  } catch (error) {
    log(
      `Could not re-enter programming mode to set the fuses: ${handleError(error, 'unknown error')}`,
      'warn'
    );
    return;
  }

  try {
    await applyFuse(bodAddress, BODCFG_DISABLED, 'Brown-out detector');
  } catch (error) {
    log(`Brown-out fuse write failed: ${handleError(error, 'unknown error')}`, 'warn');
  }

  // The clock fuse goes last: it changes the clock the UPDI link is timed
  // against, so nothing after this should depend on the link still answering.
  try {
    await applyFuse(osccfgAddress, OSCCFG_16MHZ, 'Clock (16 MHz)');
  } catch (error) {
    log(`Clock fuse write failed: ${handleError(error, 'unknown error')}`, 'warn');
  }
}

/**
 * Best-effort nudge to get the chip running the new firmware. A fuse write
 * already leaves a reset pending; when the fuses were correct to begin with this
 * asks for a reset over UPDI instead. Neither is guaranteed to lift the chip out
 * of NVM programming mode, which is why the closing message tells the owner to
 * power-cycle if the ring stays dark.
 */
async function releaseDevice(): Promise<void> {
  try {
    await app!.reset(true);
    await app!.reset(false);
    log('Reset requested', 'info');
  } catch (error) {
    log(`Could not request a reset: ${handleError(error, 'unknown error')}`, 'warn');
  }
}

/**
 * Erases the chip, writes the firmware, reads every page back, then tunes the
 * fuses. Throws on anything that would leave the device in an unknown state, so
 * a failure is never reported as a success.
 */
async function flashDevice(): Promise<void> {
  checkConnected();

  if (!firmware) {
    throw new Error('No rite chosen');
  }
  if (!attached) {
    throw new Error('No device identified');
  }

  const pageSize = attached.info.flash_page_size ?? 0x40;
  // Flash is mapped into the data space at 0x8000 (MAPPED_PROGMEM_START), not at
  // 0x4000 (which is flash_size). Writing the page buffer to the wrong address
  // makes the NVM controller raise WRERROR, which surfaces as a bare "NVM error".
  const startAddress = attached.info.flash_address ?? 0x8000;
  const flashSize = attached.info.flash_size ?? 0x4000;
  const data = firmware.data;

  if (data.length > flashSize) {
    throw new Error(
      `The image is ${data.length} bytes but ${attached.name} only has ${flashSize}`
    );
  }

  log(`Inscribing "${firmware.program.name}" onto ${attached.name}`, 'step');

  await app!.enterProgmode();
  await app!.chipErase();
  // The erase drops the chip out of programming mode, so go back in.
  await app!.enterProgmode();
  log('Chip erased', 'info');

  const totalPages = Math.ceil(data.length / pageSize);

  for (let page = 0; page < totalPages; page++) {
    const offset = page * pageSize;
    const chunk = data.slice(offset, offset + pageSize);

    let write = chunk;
    if (chunk.length < pageSize) {
      // The NVM controller writes whole pages, so pad the tail with the erased
      // value and let the last page go out in one piece.
      write = new Uint8Array(pageSize).fill(0xFF);
      write.set(chunk);
    }

    await app!.writeFlash(startAddress + offset, write);
    stepProgress(page + 1, totalPages, 'Writing flash', 0, 0.5);
  }

  for (let page = 0; page < totalPages; page++) {
    const offset = page * pageSize;

    const expected = new Uint8Array(pageSize).fill(0xFF);
    expected.set(data.slice(offset, offset + pageSize));

    const actual = await app!.readData(startAddress + offset, pageSize);
    for (let i = 0; i < pageSize; i++) {
      if (actual[i] !== expected[i]) {
        throw new Error(
          `Verification failed at ${formatHex(startAddress + offset + i)}: ` +
            `expected ${formatHex(expected[i], 2)}, read ${formatHex(actual[i], 2)}`
        );
      }
    }

    stepProgress(page + 1, totalPages, 'Verifying', 0.5, 1);
  }

  setProgress(null);
  log(`"${firmware.program.name}" written and verified`, 'success');

  await tuneDevice();
  await releaseDevice();

  log(
    'Rite complete. If the ring stays dark, unplug the adapter (or reseat the coin cell) to restart the chip',
    'step'
  );
}

/** Wires up the page: support check, listeners, then the firmware archive. */
export function initializeUI(): void {
  const connectBtn = getElement<HTMLButtonElement>('btn-connect');
  const disconnectBtn = getElement<HTMLButtonElement>('btn-disconnect');
  const flashBtn = getElement<HTMLButtonElement>('btn-flash');
  const { picker, trigger, list } = pickerParts();
  const clearBtn = getElement<HTMLButtonElement>('btn-clear-log');
  const hint = getElement<HTMLParagraphElement>('serial-hint');

  serialSupported = 'serial' in navigator;

  if (hint) {
    if (serialSupported) {
      hint.textContent = 'Plug a serialUPDI adapter in, then bind it to this page.';
      hint.className = 'step-hint';
    } else {
      hint.textContent = 'This browser has no Web Serial. Use Chrome, Edge or Opera on a desktop.';
      hint.className = 'step-hint error';
    }
  }

  if (connectBtn) {
    connectBtn.addEventListener('click', async () => {
      try {
        await connect();
      } catch (error) {
        // Dismissing the browser's port picker is a choice, not a failure.
        if (error instanceof DOMException && error.name === 'NotFoundError') {
          log('No adapter chosen', 'info');
        } else {
          log(`Binding failed: ${handleError(error, 'unknown error')}`, 'error');
        }
        setStatus('disconnected');
        updateButtons();
      }
    });
  }

  if (disconnectBtn) {
    disconnectBtn.addEventListener('click', async () => {
      try {
        await disconnect();
      } catch (error) {
        log(`Could not release the adapter: ${handleError(error, 'unknown error')}`, 'error');
      }
    });
  }

  if (trigger) {
    trigger.addEventListener('click', () => {
      if (pickerOpen) closePicker();
      else openPicker();
    });

    // The trigger owns the keyboard: Escape/Tab close, arrows open and move the
    // active row, Enter/Space commit. preventDefault also stops the button from
    // synthesising a click of its own on Space/Enter, which would re-toggle.
    trigger.addEventListener('keydown', (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        if (pickerOpen) {
          event.preventDefault();
          closePicker();
        }
        return;
      }
      if (event.key === 'Tab') {
        closePicker();
        return;
      }
      if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault();
        if (!pickerOpen) {
          openPicker();
        } else {
          setPickerActive(pickerActive + (event.key === 'ArrowDown' ? 1 : -1));
        }
        return;
      }
      if (event.key === 'Home' || event.key === 'End') {
        if (pickerOpen) {
          event.preventDefault();
          setPickerActive(event.key === 'Home' ? 0 : programs.length - 1);
        }
        return;
      }
      if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        if (!pickerOpen) {
          openPicker();
        } else {
          const id = programs[pickerActive]?.id;
          if (id) chooseProgram(id);
        }
      }
    });
  }

  if (list) {
    // Rows must not take focus from the trigger, so the mousedown that would
    // move it is suppressed and the click is handled instead.
    list.addEventListener('mousedown', (event) => event.preventDefault());
    list.addEventListener('click', (event) => {
      const row = (event.target as HTMLElement).closest<HTMLElement>('.picker-option');
      const id = row?.dataset.id;
      if (id) chooseProgram(id);
    });
  }

  // A click anywhere else closes the list, the way a native select behaves.
  document.addEventListener('mousedown', (event) => {
    if (pickerOpen && picker && !picker.contains(event.target as Node)) {
      closePicker();
    }
  });

  if (flashBtn) {
    flashBtn.addEventListener('click', async () => {
      busy = true;
      updateButtons();

      try {
        await flashDevice();
      } catch (error) {
        setProgress(null);
        log(`Inscription failed: ${handleError(error, 'unknown error')}`, 'error');
      } finally {
        busy = false;
        updateButtons();
      }
    });
  }

  if (clearBtn) {
    clearBtn.addEventListener('click', () => {
      const logDiv = document.getElementById('log');
      if (logDiv) logDiv.innerHTML = '';
    });
  }

  updateButtons();
  setStatus('disconnected');
  log('Noctis Arcanum flasher ready', 'step');

  // Fire and forget: the picker fills in as soon as the archive answers.
  void loadPrograms();
}


