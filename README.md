# RANS0M (C++)

A fan-made recreation of the RANSOM (A-90) entity from the Roblox game
*Doors*, as a desktop app. It pops the entity's face up on your screen at
random; stay off your mouse and keyboard or it "infects" your PC — gold coin
files get scattered around your folders, and you have to drag enough of them
onto the ransom window before the timer runs out. Fail to pay in time and,
if you enabled it, it crashes your computer for real.

</br>
<img width="70%" alt="image_2026-09-10_22-45-53" src="https://github.com/user-attachments/assets/15a9c3d1-4209-443f-9be2-49a343e1e845" />
<img width="25%" alt="image_2026-09-10_22-48-44" src="https://github.com/user-attachments/assets/7feec338-d3a3-4238-9ba7-1c496feeed20" />
</br></br>

This is a C++/SDL2 rewrite of the original WinForms app, same logic
underneath, with a more structured codebase than the original source. The
repo ships without binaries, and the final build is roughly half the size
of the original. Built for fun, not perfectly polished.

## Read this before running it

If you enable "crash on death" in Config, it **really** shuts down or
crashes your computer when the timer runs out unpaid. Not a metaphor — it
calls `shutdown /s /t 0`, or (if elevated) BSODs. Off by default, requires
an explicit confirmation in the Config dialog, and resets every launch — it
never persists to disk. So:

- Only enable it on a machine you own, save your work first, and expect it
  to actually shut down or crash.
- Never run it on someone else's computer without them knowing exactly
  what it does and agreeing to it.

It's not malware in the sense of stealing anything, hiding, or spreading —
it doesn't touch your files besides its own `.gold`/`.crucifix` marker
files, and it's fully open source. See [LICENSE.md](LICENSE.md).

## Requirements

- Windows is the primary target (Win32 hooks, `shutdown.exe`, the registry)
- macOS backend exists in source but is unverified on real hardware
- CMake 3.20+ and a C++17 compiler
- Everything else (SDL2 + friends, Dear ImGui, nlohmann/json) is fetched and
  built automatically via CMake's `FetchContent` — no manual setup

## Building & running

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

First configure takes a few minutes while dependencies are fetched and
built; rebuilds after that are fast. Produces a single self-contained
`ransomdoors.exe` — everything (SDL2 and friends, all assets) is statically
linked and embedded into the binary, no DLLs or `assets/` folder needed
next to it.

Runs from a system tray icon — right-click for Close and Config.

## Configuration

Right-click the tray icon → Config. Spawn timing, infection duration, ransom
amount, and whether coins scatter across your real folders or into a
temporary "drawers" folder. First launch opens this automatically.

"Run command on death" is far more dangerous than crash-on-death — it runs
an arbitrary command instead of a fixed, reviewable one. Same session-only
protection: unchecked by default, confirmed explicitly, never saved.

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
