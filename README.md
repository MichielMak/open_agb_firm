# open_agb_firm

Bare-metal GBA runner for the 3DS. Loads `.gba` files directly from the SD card using the console's real GBA hardware — not emulation.

A complete replacement for GBA Virtual Console injects (AGB_FIRM), with accurate saves, color correction, button remapping, live settings, and lid-close sleep.

> **Beta software.** Reasonably stable, but use at your own risk. We are not responsible for damage to your system.

---

## Installation

1. Download the [latest release](https://github.com/MichielMak/open_agb_firm/releases/latest) and extract it.
2. Copy `open_agb_firm.firm` to `/luma/payloads/` on your SD card (Luma3DS), or assign it a slot in fastboot3DS.
3. Copy the `3ds` folder to the root of your SD card. Merge if prompted.
4. Launch via Luma3DS by holding **START** at boot, or via your fastboot3DS slot.
5. Use the file browser to pick a `.gba` ROM.

---

## Controls

| Combo | Action |
|-------|--------|
| A / B / L / R / START / SELECT | GBA buttons |
| X + SELECT | Open live settings menu |
| X + UP / DOWN | Adjust backlight by `backlightSteps` |
| X + LEFT | Turn off LCD backlight |
| X + RIGHT | Turn on LCD backlight |
| SELECT + Y | Screenshot to `/3ds/open_agb_firm/screenshots/` |
| X (hold on launch) | Skip ROM patches |
| Power (hold) | Power off |

> If the screen freezes after a screenshot, press HOME to recover.

---

## Live Settings Menu

Press **X+SELECT** during gameplay. The top screen keeps showing the game.

| Button | Action |
|--------|--------|
| Up / Down | Move between items |
| Left / Right | Adjust value |
| B or X+SELECT | Close |
| Power | Power off |

### Items

| Item | Range | Step | Default |
|------|-------|------|---------|
| Backlight | hardware min–max | `backlightSteps` | 64 |
| Color Profile | None / GBA / GB Micro / GBA SP / NDS / NDS Lite / Switch Online / VBA / Identity | — | None |
| Contrast | 0.00–3.00 | 0.05 | 1.00 |
| Brightness | -1.00–1.00 | 0.05 | 0.00 |
| Saturation | 0.00–3.00 | 0.05 | 1.00 |
| Volume | Muted / -63.5 dB–+24 dB / Slider | 0.5 dB | Slider |
| Audio Output | Auto / Speakers / Headphones | — | Auto |

**Notes:**
- Contrast, Brightness, and Saturation have no effect when Color Profile is None.
- Switching between non-None profiles updates immediately. Switching to or from None requires a game restart (changes the capture pipeline). These items are marked **(restart to apply)**.
- Volume skips the range -19 to +48 dB — that range is undefined behavior on this hardware.
- All changes are **session-only**. Edit `config.ini` to persist them.

---

## Configuration

Settings are stored in `/3ds/open_agb_firm/config.ini`. Per-game overrides go in `/3ds/open_agb_firm/saves/<romName>.ini`.

### [general]

| Key | Default | Description |
|-----|---------|-------------|
| `backlight` | `64` | Backlight brightness (cd/m²). Old 3DS: 20–117. New 3DS: 16–142. Values ≤64 recommended. |
| `backlightSteps` | `5` | Step size for X+UP/DOWN and the settings menu. |
| `directBoot` | `false` | Skip the GBA BIOS intro animation. |
| `useGbaDb` | `true` | Use `gba_db.bin` to identify save types. |
| `useSavesFolder` | `true` | Store saves in `/3ds/open_agb_firm/saves/` instead of next to the ROM. |

### [video]

| Key | Default | Options / Range | Description |
|-----|---------|-----------------|-------------|
| `scaler` | `matrix` | `none`, `bilinear`, `matrix` | Upscaling method for the 1.5× top-screen output. |
| `colorProfile` | `none` | `none`, `gba`, `gb_micro`, `gba_sp101`, `nds`, `ds_lite`, `nso`, `vba`, `identity` | Color correction profile. Use `identity` if you just want to tweak contrast/saturation without a full profile. Non-`none` profiles increase RAM usage and reduce battery life. |
| `contrast` | `1.0` | 0.0–3.0 | Screen gain. No effect when `colorProfile=none`. |
| `brightness` | `0.0` | -1.0–1.0 | Screen lift. No effect when `colorProfile=none`. |
| `saturation` | `1.0` | 0.0–3.0 | Color saturation. No effect when `colorProfile=none`. |

### [audio]

| Key | Default | Options / Range | Description |
|-----|---------|-----------------|-------------|
| `audioOut` | `auto` | `auto`, `speakers`, `headphones` | Force a specific output or let the hardware decide. |
| `volume` | `127` | -128–127 | -128 = muted. -127 to -20 = -63.5 to 0 dB. Values above 48 = use hardware volume slider. Avoid -19 to 48 (undefined hardware behavior). |

### [input]

Maps 3DS buttons to GBA inputs. Each key accepts one or more buttons separated by commas (no spaces).

Available buttons: `A B SELECT START RIGHT LEFT UP DOWN R L X Y TOUCH CP_RIGHT CP_LEFT CP_UP CP_DOWN`

`TOUCH` reacts to any touchscreen press. `CP_*` is the Circle Pad. Button mappings can add up to 1 frame of input lag.

```ini
[input]
RIGHT=RIGHT,CP_RIGHT
LEFT=LEFT,CP_LEFT
UP=UP,CP_UP
DOWN=DOWN,CP_DOWN
```

### [game]

Intended for per-game `.ini` files only.

| Key | Default | Description |
|-----|---------|-------------|
| `saveSlot` | `0` | Save file slot (0–9). |
| `saveType` | `auto` | Override the save type. See options below. |

Save type options: `eeprom_8k`, `rom_256m_eeprom_8k`, `eeprom_64k`, `rom_256m_eeprom_64k`, `flash_512k_atmel`, `flash_512k_atmel_rtc`, `flash_512k_sst`, `flash_512k_sst_rtc`, `flash_512k_panasonic`, `flash_512k_panasonic_rtc`, `flash_1m_macronix`, `flash_1m_macronix_rtc`, `flash_1m_sanyo`, `flash_1m_sanyo_rtc`, `sram_256k`, `none`, `auto`

Options prefixed with `rom_256m` are for 32 MiB games. Options suffixed with `_rtc` enable the hardware real-time clock.

### [advanced]

| Key | Default | Description |
|-----|---------|-------------|
| `saveOverride` | `false` | Show a save type selection menu after launching a game. |
| `colorOverride` | `false` | Allow per-game `.ini` files to override the global `colorProfile`. |
| `defaultSave` | `sram_256k` | Fallback save type when the game isn't in `gba_db.bin` and autodetection fails. Same options as `saveType`, excluding `auto`. |

---

## Patches

IPS and UPS patches are applied automatically if a patch file with the same base name as the ROM is present in the same directory.

```
example.gba   →   example.ips  (or example.ups)
```

Hold **X** while launching a game to skip patch application.

---

## EEPROM Saves and Emulator Compatibility

Most emulators write EEPROM saves in a different format than open_agb_firm. Use [gba-eeprom-save-fix](https://exelotl.github.io/gba-eeprom-save-fix/) by exelotl to convert in either direction.

---

## Hardware Limitations

open_agb_firm uses the real GBA hardware inside the 3DS. Some limitations are fundamental and cannot be worked around in software:

- ROMs larger than 32 MiB (256 Mbit).
- Cartridge add-ons other than RTCs (e.g. solar sensors, gyroscopes). Patches are required.
- Save type detection during gameplay — type must be known before launch.
- GBA Link Cable / serial port.
- SRAM larger than 32 KiB (affects some homebrew).
- Game switching requires a reboot.
- No save states.
- Audio aliasing artifacts (hardware bug, no known fix).

---

## Known Issues

- Screenshot (SELECT+Y) can occasionally freeze the screen output. Press HOME to recover.
- Save type autodetection may fail for some EEPROM games.

Found a bug? [Open an issue](https://github.com/profi200/open_agb_firm/issues).

---

## FAQ

**Why does this run as a FIRM instead of a normal 3DS app?**
Accessing the GBA hardware requires full hardware control, which is only possible from FIRM level.

**Is it safe?**
Yes. open_agb_firm has been used by many people and some of its backend code is also used in [fastboot3DS](https://github.com/derrekr/fastboot3DS).

**Which games are compatible?**
All of them in theory, except those that hit the [hardware limitations](#hardware-limitations) above.

**Why do the colors look washed out?**
Most 2/3DS LCDs are not factory calibrated. Set `colorProfile=identity` and tune `contrast` and `saturation` to taste, or pick a profile that matches your target hardware.

**Why doesn't my ROM hack or homebrew save correctly?**
open_agb_firm uses `gba_db.bin` for official games only. For anything not in the database it falls back to autodetection, which can misidentify EEPROM. Try enabling `saveOverride=true` to pick the type manually, or set `defaultSave` to a known-good type for your game.

**Why doesn't my emulator save work?**
Likely an EEPROM format mismatch. See [EEPROM Saves and Emulator Compatibility](#eeprom-saves-and-emulator-compatibility).

---

## Building

Requirements:
- [devkitARM](https://devkitpro.org/wiki/devkitPro_pacman)
- [CTR Firm Builder](https://github.com/derrekr/ctr_firm_builder) or [firmtool](https://github.com/TuxSH/firmtool)
- `p7zip-full` for release builds

```sh
git clone --recurse-submodules https://github.com/MichielMak/open_agb_firm
cd open_agb_firm
make          # debug build
make release  # release archive (.7z)
```

Update: `git pull && git submodule update --init --recursive`

---

## License

GPL v3 or any later version. See `LICENSE.txt`.

---

## Credits

- **yellows8**, **plutoo**, **smea**, **Normmatt**, **WinterMute** — early 3DS research
- **ctrulib devs**, **LumaTeam**, **devkitPro**
- **ChaN** — FatFS
- **benhoyt** — inih
- **fastboot3DS project**
- **MAME**, **No-Intro**
- **Wolfvak, Sono** and everyone in #GodMode9
- **endrift, Extrems** and everyone in #mgba
- **Oleh Prypin (oprypin)** — nightly.link
- **hunterk, Pokefan531** — [libretro color profiles](https://forums.libretro.com/t/real-gba-and-ds-phat-colors/1540/220)
- Everyone who contributed to **3dbrew.org**

Copyright (C) 2024 derrek, profi200, d0k3
