# KOReader for Nintendo Switch

> ⚠️ **This is a completely vibe-coded experiment.** I just wanted to see whether KOReader could run on the
> Switch, and had it built end-to-end by an AI assistant. It runs on a real Switch and works well — but it's a
> fun proof of concept, not a polished release. No warranty, no support.

A port of [KOReader](https://github.com/koreader/koreader) to Nintendo Switch homebrew.
It is built from KOReader `v2026.07.2-161-g945470b` (2026-09-14) and comes in two forms:

| File | What it is |
|---|---|
| `koreader-switch-v2026.07.2-161-g945470b_2026-09-14.nsp` | An installable title: KOReader gets its own icon on the HOME menu, and a game's full memory. |
| `koreader-switch-v2026.07.2-161-g945470b_2026-09-14.nro` | A homebrew app, started from the Homebrew Menu. |

Both contain the same program and use the same data folder, so you can switch between them.

**Touch screen only:** no Joy-Con or controller is needed, and buttons are ignored. Hold the bare console like a book.

> **Status:** working on a real Switch — it launches, installs itself, renders documents and the UI, handles the
> file browser and deleting files, loads user-added plugins, auto-rotates, goes online (Wikipedia, OPDS, news,
> translation), and exits cleanly back to the HOME menu.
> If something ever goes wrong, `/switch/koreader/crash.log` on the SD card contains KOReader's log (it's kept across launches).

## Install the NSP (recommended)

1. You need a Switch running custom firmware (Atmosphère) with **signature patches**, e.g. sys-patch. Homebrew titles aren't signed by Nintendo, so without them the console refuses to install or start KOReader.
2. Install the `.nsp` with a title installer such as DBI, Tinfoil or Goldleaf. Most installers are operated with buttons, so you may need a Joy-Con or another controller for this one step.
3. Tap **KOReader** on the HOME menu.

- **Update:** install a newer `.nsp` over the old one.
- **Uninstall:** *System Settings → Data Management → Manage Software → KOReader → Delete Software*. Your settings in `/switch/koreader/` are kept.
- **Title ID:** `01004B4F52454144`.

## Or use the NRO

1. Copy the `.nro` to the SD card as `/switch/koreader/koreader.nro`.
2. Open the Homebrew Menu and tap KOReader's icon. The usual way to open that menu, holding R while starting a game, needs a button. Two touch-only options:
   - **Replace a game you don't play.** Homebrew then runs with a game's full memory.
     1. Look up that game's 16-digit title ID online.
     2. Add this to `/atmosphere/config/override_config.ini`, creating the file if needed:
        ```ini
        [hbl_config]
        program_id_0=0100XXXXXXXXXXXX
        override_key_0=!R
        ```
     3. Restart the console. Tapping that game now opens the Homebrew Menu. `!R` means "unless R is held", so with a Joy-Con attached you can still hold R to play the game.
   - **Tap the Album** on the HOME menu. That runs homebrew in applet mode, with much less memory. Large documents may fail to open, and KOReader shows a warning.

## First launch

Put your books anywhere on the SD card, e.g. in `/books`. EPUB, PDF, DjVu, CBZ, FB2, MOBI, TXT and HTML all work, like in any KOReader build.

On first launch, and after installing a newer version, KOReader unpacks itself to `/switch/koreader/`. That is about 65 MB and 1,000 files; progress is shown on screen. To force a reinstall, create an empty `/switch/koreader/.reinstall` file.

## Orientation

KOReader boots in the panel's **native landscape** orientation, upright, and orientation is fully under your control via *Settings → (☰ last tab) → Rotation*:
- **Auto-rotation (accelerometer):** the screen follows how you hold the console — turn it 90° to read in portrait, like a book. You can lock auto-rotation to the current orientation or ignore the accelerometer entirely. ⚠️ Only the **Switch Lite** has a built-in motion sensor; on a regular **Switch/OLED** the sensor is in the Joy-Con, so auto-rotation there needs the Joy-Con attached. Docked, or a bare tablet with no sensor, it simply keeps the orientation you last set.
- **Manual:** pick any of the four orientations (portrait, landscape, inverted) and it stays there. Works on every model, sensor or not.

| Touch | Action |
|---|---|
| Tap the right ¾ of the page, or swipe left | Next page |
| Tap the left ¼ of the page, or swipe right | Previous page |
| Tap the top of the screen, or swipe down from it | Main menu (file browser, settings, brightness…) |
| Tap the bottom of the screen, or swipe up from it | Reading settings (font, margins, contrast…) |
| Long-press a word, or long-press and drag | Highlight, look up on Wikipedia, translate |
| Pinch / spread | Zoom (PDF, CBZ, DjVu) |

