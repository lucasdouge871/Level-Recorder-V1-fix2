# Level Recorder (Geode, GD 2.2081)

Developer: Lucasdouge · Mod ID: `lucasdouge.level-recorder` · Windows only (v1.0.0)

## How it works
1. **REC**: open the pause menu, tap the Level Recorder button, pick REC, press the big box.
   Only your inputs are logged (no capture, no overhead). Your last attempt is kept; completing the
   level marks the macro as a completion and saves it. Toggle **MIC** first to record your microphone.
2. **RENDER**: load a macro (File menu), pick RENDER, press the box. The level is replayed at a fixed
   timestep and every frame is piped to ffmpeg, so lag in-game cannot drop frames. Output goes to
   `<mod save dir>/renders`. The level song (and your mic take, if MIC is on) is muxed in.
3. **PLAY**: previews a macro in-game. The Speed slider (0.25x to 4x) applies to PLAY.

Volume slider = microphone gain applied when muxing.

Requires **ffmpeg** (set its path in the mod settings if it isn't on PATH).

## Importing macros from other mods
`File` lists everything in `macros/` and `imports/` (inside the mod's save directory).
Text macros are accepted (`.txt` or `.lrm`), one input per line:

    frame button down player2     e.g.  1532 1 1 0
    frame down player2            (button defaults to 1)
    frame down

Binary formats from other mods (GDR/.gdr, Mega Hack, QOLMod, Eclipse, etc.) are NOT parsed yet.
Add a parser in `io::load` in `src/Recorder.cpp` once you have the format spec, or convert to the
text format above. Macros must be made at the same TPS they are replayed at.

## Building
Local: install the Geode SDK + CLI, then `geode build` → produces `lucasdouge.level-recorder.geode`.
GitHub: push this folder to a repo and run the **Build Level Recorder** workflow; the artifact is the .geode.
Drop the .geode into `Geometry Dash/geode/mods/`.

## Known limitations
- Not yet compiled or tested against the SDK: expect small API fixes on first build.
- Only full normal-mode runs from the start (no startpos/practice checkpoints).
- Game SFX are not captured (only the level song + mic).
- Seek bar is display-only.
- Single-player-button inputs only (jump); platformer directional inputs untested.
