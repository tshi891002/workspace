from __future__ import annotations

import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

import pyglet
from pyglet.gl import Config
from pyglet.gl.gl_compat import *
from pyglet.window import key, mouse


WIDTH, HEIGHT = 1280, 800
BG = (0.055, 0.06, 0.07, 1.0)


@dataclass
class Vec3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def __add__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, scalar: float) -> "Vec3":
        return Vec3(self.x * scalar, self.y * scalar, self.z * scalar)


@dataclass
class CadObject:
    kind: str
    name: str
    pos: Vec3 = field(default_factory=Vec3)
    rot_y: float = 0.0
    scale: Vec3 = field(default_factory=lambda: Vec3(1, 1, 1))
    color: tuple[float, float, float] = (0.7, 0.7, 0.8)


@dataclass
class Camera:
    yaw: float = -42.0
    pitch: float = 28.0
    distance: float = 12.0
    target: Vec3 = field(default_factory=Vec3)

    def eye(self) -> Vec3:
        yaw = math.radians(self.yaw)
        pitch = math.radians(self.pitch)
        cp = math.cos(pitch)
        return Vec3(
            self.target.x + self.distance * cp * math.sin(yaw),
            self.target.y + self.distance * math.sin(pitch),
            self.target.z + self.distance * cp * math.cos(yaw),
        )