The zones and gestures can be changed under *Settings → Taps and gestures*.

**To leave KOReader,** open the main menu (tap the top), go to the last tab (☰) and tap **Exit → Exit**. KOReader saves and closes cleanly. Where you land depends on how it was launched:
- **Installed NSP, or the NRO launched by holding R on a game:** you go straight back to the Switch HOME menu.
- **NRO launched from the Album (applet mode):** you return to the Homebrew Menu.

**Restart KOReader** is in the same submenu. Pressing POWER puts the console to sleep as usual, and KOReader is still open when it wakes up. Closing KOReader from the HOME menu also exits it cleanly (settings and reading position are saved).

When docked, the UI switches to landscape, but without a controller there's nothing to operate it with.

## Files

Everything lives in `/switch/koreader/` on the SD card: settings, reading history, caches, extra fonts (`fonts/`), style tweaks and so on. Deleting that folder resets KOReader; it reinstalls itself on the next launch.

- **Logs:** `crash.log` keeps growing across launches, so a crash can still be looked into after relaunching. Past 1 MB, it's moved to `crash.old.log`.
- **Updates:** KOReader reinstalls itself whenever the `.nro` or `.nsp` holds a different tree than the installed one; that tree is identified by a hash in `.install-id`. It also removes the files the previous install put there that the new one no longer has, going by the list kept in `.install-manifest`. Files you added yourself (settings, fonts, dictionaries, plugins…) are never touched.

## Adding plugins

Drop a plugin's `.koplugin` folder into `/switch/koreader/plugins/` on the SD card and relaunch. It's loaded like on any other KOReader build, and enabled under *Settings → (☰) → Plugin management*. User-added plugins are kept across updates. Pure-Lua plugins work; a plugin that loads an external native library of its own won't (there's no dynamic loader — everything is statically linked), but that's rare.

## Differences from other KOReader builds

- **No JIT.** Homebrew can't easily get executable memory, so LuaJIT runs as an interpreter. Document rendering (crengine, MuPDF) is native code, but some menus may feel slower than on a Kobo or Kindle.
- **No subprocesses** (Horizon has no `fork`):
  - Work that normally runs in the background (Wikipedia, OPDS and news downloads, full-text search) runs in the foreground and can't be cancelled.
  - Background cover extraction and page-browser thumbnails are unavailable.
- **No StarDict dictionaries.** They rely on the external `sdcv` program. Wikipedia lookups and translation still work when online.
- **Network:** Wi-Fi is managed by the console, and KOReader uses whatever connection it has. Online features (Wikipedia, translation, OPDS catalogs, news) work over the console's connection.
- **Frontlight** controls the Switch's backlight; 0 is the dimmest setting, it can't turn off.
- **Auto-sleep** is disabled while KOReader is open, so the screen doesn't dim mid-page.
- **Not included:** the SSH, terminal, time sync, auto-frontlight, auto-warmth and external keyboard plugins.

## Building from source

`source/` contains:

