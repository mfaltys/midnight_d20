// Auto-generated from device definitions.
//
// WARNING: this table is what the UPDI stack trusts for memory layout, so wrong
// values here cause confusing runtime failures rather than build errors.
//
// Verified against pymcuprog (generated from the Microchip.ATtiny_DFP device pack)
// and the AVR device headers. Note in particular:
//   flash_address = 0x8000 (MAPPED_PROGMEM_START) - NOT flash_size (0x4000).
// Writing the page buffer to the wrong address makes the NVM controller raise
// WRERROR, which surfaces as a bare "NVM error" from waitNvmReady().
//
// Peripheral bases come from P0_PERIPHERALS below.
// Device info parameters for UPDI-enabled devices

export interface DeviceInfo {
  eeprom_address?: number;
  eeprom_size?: number;
  eeprom_page_size?: number;
  eeprom_read_size?: number;
  eeprom_write_size?: number;
  flash_address?: number;
  flash_size?: number;
  flash_page_size?: number;
  flash_read_size?: number;
  flash_write_size?: number;
  fuses_address?: number;
  fuses_size?: number;
  user_row_address?: number;
  user_row_size?: number;
  user_row_page_size?: number;
  user_row_read_size?: number;
  user_row_write_size?: number;
  sigrowAddress?: number;
  syscfgAddress?: number;
  nvmctrlAddress?: number;
  device_id?: number;
}

/**
 * Peripheral base addresses (data space) for NVM revision P:0.
 *
 * These are identical across the tinyAVR 0/1/2-series and megaAVR 0-series parts,
 * and are verified against the AVR device headers - see iotn1614.h:
 *   #define SYSCFG   (*(SYSCFG_t *) 0x0F00)
 *   #define NVMCTRL  (*(NVMCTRL_t *) 0x1000)
 *   #define SIGROW   (*(SIGROW_t *) 0x1100)
 *
 * The UPDI NVM driver uses nvmctrlAddress to reach the erase/write control
 * registers, so a device entry without it fails with
 * "Cannot read properties of undefined (reading 'nvmctrlAddress')".
 */
const P0_PERIPHERALS = {
  sigrowAddress: 0x1100,
  syscfgAddress: 0x0f00,
  nvmctrlAddress: 0x1000,
} as const;