class SimpleCAD(pyglet.window.Window):
    def __init__(self) -> None:
        config = Config(
            double_buffer=True,
            depth_size=24,
            sample_buffers=1,
            samples=4,
            major_version=2,
            minor_version=1,
        )
        super().__init__(WIDTH, HEIGHT, "SimpleCAD3D - Python OpenGL CAD", resizable=True, config=config)
        self.camera = Camera()
        self.objects: list[CadObject] = []
        self.selected = -1
        self.show_grid = True
        self.drag_button: int | None = None
        self.next_id = 1
        self.message = "B/C/S/N add primitives | drag orbit/pan | Ctrl+S export STL"
        self.message_ttl = 240
        self.labels: list[pyglet.text.Label] = []
        self._init_gl()
        self._seed_scene()
        pyglet.clock.schedule_interval(self._tick, 1 / 60)

    def _init_gl(self) -> None:
        glClearColor(*BG)
        glDisable(GL_DEPTH_TEST)

    def _seed_scene(self) -> None:
        self.add_object("box", Vec3(-1.4, 0.5, 0), Vec3(1.8, 1, 1.2))
        self.add_object("cylinder", Vec3(1.3, 0.55, 0.15), Vec3(0.8, 1.1, 0.8))
        self.selected = 0

    def add_object(self, kind: str, pos: Vec3 | None = None, scale: Vec3 | None = None) -> None:
        palette = {
            "box": (0.34, 0.69, 0.92),
            "cylinder": (0.42, 0.82, 0.58),
            "sphere": (0.94, 0.66, 0.31),
            "cone": (0.9, 0.45, 0.5),
        }
        obj = CadObject(
            kind=kind,
            name=f"{kind}-{self.next_id}",
            pos=pos or Vec3(0, 0.5, 0),
            scale=scale or Vec3(1, 1, 1),
            color=palette[kind],
        )
        self.next_id += 1
        self.objects.append(obj)
        self.selected = len(self.objects) - 1
        self.toast(f"Added {obj.name}")

    def toast(self, text: str) -> None:
        self.message = text
        self.message_ttl = 240

    def _tick(self, _dt: float) -> None:
        if self.message_ttl > 0:
            self.message_ttl -= 1

    def on_draw(self) -> None:
        self.clear()
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        if self.show_grid:
            self._draw_grid()
        self._draw_axes()
        self._draw_objects()
        self._draw_overlay()

    def on_mouse_press(self, x: int, y: int, button: int, _modifiers: int) -> None:
        if button in (mouse.LEFT, mouse.MIDDLE, mouse.RIGHT):
            self.drag_button = button
            if button == mouse.LEFT:
                self._pick(x, y)

    def on_mouse_release(self, _x: int, _y: int, _button: int, _modifiers: int) -> None:
        self.drag_button = None

    def on_mouse_drag(self, _x: int, _y: int, dx: int, dy: int, _buttons: int, _modifiers: int) -> None:
        if self.drag_button == mouse.LEFT:
            self.camera.yaw += dx * 0.35
            self.camera.pitch = max(-85, min(85, self.camera.pitch + dy * 0.35))
        elif self.drag_button in (mouse.MIDDLE, mouse.RIGHT):
            yaw = math.radians(self.camera.yaw)
            right = Vec3(math.cos(yaw), 0, -math.sin(yaw))
            up = Vec3(0, 1, 0)
            amount = self.camera.distance * 0.0015
            self.camera.target = self.camera.target - right * (dx * amount) - up * (dy * amount)

    def on_mouse_scroll(self, _x: int, _y: int, _scroll_x: float, scroll_y: float) -> None:
        factor = 0.9 if scroll_y > 0 else 1.1
        self.camera.distance = min(60, max(2.5, self.camera.distance * factor))

    def on_key_press(self, symbol: int, modifiers: int) -> None:
        if symbol == key.ESCAPE:
            self.close()
        elif symbol == key.B:
            self.add_object("box")
        elif symbol == key.C:
            self.add_object("cylinder")
        elif symbol == key.S and not (modifiers & key.MOD_CTRL):
            self.add_object("sphere")
        elif symbol == key.N:
            self.add_object("cone")
        elif symbol == key.S and (modifiers & key.MOD_CTRL):
            self.export_stl(Path("simplecad_export.stl"))
        elif symbol == key.G:
            self.show_grid = not self.show_grid
        elif symbol == key.TAB and self.objects:
            self.selected = (self.selected + 1) % len(self.objects)
        elif symbol in (key.DELETE, key.BACKSPACE):
            self.delete_selected()
        elif symbol == key.R:
            self.camera = Camera()
        else:
            self._transform_selected(symbol)

    def _transform_selected(self, symbol: int) -> None:
        obj = self.current()
        if obj is None:
            return
        step = 0.15
        if symbol == key.LEFT:
            obj.pos.x -= step
        elif symbol == key.RIGHT:
            obj.pos.x += step
        elif symbol == key.UP:
            obj.pos.z -= step
        elif symbol == key.DOWN:
            obj.pos.z += step
        elif symbol == key.PAGEUP:
            obj.pos.y += step
        elif symbol == key.PAGEDOWN:
            obj.pos.y = max(0.05, obj.pos.y - step)
        elif symbol in (key.PLUS, key.EQUAL, key.NUM_ADD):
            obj.scale = obj.scale * 1.08
        elif symbol in (key.MINUS, key.NUM_SUBTRACT):
            obj.scale = obj.scale * 0.92
        elif symbol == key.Q:
            obj.rot_y -= 7.5
        elif symbol == key.E:
            obj.rot_y += 7.5

    def current(self) -> CadObject | None:
        if 0 <= self.selected < len(self.objects):
            return self.objects[self.selected]
        return None

    def delete_selected(self) -> None:
        obj = self.current()
        if obj is None:
            return
        self.toast(f"Deleted {obj.name}")
        del self.objects[self.selected]
        self.selected = min(self.selected, len(self.objects) - 1)

    def _draw_grid(self) -> None:
        lines: list[tuple[Vec3, Vec3, tuple[float, float, float]]] = []
        for i in range(-10, 11):
            color = 0.28 if i == 0 else 0.16
            line_color = (color, color, color)
            lines.append((Vec3(i, 0, -10), Vec3(i, 0, 10), line_color))
            lines.append((Vec3(-10, 0, i), Vec3(10, 0, i), line_color))
        draw_projected_lines(self.camera, self.width, self.height, lines)

    def _draw_axes(self) -> None:
        lines = [
            (Vec3(0, 0.02, 0), Vec3(2.2, 0.02, 0), (0.9, 0.25, 0.25)),
            (Vec3(0, 0.02, 0), Vec3(0, 2.2, 0), (0.25, 0.85, 0.35)),
            (Vec3(0, 0.02, 0), Vec3(0, 0.02, 2.2), (0.3, 0.5, 1.0)),
        ]
        draw_projected_lines(self.camera, self.width, self.height, lines)

    def _draw_objects(self) -> None:
        faces = []
        selection_lines = []
        light = normalize(Vec3(-0.45, 0.9, 0.35))
        for idx, obj in enumerate(self.objects):
            color = obj.color if idx != self.selected else brighten(obj.color)
            for tri in STL_BUILDERS[obj.kind]():
                world = tuple(transform_vertex(v, obj) for v in tri)
                projected = [project_world(v, self.camera, self.width, self.height) for v in world]
                if any(p is None for p in projected):
                    continue
                n = normal(*world)
                intensity = 0.28 + 0.72 * max(0.0, dot(n, light))
                shaded = tuple(min(1.0, c * intensity) for c in color)
                depth = sum(p[2] for p in projected if p is not None) / 3
                faces.append((depth, projected, shaded))
            if idx == self.selected:
                selection_lines.extend(selection_box_lines(obj))
        faces.sort(key=lambda item: item[0], reverse=True)
        for _depth, projected, color in faces:
            draw_triangle_2d(projected, color)
        draw_projected_lines(self.camera, self.width, self.height, selection_lines)

    def _draw_overlay(self) -> None:
        lines = [
            f"Objects: {len(self.objects)}",
            f"Selected: {self.current().name if self.current() else 'none'}",
            self.message if self.message_ttl > 0 else "",
        ]
        for idx, line in enumerate(lines):
            if line:
                label = pyglet.text.Label(
                    line,
                    font_name="Consolas",
                    font_size=12,
                    x=14,
                    y=self.height - 24 - idx * 24,
                    color=(225, 231, 235, 255),
                )
                label.draw()

    def _pick(self, x: int, y: int) -> None:
        if not self.objects:
            return
        best_idx = -1
        best_dist = 999999.0
        for idx, obj in enumerate(self.objects):
            projected = project_world(obj.pos, self.camera, self.width, self.height)
            if projected is None:
                continue
            sx, sy, sz = projected
            dist = math.hypot(x - sx, y - sy)
            radius = 55 / max(0.25, sz)
            if dist < min(80, radius) and dist < best_dist:
                best_idx = idx
                best_dist = dist
        if best_idx >= 0:
            self.selected = best_idx

    def export_stl(self, path: Path) -> None:
        triangles: list[tuple[Vec3, Vec3, Vec3]] = []
        for obj in self.objects:
            builder = STL_BUILDERS[obj.kind]
            for tri in builder():
                triangles.append(tuple(transform_vertex(v, obj) for v in tri))
        with path.open("w", encoding="utf-8") as fh:
            fh.write("solid simplecad3d\n")
            for a, b, c in triangles:
                n = normal(a, b, c)
                fh.write(f"  facet normal {n.x:.6g} {n.y:.6g} {n.z:.6g}\n")
                fh.write("    outer loop\n")
                for v in (a, b, c):
                    fh.write(f"      vertex {v.x:.6g} {v.y:.6g} {v.z:.6g}\n")
                fh.write("    endloop\n")
                fh.write("  endfacet\n")
            fh.write("endsolid simplecad3d\n")
        self.toast(f"Exported {path}")


