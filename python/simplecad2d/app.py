import json
import math
import tkinter as tk
from dataclasses import asdict, dataclass
from pathlib import Path
from tkinter import filedialog, messagebox, simpledialog, ttk
from typing import Any


GRID_SIZE = 20
HANDLE_SIZE = 5
SELECT_HIT_RADIUS = 8
MAX_UNDO_STEPS = 50

STROKE_COLORS = ["#111827", "#2563eb", "#16a34a", "#dc2626", "#f59e0b"]
TOOLS = [
    ("Select", "select"),
    ("Line", "line"),
    ("Arrow", "arrow"),
    ("Rect", "rect"),
    ("Circle", "circle"),
    ("Oval", "ellipse"),
    ("Arc", "arc"),
    ("Tri", "triangle"),
    ("Diamond", "diamond"),
    ("Text", "text"),
]


@dataclass
class Shape:
    kind: str
    x1: float
    y1: float
    x2: float
    y2: float
    stroke: str = "#111827"
    width: int = 2
    text: str = ""

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Shape":
        required = {"kind", "x1", "y1", "x2", "y2"}
        missing = required - data.keys()
        if missing:
            raise ValueError(f"Shape is missing fields: {', '.join(sorted(missing))}")

        return cls(
            kind=str(data["kind"]),
            x1=float(data["x1"]),
            y1=float(data["y1"]),
            x2=float(data["x2"]),
            y2=float(data["y2"]),
            stroke=str(data.get("stroke", "#111827")),
            width=int(data.get("width", 2)),
            text=str(data.get("text", "")),
        )

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    def moved(self, dx: float, dy: float) -> "Shape":
        return Shape(
            self.kind,
            self.x1 + dx,
            self.y1 + dy,
            self.x2 + dx,
            self.y2 + dy,
            self.stroke,
            self.width,
            self.text,
        )

    def bounds(self) -> tuple[float, float, float, float]:
        if self.kind == "circle":
            radius = math.hypot(self.x2 - self.x1, self.y2 - self.y1)
            return self.x1 - radius, self.y1 - radius, self.x1 + radius, self.y1 + radius

        if self.kind == "text":
            font_size = self.text_size()
            width = max(40, len(self.text) * max(7, font_size * 0.55))
            height = max(18, font_size * 1.4)
            return self.x1, self.y1, self.x1 + width, self.y1 + height

        return (
            min(self.x1, self.x2),
            min(self.y1, self.y2),
            max(self.x1, self.x2),
            max(self.y1, self.y2),
        )

    def triangle_points(self) -> tuple[float, float, float, float, float, float]:
        x1, y1, x2, y2 = self.bounds()
        return (x1 + (x2 - x1) / 2, y1, x2, y2, x1, y2)

    def diamond_points(self) -> tuple[float, float, float, float, float, float, float, float]:
        x1, y1, x2, y2 = self.bounds()
        mid_x = x1 + (x2 - x1) / 2
        mid_y = y1 + (y2 - y1) / 2
        return (mid_x, y1, x2, mid_y, mid_x, y2, x1, mid_y)

    def text_size(self) -> int:
        return max(9, self.width * 5)