export const UPDI_DEVICES: Record<string, DeviceInfo> = {
  // tinyAVR 0/1/2 Series - NVM version P:0
  'attiny1614': {
    ...P0_PERIPHERALS,
    device_id: 0x1E9422,
    eeprom_address: 0x00001400,
    eeprom_size: 0x0100,
    eeprom_page_size: 0x20,
    eeprom_read_size: 0x01,
    eeprom_write_size: 0x01,
    flash_address: 0x00008000, // MAPPED_PROGMEM_START - flash is mapped into the data space at 0x8000, not 0x4000 (which is flash_size)
    flash_size: 0x4000,
    flash_page_size: 0x40,
    flash_read_size: 0x02,
    flash_write_size: 0x40,
    fuses_address: 0x1280,
    user_row_address: 0x1300,
    user_row_size: 0x20,
    user_row_page_size: 0x01,
    user_row_read_size: 0x01,
    user_row_write_size: 0x01,
  },
  'attiny1616': {
    ...P0_PERIPHERALS,
    device_id: 0x1E9424,
    eeprom_address: 0x00001400,
    eeprom_size: 0x0100,
    eeprom_page_size: 0x20,
    eeprom_read_size: 0x01,
    eeprom_write_size: 0x01,
    flash_address: 0x00008000, // MAPPED_PROGMEM_START - flash is mapped into the data space at 0x8000, not 0x4000 (which is flash_size)
    flash_size: 0x8000,
    flash_page_size: 0x80,
    flash_read_size: 0x02,
    flash_write_size: 0x80,
    fuses_address: 0x1280,
    user_row_address: 0x1300,
    user_row_size: 0x40,
    user_row_page_size: 0x01,
    user_row_read_size: 0x01,
    user_row_write_size: 0x01,
  },
  'attiny1617': {
    ...P0_PERIPHERALS,
    device_id: 0x1E9425,
    eeprom_address: 0x00001400,
    eeprom_size: 0x0100,
    eeprom_page_size: 0x20,
    eeprom_read_size: 0x01,
    eeprom_write_size: 0x01,
    flash_address: 0x00008000, // MAPPED_PROGMEM_START - flash is mapped into the data space at 0x8000, not 0x4000 (which is flash_size)
    flash_size: 0x8000,
    flash_page_size: 0x80,
    flash_read_size: 0x02,
    flash_write_size: 0x80,
    fuses_address: 0x1280,
    user_row_address: 0x1300,
    user_row_size: 0x40,
    user_row_page_size: 0x01,
    user_row_read_size: 0x01,
    user_row_write_size: 0x01,
  },
  'attiny3216': {
    ...P0_PERIPHERALS,
    device_id: 0x1E9524,
    eeprom_address: 0x00001400,
    eeprom_size: 0x0100,
    eeprom_page_size: 0x20,
    eeprom_read_size: 0x01,
    eeprom_write_size: 0x01,
    flash_address: 0x00008000, // MAPPED_PROGMEM_START - flash is mapped into the data space at 0x8000, not 0x4000 (which is flash_size)
    flash_size: 0x8000,
    flash_page_size: 0x80,
    flash_read_size: 0x02,
    flash_write_size: 0x80,
    fuses_address: 0x1280,
    user_row_address: 0x1300,
    user_row_size: 0x40,
    user_row_page_size: 0x01,
    user_row_read_size: 0x01,
    user_row_write_size: 0x01,
  },
  'attiny3217': {
    ...P0_PERIPHERALS,
    device_id: 0x1E9525,
    eeprom_address: 0x00001400,
    eeprom_size: 0x0100,
    eeprom_page_size: 0x20,
    eeprom_read_size: 0x01,
    eeprom_write_size: 0x01,
    flash_address: 0x00008000, // MAPPED_PROGMEM_START - flash is mapped into the data space at 0x8000, not 0x4000 (which is flash_size)
    flash_size: 0x8000,
    flash_page_size: 0x80,
    flash_read_size: 0x02,
    flash_write_size: 0x80,
    fuses_address: 0x1280,
    user_row_address: 0x1300,
    user_row_size: 0x40,
    user_row_page_size: 0x01,
    user_row_read_size: 0x01,
    user_row_write_size: 0x01,
  },
  'attiny814': {
    ...P0_PERIPHERALS,
    device_id: 0x1E9324,
    eeprom_address: 0x00001400,
    eeprom_size: 0x0100,
    eeprom_page_size: 0x10,
    eeprom_read_size: 0x01,
    eeprom_write_size: 0x01,
    flash_address: 0x00008000, // MAPPED_PROGMEM_START - flash is mapped into the data space at 0x8000, not 0x4000 (which is flash_size)
    flash_size: 0x2000,
    flash_page_size: 0x40,
    flash_read_size: 0x02,
    flash_write_size: 0x40,
    fuses_address: 0x1280,
    user_row_address: 0x1300,
    user_row_size: 0x20,
    user_row_page_size: 0x01,
    user_row_read_size: 0x01,
    user_row_write_size: 0x01,
  },
  'attiny816': {
    ...P0_PERIPHERALS,
    device_id: 0x1E9326,
    eeprom_address: 0x00001400,
    eeprom_size: 0x0100,
    eeprom_page_size: 0x20,
    eeprom_read_size: 0x02,
    eeprom_write_size: 0x01,
    flash_address: 0x00008000, // MAPPED_PROGMEM_START - flash is mapped into the data space at 0x8000, not 0x4000 (which is flash_size)
    flash_size: 0x2000,
    flash_page_size: 0x40,
    flash_read_size: 0x02,
    flash_write_size: 0x40,
    fuses_address: 0x1280,
    user_row_address: 0x1300,
    user_row_size: 0x20,
    user_row_page_size: 0x01,
    user_row_read_size: 0x01,
    user_row_write_size: 0x01,
  },
};