def brighten(color: tuple[float, float, float]) -> tuple[float, float, float]:
    return tuple(min(1.0, c * 1.25 + 0.08) for c in color)


def project_world(point: Vec3, camera: Camera, width: int, height: int) -> tuple[float, float, float] | None:
    eye = camera.eye()
    forward = normalize(camera.target - eye)
    right = normalize(cross(forward, Vec3(0, 1, 0)))
    up = cross(right, forward)
    rel = point - eye
    view_x = dot(rel, right)
    view_y = dot(rel, up)
    view_z = dot(rel, forward)
    if view_z <= 0.08:
        return None
    fov = math.radians(45)
    half_h = math.tan(fov / 2) * view_z
    half_w = half_h * (width / max(1, height))
    ndc_x = view_x / half_w
    ndc_y = view_y / half_h
    if abs(ndc_x) > 4 or abs(ndc_y) > 4:
        return None
    screen_x = (ndc_x + 1) * width / 2
    screen_y = (ndc_y + 1) * height / 2
    return screen_x, screen_y, view_z


def draw_projected_lines(
    camera: Camera,
    width: int,
    height: int,
    lines: list[tuple[Vec3, Vec3, tuple[float, float, float]]],
) -> None:
    vertices: list[float] = []
    colors: list[float] = []
    for a, b, color in lines:
        pa = project_world(a, camera, width, height)
        pb = project_world(b, camera, width, height)
        if pa is None or pb is None:
            continue
        vertices.extend([pa[0], pa[1], 0, pb[0], pb[1], 0])
        rgba = [color[0], color[1], color[2], 1.0]
        colors.extend(rgba + rgba)
    if vertices:
        glLineWidth(1.5)
        pyglet.graphics.draw(len(vertices) // 3, GL_LINES, position=("f", vertices), colors=("f", colors))


def draw_triangle_2d(points: list[tuple[float, float, float] | None], color: tuple[float, float, float]) -> None:
    if any(point is None for point in points):
        return
    vertices: list[float] = []
    for point in points:
        assert point is not None
        vertices.extend([point[0], point[1], 0])
    rgba = [color[0], color[1], color[2], 1.0]
    pyglet.graphics.draw(3, GL_TRIANGLES, position=("f", vertices), colors=("f", rgba * 3))


def selection_box_lines(obj: CadObject) -> list[tuple[Vec3, Vec3, tuple[float, float, float]]]:
    local = [
        Vec3(-0.58, -0.58, -0.58),
        Vec3(0.58, -0.58, -0.58),
        Vec3(0.58, -0.58, 0.58),
        Vec3(-0.58, -0.58, 0.58),
        Vec3(-0.58, 0.58, -0.58),
        Vec3(0.58, 0.58, -0.58),
        Vec3(0.58, 0.58, 0.58),
        Vec3(-0.58, 0.58, 0.58),
    ]
    verts = [transform_vertex(v, obj) for v in local]
    edge_indices = [
        (0, 1), (1, 2), (2, 3), (3, 0),
        (4, 5), (5, 6), (6, 7), (7, 4),
        (0, 4), (1, 5), (2, 6), (3, 7),
    ]
    return [(verts[a], verts[b], (1.0, 0.9, 0.18)) for a, b in edge_indices]


def set_perspective(fovy: float, aspect: float, z_near: float, z_far: float) -> None:
    ymax = z_near * math.tan(math.radians(fovy) / 2)
    xmax = ymax * aspect
    glFrustum(-xmax, xmax, -ymax, ymax, z_near, z_far)


def look_at(eye: Vec3, center: Vec3, up: Vec3) -> None:
    f = normalize(center - eye)
    s = normalize(cross(f, up))
    u = cross(s, f)
    matrix = (GLfloat * 16)(
        s.x, u.x, -f.x, 0,
        s.y, u.y, -f.y, 0,
        s.z, u.z, -f.z, 0,
        0, 0, 0, 1,
    )
    glMultMatrixf(matrix)
    glTranslatef(-eye.x, -eye.y, -eye.z)


def project_point(
    point: Vec3,
    model: "GLdouble_Array_16",
    proj: "GLdouble_Array_16",
    view: "GLint_Array_4",
) -> tuple[float, float, float] | None:
    mv = multiply_matrix_vec(model, (point.x, point.y, point.z, 1.0))
    clip = multiply_matrix_vec(proj, mv)
    if abs(clip[3]) < 1e-9:
        return None
    ndc = (clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3])
    sx = view[0] + (ndc[0] + 1) * view[2] / 2
    sy = view[1] + (ndc[1] + 1) * view[3] / 2
    sz = (ndc[2] + 1) / 2
    return sx, sy, sz