class SimpleCAD(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("SimpleCAD 2D")
        self.geometry("1100x720")
        self.minsize(860, 560)

        self.shapes: list[Shape] = []
        self.item_to_shape: dict[int, int] = {}
        self.selected_index: int | None = None
        self.preview_item: int | None = None
        self.drag_start: tuple[float, float] | None = None
        self.move_origin: tuple[float, float, Shape] | None = None
        self.move_snapshot_taken = False
        self.undo_stack: list[list[dict[str, Any]]] = []

        self.tool = tk.StringVar(value="select")
        self.snap_to_grid = tk.BooleanVar(value=True)
        self.show_grid = tk.BooleanVar(value=True)
        self.stroke = tk.StringVar(value="#111827")
        self.line_width = tk.IntVar(value=2)
        self.status_text = tk.StringVar(value="Ready")

        self.canvas: tk.Canvas
        self._build_ui()
        self._bind_events()
        self._draw_grid()

    def _build_ui(self):
        root = ttk.Frame(self)
        root.pack(fill=tk.BOTH, expand=True)

        toolbar = ttk.Frame(root, padding=(8, 8, 8, 4))
        toolbar.pack(side=tk.TOP, fill=tk.X)
        tool_row = ttk.Frame(toolbar)
        tool_row.pack(side=tk.TOP, fill=tk.X)
        command_row = ttk.Frame(toolbar)
        command_row.pack(side=tk.TOP, fill=tk.X, pady=(6, 0))

        self._build_tool_buttons(tool_row)
        self._build_style_controls(command_row)
        ttk.Separator(command_row, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        self._build_command_buttons(command_row)

        body = ttk.Frame(root)
        body.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(body, bg="#ffffff", highlightthickness=1, highlightbackground="#d1d5db")
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=8, pady=(4, 0))

        status = ttk.Frame(root, padding=(8, 4))
        status.pack(side=tk.BOTTOM, fill=tk.X)
        ttk.Label(status, textvariable=self.status_text).pack(side=tk.LEFT)
        ttk.Label(status, text="Tip: drag to draw; use Text to place labels").pack(side=tk.RIGHT)

    def _build_tool_buttons(self, parent):
        for label, value in TOOLS:
            ttk.Radiobutton(
                parent,
                text=label,
                value=value,
                variable=self.tool,
                command=self._clear_selection,
            ).pack(side=tk.LEFT, padx=(0, 8))

    def _build_style_controls(self, parent):
        ttk.Label(parent, text="Stroke").pack(side=tk.LEFT)
        for color in STROKE_COLORS:
            tk.Button(
                parent,
                width=2,
                bg=color,
                activebackground=color,
                command=lambda c=color: self.stroke.set(c),
            ).pack(side=tk.LEFT, padx=2)

        ttk.Label(parent, text="Width").pack(side=tk.LEFT, padx=(12, 4))
        ttk.Spinbox(parent, from_=1, to=10, width=4, textvariable=self.line_width).pack(side=tk.LEFT)
        ttk.Checkbutton(parent, text="Snap", variable=self.snap_to_grid).pack(side=tk.LEFT, padx=(12, 0))
        ttk.Checkbutton(parent, text="Grid", variable=self.show_grid, command=self._redraw_all).pack(
            side=tk.LEFT,
            padx=(8, 0),
        )

    def _build_command_buttons(self, parent):
        for label, command in [
            ("Undo", self.undo),
            ("Delete", self.delete_selected),
            ("Clear", self.clear),
        ]:
            ttk.Button(parent, text=label, command=command).pack(side=tk.LEFT, padx=2)

        for label, command in [
            ("Save", self.save_file),
            ("Load", self.load_file),
            ("Export", self.export_postscript),
        ]:
            ttk.Button(parent, text=label, command=command).pack(side=tk.RIGHT, padx=2)

    def _bind_events(self):
        self.canvas.bind("<Configure>", lambda _event: self._redraw_all())
        self.canvas.bind("<ButtonPress-1>", self.on_press)
        self.canvas.bind("<B1-Motion>", self.on_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_release)
        self.canvas.bind("<Motion>", self.on_motion)
        self.bind("<Delete>", lambda _event: self.delete_selected())
        self.bind("<Control-z>", lambda _event: self.undo())
        self.bind("<Control-s>", lambda _event: self.save_file())
        self.bind("<Control-o>", lambda _event: self.load_file())

    def canvas_point(self, event):
        x = self.canvas.canvasx(event.x)
        y = self.canvas.canvasy(event.y)
        return self._snap_point(x, y)

    def on_motion(self, event):
        x, y = self.canvas_point(event)
        self.status_text.set(f"{self.tool.get().title()} | x={int(x)} y={int(y)}")

    def on_press(self, event):
        x, y = self.canvas_point(event)
        self.drag_start = (x, y)

        active_tool = self.tool.get()
        if active_tool == "select":
            self._start_selection(event.x, event.y, x, y)
            return

        self._clear_selection()
        if active_tool != "text":
            self.preview_item = self._create_canvas_shape(
                self._new_shape(active_tool, x, y, x, y),
                tags=("preview",),
                dash=(4, 3),
            )

    def on_drag(self, event):
        if self.drag_start is None:
            return

        x, y = self.canvas_point(event)
        if self.tool.get() == "select":
            self._move_selected_to(x, y)
            return

        if self.tool.get() != "text":
            self._update_preview(x, y)

    def on_release(self, event):
        if self.drag_start is None:
            return

        x, y = self.canvas_point(event)
        active_tool = self.tool.get()

        if active_tool == "select":
            self.move_origin = None
            self.move_snapshot_taken = False
        elif active_tool == "text":
            self._place_text(x, y)
        else:
            self._finish_draw(active_tool, x, y)

        self.drag_start = None

    def _start_selection(self, event_x: float, event_y: float, x: float, y: float):
        self.select_at(event_x, event_y)
        if self.selected_index is None:
            return

        shape = self.shapes[self.selected_index]
        self.move_origin = (x, y, shape)
        self.move_snapshot_taken = False

    def _move_selected_to(self, x: float, y: float):
        if self.selected_index is None or self.move_origin is None:
            return

        if not self.move_snapshot_taken:
            self._snapshot()
            self.move_snapshot_taken = True

        start_x, start_y, original_shape = self.move_origin
        self.shapes[self.selected_index] = original_shape.moved(x - start_x, y - start_y)
        self._redraw_shapes()
        self._draw_selection()

    def _update_preview(self, x: float, y: float):
        if self.preview_item is not None:
            self.canvas.delete(self.preview_item)

        start_x, start_y = self.drag_start
        self.preview_item = self._create_canvas_shape(
            self._new_shape(self.tool.get(), start_x, start_y, x, y),
            tags=("preview",),
            dash=(4, 3),
        )

    def _finish_draw(self, tool: str, x: float, y: float):
        start_x, start_y = self.drag_start
        if self._is_meaningful_drag(start_x, start_y, x, y):
            self._snapshot()
            self.shapes.append(self._new_shape(tool, start_x, start_y, x, y))

        self._delete_preview()
        self._redraw_shapes()

    def _place_text(self, x: float, y: float):
        label = simpledialog.askstring("Text", "Label text:", parent=self)
        if not label:
            return

        self._snapshot()
        self.shapes.append(self._new_shape("text", x, y, x, y, label))
        self._redraw_shapes()

    def _new_shape(self, kind: str, x1: float, y1: float, x2: float, y2: float, text: str = ""):
        return Shape(kind, x1, y1, x2, y2, self.stroke.get(), self.line_width.get(), text)

    def select_at(self, x: float, y: float):
        self._clear_selection()
        nearby_items = self.canvas.find_overlapping(
            x - SELECT_HIT_RADIUS,
            y - SELECT_HIT_RADIUS,
            x + SELECT_HIT_RADIUS,
            y + SELECT_HIT_RADIUS,
        )
        for item in reversed(nearby_items):
            index = self.item_to_shape.get(item)
            if index is not None:
                self.selected_index = index
                self._draw_selection()
                return

    def _snapshot(self):
        self.undo_stack.append([shape.to_dict() for shape in self.shapes])
        if len(self.undo_stack) > MAX_UNDO_STEPS:
            self.undo_stack.pop(0)

    def undo(self):
        if not self.undo_stack:
            return
        self.shapes = [Shape.from_dict(shape) for shape in self.undo_stack.pop()]
        self._clear_selection()
        self._redraw_shapes()

    def delete_selected(self):
        if self.selected_index is None:
            return
        self._snapshot()
        del self.shapes[self.selected_index]
        self._clear_selection()
        self._redraw_shapes()

    def clear(self):
        if not self.shapes:
            return
        self._snapshot()
        self.shapes.clear()
        self._clear_selection()
        self._redraw_shapes()

    def save_file(self):
        path = filedialog.asksaveasfilename(
            title="Save drawing",
            defaultextension=".json",
            filetypes=[("CAD drawing", "*.json"), ("All files", "*.*")],
        )
        if not path:
            return

        Path(path).write_text(
            json.dumps([shape.to_dict() for shape in self.shapes], indent=2),
            encoding="utf-8",
        )
        self.status_text.set(f"Saved {Path(path).name}")

    def load_file(self):
        path = filedialog.askopenfilename(
            title="Load drawing",
            filetypes=[("CAD drawing", "*.json"), ("All files", "*.*")],
        )
        if not path:
            return

        try:
            raw_shapes = json.loads(Path(path).read_text(encoding="utf-8"))
            if not isinstance(raw_shapes, list):
                raise ValueError("Drawing file must contain a list of shapes.")

            self._snapshot()
            self.shapes = [Shape.from_dict(shape) for shape in raw_shapes]
            self._clear_selection()
            self._redraw_shapes()
            self.status_text.set(f"Loaded {Path(path).name}")
        except (OSError, TypeError, ValueError, json.JSONDecodeError) as exc:
            messagebox.showerror("Load failed", str(exc))

    def export_postscript(self):
        path = filedialog.asksaveasfilename(
            title="Export drawing",
            defaultextension=".ps",
            filetypes=[("PostScript", "*.ps"), ("All files", "*.*")],
        )
        if not path:
            return
        self.canvas.postscript(file=path, colormode="color")
        self.status_text.set(f"Exported {Path(path).name}")

    def _redraw_all(self):
        self.canvas.delete("all")
        self.item_to_shape.clear()
        self._draw_grid()
        self._redraw_shapes(clear=False)
        self._draw_selection()

    def _redraw_shapes(self, clear=True):
        if clear:
            self.canvas.delete("shape")
            self.canvas.delete("selection")
            self.item_to_shape.clear()

        for index, shape in enumerate(self.shapes):
            item = self._create_canvas_shape(shape, tags=("shape",))
            self.item_to_shape[item] = index

    def _create_canvas_shape(self, shape: Shape, tags=(), dash=None):
        renderers = {
            "line": self._draw_line,
            "arrow": self._draw_arrow,
            "rect": self._draw_rectangle,
            "circle": self._draw_circle,
            "ellipse": self._draw_ellipse,
            "arc": self._draw_arc,
            "triangle": self._draw_triangle,
            "diamond": self._draw_diamond,
            "text": self._draw_text,
        }

        try:
            return renderers[shape.kind](shape, tags, dash)
        except KeyError as exc:
            raise ValueError(f"Unknown shape kind: {shape.kind}") from exc

    def _draw_line(self, shape: Shape, tags, dash):
        return self.canvas.create_line(
            shape.x1,
            shape.y1,
            shape.x2,
            shape.y2,
            fill=shape.stroke,
            width=shape.width,
            tags=tags,
            dash=dash,
        )

    def _draw_arrow(self, shape: Shape, tags, dash):
        return self.canvas.create_line(
            shape.x1,
            shape.y1,
            shape.x2,
            shape.y2,
            fill=shape.stroke,
            width=shape.width,
            tags=tags,
            dash=dash,
            arrow=tk.LAST,
            arrowshape=(14, 18, 6),
        )

    def _draw_rectangle(self, shape: Shape, tags, dash):
        return self.canvas.create_rectangle(*shape.bounds(), **self._outline_options(shape, tags, dash))

    def _draw_circle(self, shape: Shape, tags, dash):
        return self.canvas.create_oval(*shape.bounds(), **self._outline_options(shape, tags, dash))

    def _draw_ellipse(self, shape: Shape, tags, dash):
        return self.canvas.create_oval(*shape.bounds(), **self._outline_options(shape, tags, dash))

    def _draw_arc(self, shape: Shape, tags, dash):
        return self.canvas.create_arc(
            *shape.bounds(),
            start=25,
            extent=230,
            style=tk.ARC,
            **self._outline_options(shape, tags, dash),
        )

    def _draw_triangle(self, shape: Shape, tags, dash):
        return self.canvas.create_polygon(
            shape.triangle_points(),
            fill="",
            outline=shape.stroke,
            width=shape.width,
            tags=tags,
            dash=dash,
        )

    def _draw_diamond(self, shape: Shape, tags, dash):
        return self.canvas.create_polygon(
            shape.diamond_points(),
            fill="",
            outline=shape.stroke,
            width=shape.width,
            tags=tags,
            dash=dash,
        )

    def _draw_text(self, shape: Shape, tags, _dash):
        return self.canvas.create_text(
            shape.x1,
            shape.y1,
            anchor=tk.NW,
            text=shape.text,
            fill=shape.stroke,
            font=("Segoe UI", shape.text_size()),
            tags=tags,
        )

    def _outline_options(self, shape: Shape, tags, dash=None):
        options = {"outline": shape.stroke, "width": shape.width, "tags": tags}
        if dash:
            options["dash"] = dash
        return options

    def _draw_grid(self):
        if not self.show_grid.get():
            return

        width = max(self.canvas.winfo_width(), 1)
        height = max(self.canvas.winfo_height(), 1)
        for x in range(0, width, GRID_SIZE):
            self.canvas.create_line(x, 0, x, height, fill="#eef2f7", tags=("grid",))
        for y in range(0, height, GRID_SIZE):
            self.canvas.create_line(0, y, width, y, fill="#eef2f7", tags=("grid",))

    def _draw_selection(self):
        self.canvas.delete("selection")
        if self.selected_index is None:
            return

        shape = self.shapes[self.selected_index]
        x1, y1, x2, y2 = shape.bounds()
        self.canvas.create_rectangle(
            x1,
            y1,
            x2,
            y2,
            outline="#0ea5e9",
            width=1,
            dash=(3, 2),
            tags=("selection",),
        )
        for x, y in [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]:
            self.canvas.create_rectangle(
                x - HANDLE_SIZE,
                y - HANDLE_SIZE,
                x + HANDLE_SIZE,
                y + HANDLE_SIZE,
                fill="#0ea5e9",
                outline="#ffffff",
                tags=("selection",),
            )

    def _clear_selection(self):
        self.selected_index = None
        self.canvas.delete("selection")

    def _delete_preview(self):
        if self.preview_item is not None:
            self.canvas.delete(self.preview_item)
            self.preview_item = None

    def _snap_point(self, x: float, y: float) -> tuple[float, float]:
        if not self.snap_to_grid.get():
            return x, y
        return round(x / GRID_SIZE) * GRID_SIZE, round(y / GRID_SIZE) * GRID_SIZE

    def _is_meaningful_drag(self, x1: float, y1: float, x2: float, y2: float) -> bool:
        return abs(x2 - x1) > 1 or abs(y2 - y1) > 1


if __name__ == "__main__":
    app = SimpleCAD()
    app.mainloop()