| File | Applies to |
|---|---|
| `koreader.patch` | [koreader](https://github.com/koreader/koreader) at `945470b` |
| `koreader-base.patch` | [koreader-base](https://github.com/koreader/koreader-base) at `4f808b1b` (checked out in `base/`) |
| `crengine.patch` | [crengine](https://github.com/koreader/crengine) at `55b4225` (`base/thirdparty/kpvcrlib/crengine`) |
| `build-toolchain.sh` | Builds devkitA64 (GCC 16.1, binutils 2.46, newlib), libnx, switch-tools and hacbrewpack from source |
| `icon/` | The icon's generator: `make_icon.swift` renders it from KOReader's vector logo, `png2jpg.c` encodes it |
| `tests/` | Host tests for the touch input (`input_test.c`, against a fake `switch.h`) and the `os.exit` handling (`exit_test.c`, against LuaJIT). The installer's test is `base/switch/install_test.c`. |

You only need `build-toolchain.sh` if devkitPro's package servers aren't usable. Otherwise, install `switch-dev` with devkitPro pacman, plus hacbrewpack for the NSP.

```sh
git clone https://github.com/koreader/koreader && cd koreader
git checkout 945470b && make fetchthirdparty
git -C base checkout 4f808b1b && git -C base submodule update --init
git apply /path/to/source/koreader.patch
git -C base apply /path/to/source/koreader-base.patch
git -C base/thirdparty/kpvcrlib/crengine apply /path/to/source/crengine.patch
export DEVKITPRO=/opt/devkitpro        # or the prefix used by build-toolchain.sh
make TARGET=switch update              # the .nro (GNU make >= 4.1)
make TARGET=switch nsp SWITCH_KEYS=/path/to/prod.keys   # the .nsp
```

The NSP needs the keys dumped from your own console, e.g. with Lockpick_RCM: they encrypt the title. Only `header_key` and `key_area_key_application_00` are used, and the keys don't end up in the NSP.

On macOS the build also needs, from Homebrew: `cmake ninja meson make coreutils gnu-sed pkg-config gettext flock`. The toolchain script additionally needs `gmp mpfr libmpc isl texinfo bison flex lz4 gpatch`.

## How the port works

- **`base/switch/main.c`** is the entry point. It:
  - shows the splash screen, drawn with libnx's console font in both orientations;
  - installs the KOReader tree from the RomFS to the SD card (`base/switch/install.c`);
  - sets up logging and the time zone, and starts the system services;
  - runs `reader.lua` with LuaJIT on a thread with a 32 MB stack. `os.exit` unwinds back to that thread's entry function, so the process always exits from the main thread, which libnx's exit path requires;
  - on exit, closes the whole application (`__nx_applet_exit_mode`) instead of returning to the Homebrew Menu. Restarting goes through the homebrew loader for the NRO, and through `appletRestartProgram` for the installed title.
- **`base/switch/input.c`** turns the touch screen into multi-touch protocol B events, like a Kobo's, plus applet events (close requested from the HOME menu, docking). It replays HID's touch history, so quick taps between two polls aren't lost, and polls at 120 Hz while the screen is touched and 60 Hz otherwise. It also reads the handheld six-axis sensor and emits KOReader's `EV_MSC`/`MSC_GYRO` rotation events so auto-rotation works; the accelerometer→orientation mapping is a single `GYRO_BASE` constant. Controller buttons aren't polled.
- **`base/switch/device.c`** handles framebuffer presentation, plus battery, backlight, network and auto-sleep. Frames go through libnx's linear shadow buffer, which libnx converts to the GPU's block-linear layout.
- **`base/ffi/framebuffer_switch.lua`, `base/ffi/input_switch.lua` and `frontend/device/switch/`** are the KOReader backends. The device declares no keys and no D-pad, so the UI behaves like on a touch-only e-reader. It boots in the panel's native landscape orientation; rotation (portrait and the rest) is driven by the accelerometer and the rotation menu, with touch handled by KOReader's standard per-rotation transforms.
- **LuaJIT** is built interpreter-only, with two more build options:
  - The system allocator, because LuaJIT's own allocator releases parts of `mmap`ed segments, which the `mmap` emulation can't do safely.
  - Internal error unwinding, so Lua errors don't depend on the platform's DWARF unwinder. No C++ exceptions cross Lua frames in KOReader. There is no dynamic loader, so `ffi.C`, `ffi.load` and `require` of C modules look symbols up in a table generated at link time (`base/switch/gen_symtab.py`) from KOReader's ffi-cdecl lists.
- **`base/switch/compat/` and `base/switch/include/`** are a small POSIX layer filling gaps in newlib and libnx: a `getentropy` backend, `mmap` emulation, `pread`, `fcntl` locks and so on. A GCC specs file adds it to every link, configure checks included.
- **Packaging** (`make/switch.mk`):
  - The NRO embeds the KOReader tree as its RomFS.
  - The NSP turns the same ELF into an NSO, with program metadata (`platform/switch/koreader.json`) and a NACP adjusted for an installed title (`platform/switch/nsp_nacp.py`: no user selection, no save data). hacbrewpack then packs it with the same RomFS.
- **Compiler flags** match libnx's own, including `-ftls-model=local-exec`. With libnx's software thread pointer (`-mtp=soft`), the default thread-local storage model for PIC code computes garbage addresses. LuaJIT keeps its exception object in thread-local storage, so the first caught Lua error crashed.
- **Third-party fixes** cover LuaJIT (static FFI symbols, PRNG seeding), LibreSSL, libk2pdfopt and crengine. There is also a newlib variant of `ffi/posix_h.lua`, generated with ffi-cdecl and checked against the target's struct sizes.
