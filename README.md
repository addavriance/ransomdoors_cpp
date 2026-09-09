# RANS0M

A fan-made recreation of the RANSOM (A-90) entity from the Roblox game
*Doors*, as a desktop app. It randomly pops the entity's face up on
your screen, you need to stop moving your mouse and stay off the keyboard, or it
"infects" your PC: 8 gold coin files get scattered around your user folders,
and you have to drag at least 5 of them onto the ransom window before the timer runs
out. Fail to pay in time and it crashes your computer.

This is a C++/SDL2 rewrite of the original WinForms app (Windows-primary; macOS
support is in development), same logic underneath. Adds a segmented progress bar
during the download jumpscare, desktop icon blockers during the ransom, a simple
placeholder win sound, and a tray-toggled Hardmode switch (off by default) instead
of the real shutdown/BSOD always being live.

This was built for fun, it's kind of poorly coded.
Right now the only noticable bug is that the ransom window doesn't always stay on top of other windows, but it should be fine for the most part. It can't go on top of fullscreen apps.

## Read this before running it

This app **really** shuts down or crashes your computer if hard mode is enabled and
you don't pay the fake ransom in time. That's not a metaphor, it calls
`shutdown /s /t 0`, or (if elevated) marks itself as a critical process so that
closing it takes Windows down with it. Hard mode is off by default - toggle it from
the tray icon, which asks for confirmation before turning it on. This means you
should:

- Only enable it on a machine you own, save your work first, and expect it to
  actually shut down or crash at some point.
- Not run it on anyone else's computer without them knowing exactly what
  it does and agreeing to it.

It is not malware in the sense of trying to steal anything, hide itself, or
spread, it doesn't touch your files besides dropping/deleting its own
harmless `.gold` marker files, and it's fully open source so you can check
that yourself. See [LICENSE.md](LICENSE.md) for the full terms and
disclaimer.

## Requirements

- Windows is the primary target (uses Win32 hooks, `shutdown.exe`, the registry,
  etc.)
- macOS support is **in development** - the `Platform_mac.mm` backend exists but
  is largely unverified on real hardware
- CMake 3.20+ and a C++17 compiler
- SDL2, SDL2_image, SDL2_mixer, SDL2_ttf - fetched and built automatically via
  CMake's `FetchContent` if not already installed, no manual setup needed

## Building & running

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Produces `ransomdoors.exe` (or the macOS binary, once that target is finished) with
the SDL DLLs and an `assets/` folder copied next to it automatically as a post-build
step. First configure takes a couple of minutes while SDL is fetched and built;
rebuilds after that are fast.

The app runs from a system tray icon (right-click it for a Close option, disabled
while a ransom is active, plus the Hardmode toggle and Config dialog described
below).

## Configuration

Right-click the tray icon → Config to edit spawn timing, infection duration, and
the ransom amount. First launch opens this automatically. Saved to
`config.json` next to `hardmode.consent` (see above for the path).

`Config` also has a "Run command on death" option - far more dangerous than
Hardmode, since it runs an arbitrary command instead of one of two fixed,
reviewable syscalls. It's session-only by design: it always starts unchecked
and is never written to `config.json`, so a tampered config file can't silently
arm it for next time - you have to knowingly re-enable it, in the dialog, every
run, and confirm the exact command before it's accepted.

## Credits

- **Doors** is made by **LSPLASH**. The RANSOM/A-90 entity, its name, look,
  and concept are their original work — this project is an unofficial fan
  recreation, not affiliated with or endorsed by LSPLASH. Go play the real
  game.
- Sound effects and images are from the game, taken from the wikis.
- Forked from [Ixars/ransomdoors](https://github.com/Ixars/ransomdoors).

## License

Source-available, free to use/modify/redistribute for educational and
non-commercial purposes, with credit required and reselling (original or
modified) forbidden. Full terms in [LICENSE.md](LICENSE.md) — read it, it's
short.
