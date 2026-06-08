# SimpleCAD3D

A small Python/OpenGL desktop CAD sketchpad. It is intentionally lightweight:
add boxes, cylinders, spheres, and cones; select objects; move, scale, rotate,
delete, and export the scene as an ASCII STL.

## Install

```powershell
py -m pip install -r requirements.txt
```

## Run

```powershell
py src/simplecad3d.py
```

## Controls

- Left drag: orbit camera
- Right drag or middle drag: pan camera
- Mouse wheel: zoom
- Click object: select
- `B`: add box
- `C`: add cylinder
- `S`: add sphere
- `N`: add cone
- `G`: toggle grid
- `Tab`: cycle selected object
- Arrow keys: move selected object on X/Z
- `PageUp` / `PageDown`: move selected object on Y
- `+` / `-`: scale selected object
- `Q` / `E`: rotate selected object around Y
- `Delete` / `Backspace`: delete selected object
- `Ctrl+S`: export `simplecad_export.stl`
- `R`: reset camera
- `Esc`: quit