def multiply_matrix_vec(matrix: "GLdouble_Array_16", vec: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    return tuple(
        matrix[row] * vec[0]
        + matrix[4 + row] * vec[1]
        + matrix[8 + row] * vec[2]
        + matrix[12 + row] * vec[3]
        for row in range(4)
    )


def normalize(v: Vec3) -> Vec3:
    length = math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z) or 1.0
    return v * (1.0 / length)


def cross(a: Vec3, b: Vec3) -> Vec3:
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    )


def dot(a: Vec3, b: Vec3) -> float:
    return a.x * b.x + a.y * b.y + a.z * b.z


def draw_box_mesh() -> None:
    glBegin(GL_QUADS)
    faces = [
        ((0, 0, 1), [(-0.5, -0.5, 0.5), (0.5, -0.5, 0.5), (0.5, 0.5, 0.5), (-0.5, 0.5, 0.5)]),
        ((0, 0, -1), [(0.5, -0.5, -0.5), (-0.5, -0.5, -0.5), (-0.5, 0.5, -0.5), (0.5, 0.5, -0.5)]),
        ((1, 0, 0), [(0.5, -0.5, 0.5), (0.5, -0.5, -0.5), (0.5, 0.5, -0.5), (0.5, 0.5, 0.5)]),
        ((-1, 0, 0), [(-0.5, -0.5, -0.5), (-0.5, -0.5, 0.5), (-0.5, 0.5, 0.5), (-0.5, 0.5, -0.5)]),
        ((0, 1, 0), [(-0.5, 0.5, 0.5), (0.5, 0.5, 0.5), (0.5, 0.5, -0.5), (-0.5, 0.5, -0.5)]),
        ((0, -1, 0), [(-0.5, -0.5, -0.5), (0.5, -0.5, -0.5), (0.5, -0.5, 0.5), (-0.5, -0.5, 0.5)]),
    ]
    for norm, verts in faces:
        glNormal3f(*norm)
        for vert in verts:
            glVertex3f(*vert)
    glEnd()


