# Fuselage

*Free Unrestricted Software Enabling Layered Asset Game Environments*

Fuselage is a game engine for a machine that never existed. It emulates a retro-inspired system that could have plausibly shipped in the late 90s or early 2000s. Indexed color, lots of sprites, a tile-mapped world, and smart frame buffers; all built on a modern Vulkan rendering pipeline underneath and invisible to the games running on top of it. Developers develop against a small, honest API; players get pixel-perfect worlds with the texture of hardware that predates them, running at whatever framerate a modern GPU can hold steady.

The retro constraints are chosen to provide an authentic retro aesthic, small file size, efficient resource use, and of course
performance.

---

## Features

- Native Vulkan rendering pipeline, entirely hidden behind a simple frame-loop API. (GDMF)
- Sprite, tile, and text layers built on real retro constraints: 4-bit indexed color, 16-color palettes, 256 shared palette slots.
- A programmable "Pixie" framebuffer layer for effects and procedural content no sprite or tile system could pull off.
- Built-in retro color sets (C64, ZX Spectrum, ANSI, Tandy, NES — plus fully custom palettes).
- Zero runtime asset-decoding dependencies: PNGs are compressed into embedded C headers at build time.
- Raw keyboard, mouse, and controller input with a long term goal of reducing input latency. (CAKE)
- A dedicated timing subsystem driving precise, independently-paced input, game logic, and rendering. (DICE)

## Status

- **0.1 (ANUS)** — initial RayLib-based API prototype.
- **0.2 (BUTTOCKS)** — complete: RayLib removed for a native Vulkan pipeline. GDMF (graphics) and CAKE (input) deliver the full sprite/tile/text/input feature set, plus the Pixie layer.
- **0.3 (COLON)** — current release: introduces DICE (timing subsystem — timers and RNG) and finalizes the project structure for public release.
- **0.4 (DERRIERE)** — next release, already underway. Will add the optional RISC-V VPU for assembly-level scripting.

For full technical breakdown of every subsystem see DETAILS.md.

## Branches

- **ANUS** — the original RayLib-based prototype. Feature-complete, no longer developed.
- **BUTTOCKS** — the first native-Vulkan pipeline (RayLib removed in favor of GDMF for graphics and CAKE for input). Superseded by COLON.
- **COLON** — the current 0.3 release: adds the DICE timing subsystem. Promoted to `main` for this release, with a dedicated `COLON` branch kept for ongoing fixes.
- **DERRIERE** — the next release, already underway; new DERRIERE features land on `main`.

---

## Summary

Fuselage brings to life a "what if?" vision of a system that could have existed in the past, blending retro limitations with modern development practices. By combining a native Vulkan rendering pipeline, a feature-rich set of 2D graphics layers (sprites, tiles, text, and the Pixie programmable framebuffer), a dedicated timing subsystem (DICE), and embedded asset tooling, Fuselage aims to be an extensible platform for retro-inspired creativity with a strong focus on modularity, efficiency, and performance.

---

## License

Fuselage is released under the [Zero-Clause BSD](LICENSE) license — free to use, modify, and distribute, with **no attribution required**. That said, attribution is genuinely appreciated; and if you build something with Fuselage, simply letting other developers know it exists (even without declaring its use) helps the project more than anything.
