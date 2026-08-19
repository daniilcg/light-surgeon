# Light Surgeon 1.1.0

Created by **[Dan Segal](https://www.linkedin.com/in/daniilcg/)**.

Production lighting surgeon for Autodesk Maya: pixel contribution, dead/noisy lights, portal candidates, leak hints, hero-shot matching, Maya light linking, render-camera / plate resolution, and frame-range analyze. Same engine runs as a Maya plugin and as a batch CLI.

## What it does

- **Analyze** the render camera (fallback: viewport), visible meshes, and lights (Maya + Arnold dome/area), including Maya light linking.
- **Pixel autopsy** ranks lights at a pixel; **Pixel Center** uses plate resolution from `defaultResolution`.
- **Mute dead / noisy** lights with undo; **solo** and **restore** saved intensities (Maya `intensity` or Arnold `aiIntensity`).
- **Hero match** compares a saved report and optionally scales key/fill/rim.
- **Frame range** `-frameStart` / `-frameEnd` / `-frameStep` merges contribution across the shot.
- **HUD** prints the last text report in Viewport 2.0.
- **CLI** runs the same analysis on exported JSON (farm / dailies / no Maya).

## Maya — click this, in this order

1. Build with the Maya devkit (`MAYA_LOCATION`).
2. Copy the plugin into `plug-ins/`, put `modules/lightSurgeon.mod` on `MAYA_MODULE_PATH`.
3. Plug-in Manager → load `lightSurgeon`. Menu **Light Surgeon** appears.

On this machine the `.mll` is already built:

- Maya 2023: `plug-ins/2023/lightSurgeon.mll`
- Maya 2022: `plug-ins/2022/lightSurgeon.mll`

Set `MAYA_MODULE_PATH` to `D:\Projects\Develop\Tools\Lighters\modules` (Maya.env or Windows environment), restart Maya, load the plugin.
4. Look through the **shot / render camera**. Geometry in frame.
5. **Analyze Shot**. Read DEAD / NOISY / ROLE.
6. **Mute Dead Lights**, then **Mute Noisy Lights**. Undo if you hate it.
7. **Pixel Center** if a pixel looks wrong.
8. **Write Report** for dailies. On the hero shot first, then on the new shot: `lightSurgeon -matchHero "hero.json"` and `-apply` if the scales look sane.

```mel
loadPlugin "lightSurgeon";
lightSurgeon -analyze;
lightSurgeon -analyze -json;
lightSurgeon -frameStart 1001 -frameEnd 1100 -frameStep 10;
lightSurgeon -pixelCenter;
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