def draw_cylinder(segments: int = 40) -> None:
    glBegin(GL_TRIANGLES)
    for tri in cylinder_triangles(segments):
        emit_triangle(tri)
    glEnd()


def draw_sphere(segments: int = 32, rings: int = 16) -> None:
    glBegin(GL_TRIANGLES)
    for tri in sphere_triangles(segments, rings):
        emit_triangle(tri)
    glEnd()


def draw_cone(segments: int = 40) -> None:
    glBegin(GL_TRIANGLES)
    for tri in cone_triangles(segments):
        emit_triangle(tri)
    glEnd()


def emit_triangle(tri: tuple[Vec3, Vec3, Vec3]) -> None:
    n = normal(*tri)
    glNormal3f(n.x, n.y, n.z)
    for v in tri:
        glVertex3f(v.x, v.y, v.z)


def wire_cube() -> None:
    edges = [
        (-0.55, -0.55, -0.55, 0.55, -0.55, -0.55),
        (0.55, -0.55, -0.55, 0.55, -0.55, 0.55),
        (0.55, -0.55, 0.55, -0.55, -0.55, 0.55),
        (-0.55, -0.55, 0.55, -0.55, -0.55, -0.55),
        (-0.55, 0.55, -0.55, 0.55, 0.55, -0.55),
        (0.55, 0.55, -0.55, 0.55, 0.55, 0.55),
        (0.55, 0.55, 0.55, -0.55, 0.55, 0.55),
        (-0.55, 0.55, 0.55, -0.55, 0.55, -0.55),
        (-0.55, -0.55, -0.55, -0.55, 0.55, -0.55),
        (0.55, -0.55, -0.55, 0.55, 0.55, -0.55),
        (0.55, -0.55, 0.55, 0.55, 0.55, 0.55),
        (-0.55, -0.55, 0.55, -0.55, 0.55, 0.55),
    ]
    glBegin(GL_LINES)
    for edge in edges:
        glVertex3f(edge[0], edge[1], edge[2])
        glVertex3f(edge[3], edge[4], edge[5])
    glEnd()


def transform_vertex(v: Vec3, obj: CadObject) -> Vec3:
    x, y, z = v.x * obj.scale.x, v.y * obj.scale.y, v.z * obj.scale.z
    yaw = math.radians(obj.rot_y)
    rx = x * math.cos(yaw) + z * math.sin(yaw)
    rz = -x * math.sin(yaw) + z * math.cos(yaw)
    return Vec3(rx + obj.pos.x, y + obj.pos.y, rz + obj.pos.z)


def normal(a: Vec3, b: Vec3, c: Vec3) -> Vec3:
    u = b - a
    v = c - a
    n = Vec3(
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x,
    )
    length = math.sqrt(n.x * n.x + n.y * n.y + n.z * n.z) or 1.0
    return n * (1.0 / length)


