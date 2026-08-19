# Light Surgeon 1.0.0

Created by **[Dan Segal](https://www.linkedin.com/in/daniilcg/)**.

Production lighting surgeon for Autodesk Maya: pixel contribution, dead/noisy lights, portal candidates, leak hints, and hero-shot matching. Same engine runs as a Maya plugin and as a batch CLI.

## What it does

- **Analyze** the active camera, meshes, and lights (Maya + Arnold dome/area).
- **Pixel autopsy** ranks lights by estimated irradiance at a render-view pixel.
- **Mute dead / noisy** lights with undo; **solo** and **restore** saved intensities.
- **Hero match** compares a saved report and optionally scales key/fill/rim.
- **HUD** locator prints the last report in Viewport 2.0.
- **CLI** runs the same analysis on exported JSON (farm / dailies / no Maya).

## Maya install

1. Build the plugin with the Maya devkit (`MAYA_LOCATION` pointing at the Maya install).
2. Copy `lightSurgeon.mll` (Windows), `.so` (Linux) or `.bundle` (macOS) into `plug-ins/`.
3. Add the `modules` directory to `MAYA_MODULE_PATH`, or copy `modules/lightSurgeon.mod` next to `plug-ins/` and `scripts/`.
4. In Maya: Window → Settings/Preferences → Plug-in Manager → load `lightSurgeon`.
5. Menu **Light Surgeon** appears on the main menu bar.

```mel
loadPlugin "lightSurgeon";
lightSurgeon -analyze;
lightSurgeon -pixel 960 540;
lightSurgeon -muteDead;
lightSurgeon -muteNoisy;
lightSurgeon -solo "key";
lightSurgeon -restore;
lightSurgeon -report "shot_light_report.json";
lightSurgeon -matchHero "hero_shot.json" -apply;
lightSurgeon -exportScene "shot_lights.json";
```

Python:

```python
import maya.cmds as cmds
cmds.loadPlugin("lightSurgeon")
print(cmds.lightSurgeon(analyze=True))
print(cmds.lightSurgeon(pixel=(960, 540)))
cmds.lightSurgeon(muteDead=True)
cmds.lightSurgeon(restore=True)
```

Undo works for mute, solo, restore, and apply-match.

## Batch CLI

```
lightsurgeon analyze tests/fixtures/studio.json -o report.json
lightsurgeon pixel tests/fixtures/studio.json 960 540
lightsurgeon match current.json hero.json
lightsurgeon portals tests/fixtures/studio.json
```

## Build (engine + tests, no Maya)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Maya plugin (example Maya 2024):

```
cmake -S . -B build -DMAYA_LOCATION="C:/Program Files/Autodesk/Maya2024" -DMAYA_VERSION=2024
cmake --build build --config Release
```

## JSON scene schema

`camera` (position, aim, up, fov, near, far, aspect, width, height), `lights` (name, type: directional|point|spot|area|dome, position, direction, color, intensity, exposure, radius, coneAngle, penumbra, areaWidth, areaHeight, enabled, include/exclude object names), `meshes` (name, vertices, triangles as index triplets).

The estimator is a visibility-weighted lighting model for look decisions, not a final path tracer.

## Author

**Dan Segal** — [linkedin.com/in/daniilcg](https://www.linkedin.com/in/daniilcg/)