def box_triangles() -> list[tuple[Vec3, Vec3, Vec3]]:
    corners = {
        "lbf": Vec3(-0.5, -0.5, 0.5),
        "rbf": Vec3(0.5, -0.5, 0.5),
        "rtf": Vec3(0.5, 0.5, 0.5),
        "ltf": Vec3(-0.5, 0.5, 0.5),
        "lbb": Vec3(-0.5, -0.5, -0.5),
        "rbb": Vec3(0.5, -0.5, -0.5),
        "rtb": Vec3(0.5, 0.5, -0.5),
        "ltb": Vec3(-0.5, 0.5, -0.5),
    }
    quads = [
        ("lbf", "rbf", "rtf", "ltf"),
        ("rbb", "lbb", "ltb", "rtb"),
        ("rbf", "rbb", "rtb", "rtf"),
        ("lbb", "lbf", "ltf", "ltb"),
        ("ltf", "rtf", "rtb", "ltb"),
        ("lbb", "rbb", "rbf", "lbf"),
    ]
    return quad_tris([[corners[name] for name in quad] for quad in quads])


def cylinder_triangles(segments: int = 40) -> list[tuple[Vec3, Vec3, Vec3]]:
    tris = []
    top = Vec3(0, 0.5, 0)
    bottom = Vec3(0, -0.5, 0)
    for i in range(segments):
        a0 = math.tau * i / segments
        a1 = math.tau * (i + 1) / segments
        p0 = Vec3(0.5 * math.cos(a0), -0.5, 0.5 * math.sin(a0))
        p1 = Vec3(0.5 * math.cos(a1), -0.5, 0.5 * math.sin(a1))
        p2 = Vec3(p1.x, 0.5, p1.z)
        p3 = Vec3(p0.x, 0.5, p0.z)
        tris.extend([(p0, p1, p2), (p0, p2, p3), (bottom, p1, p0), (top, p3, p2)])
    return tris


def sphere_triangles(segments: int = 32, rings: int = 16) -> list[tuple[Vec3, Vec3, Vec3]]:
    tris = []
    radius = 0.55
    for r in range(rings):
        p0 = -math.pi / 2 + math.pi * r / rings
        p1 = -math.pi / 2 + math.pi * (r + 1) / rings
        for s in range(segments):
            a0 = math.tau * s / segments
            a1 = math.tau * (s + 1) / segments
            quad = [sphere_point(radius, p0, a0), sphere_point(radius, p0, a1), sphere_point(radius, p1, a1), sphere_point(radius, p1, a0)]
            tris.extend(quad_tris([quad]))
    return tris


def cone_triangles(segments: int = 40) -> list[tuple[Vec3, Vec3, Vec3]]:
    tris = []
    tip = Vec3(0, 0.6, 0)
    center = Vec3(0, -0.5, 0)
    for i in range(segments):
        a0 = math.tau * i / segments
        a1 = math.tau * (i + 1) / segments
        p0 = Vec3(0.55 * math.cos(a0), -0.5, 0.55 * math.sin(a0))
        p1 = Vec3(0.55 * math.cos(a1), -0.5, 0.55 * math.sin(a1))
        tris.extend([(p0, p1, tip), (center, p1, p0)])
    return tris


def sphere_point(radius: float, pitch: float, yaw: float) -> Vec3:
    cp = math.cos(pitch)
    return Vec3(radius * cp * math.cos(yaw), radius * math.sin(pitch), radius * cp * math.sin(yaw))


def quad_tris(quads: list[list[Vec3]]) -> list[tuple[Vec3, Vec3, Vec3]]:
    tris = []
    for a, b, c, d in quads:
        tris.extend([(a, b, c), (a, c, d)])
    return tris


STL_BUILDERS: dict[str, Callable[[], list[tuple[Vec3, Vec3, Vec3]]]] = {
    "box": box_triangles,
    "cylinder": cylinder_triangles,
    "sphere": sphere_triangles,
    "cone": cone_triangles,
}


if __name__ == "__main__":
    SimpleCAD()
    pyglet.app.run()
