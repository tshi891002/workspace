import math
import time
import tkinter as tk
from tkinter import ttk


TAU = math.tau


def identity():
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def matmul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def translate(x=0.0, y=0.0, z=0.0):
    m = identity()
    m[0][3], m[1][3], m[2][3] = x, y, z
    return m


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, c, -s, 0.0],
        [0.0, s, c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return [
        [c, 0.0, s, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [-s, 0.0, c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return [
        [c, -s, 0.0, 0.0],
        [s, c, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def transform_point(m, p=(0.0, 0.0, 0.0)):
    x, y, z = p
    return (
        m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
        m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
        m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3],
    )


def forward_kinematics(degrees):
    q = [math.radians(v) for v in degrees]
    lengths = [1.15, 1.0, 0.48, 0.28]
    base_height = 0.35

    t = identity()
    frames = [t]
    joints = [transform_point(t)]

    steps = [
        translate(z=base_height),
        rot_z(q[0]),
        rot_y(q[1]),
        translate(x=lengths[0]),
        rot_y(q[2]),
        translate(x=lengths[1]),
        rot_x(q[3]),
        translate(x=lengths[2]),
        rot_y(q[4]),
        translate(x=lengths[3]),
        rot_z(q[5]),
    ]

    for step in steps:
        t = matmul(t, step)
        frames.append(t)
        if step[0][3] or step[1][3] or step[2][3]:
            joints.append(transform_point(t))

    tool = transform_point(t, (0.22, 0.0, 0.0))
    joints.append(tool)
    frames.append(t)
    return joints, frames


class RobotArmApp:
    def __init__(self, root):
        self.root = root
        self.root.title("6 DOF Robot Arm Simulator")
        self.root.minsize(1050, 680)

        self.joint_values = [0.0, 25.0, -40.0, 0.0, 20.0, 0.0]
        self.joint_limits = [(-180, 180), (-100, 100), (-135, 135), (-180, 180), (-120, 120), (-180, 180)]
        self.joint_names = ["J1 base yaw", "J2 shoulder", "J3 elbow", "J4 wrist roll", "J5 wrist pitch", "J6 tool yaw"]
        self.link_colors = ["#2f80ed", "#00a676", "#f2a541", "#d14d72", "#7b61ff"]

        self.yaw = math.radians(-42)
        self.pitch = math.radians(22)
        self.zoom = 245.0
        self.center = (0.0, 0.0, 0.75)
        self.drag_start = None
        self.playing = False
        self.trail = []
        self.start_time = time.perf_counter()

        self._build_ui()
        self._bind_events()
        self.draw()
        self.tick()

    def _build_ui(self):
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        self.canvas = tk.Canvas(self.root, bg="#10141b", highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="nsew")

        panel = ttk.Frame(self.root, padding=14)
        panel.grid(row=0, column=1, sticky="ns")

        ttk.Label(panel, text="6 DOF Robot Arm", font=("Segoe UI", 15, "bold")).grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(panel, text="Drag view   Mouse wheel zoom", foreground="#555").grid(row=1, column=0, columnspan=3, sticky="w", pady=(0, 12))

        self.scales = []
        for idx, (name, limits, value) in enumerate(zip(self.joint_names, self.joint_limits, self.joint_values), start=2):
            ttk.Label(panel, text=name).grid(row=idx * 2, column=0, columnspan=3, sticky="w", pady=(6, 0))
            scale = ttk.Scale(panel, from_=limits[0], to=limits[1], orient="horizontal", length=260, command=lambda v, i=idx - 2: self.set_joint(i, v))
            scale.set(value)
            scale.grid(row=idx * 2 + 1, column=0, columnspan=2, sticky="ew")
            label = ttk.Label(panel, text=f"{value:6.1f} deg", width=10, anchor="e")
            label.grid(row=idx * 2 + 1, column=2, padx=(8, 0))
            self.scales.append((scale, label))

        buttons = ttk.Frame(panel)
        buttons.grid(row=16, column=0, columnspan=3, sticky="ew", pady=(18, 10))
        ttk.Button(buttons, text="Play", command=self.toggle_play).grid(row=0, column=0, padx=(0, 6))
        ttk.Button(buttons, text="Home", command=self.home).grid(row=0, column=1, padx=6)
        ttk.Button(buttons, text="Clear Trail", command=self.clear_trail).grid(row=0, column=2, padx=6)

        self.readout = ttk.Label(panel, text="", justify="left", font=("Consolas", 10))
        self.readout.grid(row=17, column=0, columnspan=3, sticky="ew", pady=(8, 0))

        ttk.Separator(panel).grid(row=18, column=0, columnspan=3, sticky="ew", pady=14)
        ttk.Label(panel, text="Controls", font=("Segoe UI", 10, "bold")).grid(row=19, column=0, columnspan=3, sticky="w")
        ttk.Label(panel, text="Each slider rotates one revolute joint.\nThe colored frames show wrist/tool orientation.\nThe yellow path is the end-effector trail.", foreground="#555").grid(row=20, column=0, columnspan=3, sticky="w")

    def _bind_events(self):
        self.canvas.bind("<Configure>", lambda _event: self.draw())
        self.canvas.bind("<ButtonPress-1>", self.begin_drag)
        self.canvas.bind("<B1-Motion>", self.drag)
        self.canvas.bind("<MouseWheel>", self.zoom_view)
        self.root.bind("<space>", lambda _event: self.toggle_play())

    def set_joint(self, index, raw_value):
        self.joint_values[index] = float(raw_value)
        self.scales[index][1].configure(text=f"{self.joint_values[index]:6.1f} deg")
        self.playing = False
        self.draw()

    def toggle_play(self):
        self.playing = not self.playing
        self.start_time = time.perf_counter()

    def home(self):
        self.playing = False
        home_values = [0.0, 25.0, -40.0, 0.0, 20.0, 0.0]
        for i, value in enumerate(home_values):
            self.scales[i][0].set(value)
            self.joint_values[i] = value
            self.scales[i][1].configure(text=f"{value:6.1f} deg")
        self.clear_trail()
        self.draw()

    def clear_trail(self):
        self.trail.clear()
        self.draw()

    def begin_drag(self, event):
        self.drag_start = (event.x, event.y, self.yaw, self.pitch)

    def drag(self, event):
        if not self.drag_start:
            return
        sx, sy, yaw, pitch = self.drag_start
        self.yaw = yaw + (event.x - sx) * 0.008
        self.pitch = max(math.radians(-75), min(math.radians(75), pitch + (event.y - sy) * 0.008))
        self.draw()

    def zoom_view(self, event):
        self.zoom = max(110.0, min(520.0, self.zoom * (1.0 + (-event.delta / 1200.0))))
        self.draw()

    def tick(self):
        if self.playing:
            t = time.perf_counter() - self.start_time
            values = [
                75 * math.sin(t * 0.70),
                -20 + 42 * math.sin(t * 0.95),
                55 + 36 * math.sin(t * 1.10 + 1.3),
                130 * math.sin(t * 1.35),
                32 * math.sin(t * 1.45 + 0.5),
                180 * ((t * 0.12) % 2.0 - 1.0),
            ]
            for i, value in enumerate(values):
                lo, hi = self.joint_limits[i]
                value = max(lo, min(hi, value))
                self.joint_values[i] = value
                self.scales[i][0].set(value)
                self.scales[i][1].configure(text=f"{value:6.1f} deg")
            self.draw(add_trail=True)
        self.root.after(30, self.tick)

    def camera_point(self, p):
        x, y, z = p[0] - self.center[0], p[1] - self.center[1], p[2] - self.center[2]
        cy, sy = math.cos(self.yaw), math.sin(self.yaw)
        cp, sp = math.cos(self.pitch), math.sin(self.pitch)
        x, y = cy * x - sy * y, sy * x + cy * y
        y, z = cp * y - sp * z, sp * y + cp * z
        return x, y, z

    def project(self, p):
        w = max(1, self.canvas.winfo_width())
        h = max(1, self.canvas.winfo_height())
        x, y, z = self.camera_point(p)
        depth = 6.0 + y
        factor = self.zoom / max(1.8, depth)
        return w * 0.5 + x * factor, h * 0.55 - z * factor, y

    def draw_grid(self):
        extent = 2.6
        step = 0.4
        values = [round(-extent + i * step, 2) for i in range(int(extent * 2 / step) + 1)]
        for v in values:
            for a, b in [((-extent, v, 0), (extent, v, 0)), ((v, -extent, 0), (v, extent, 0))]:
                x1, y1, _ = self.project(a)
                x2, y2, _ = self.project(b)
                self.canvas.create_line(x1, y1, x2, y2, fill="#27313f", width=1)
        self.draw_axis((0, 0, 0), (1.2, 0, 0), "#f45b69", "X")
        self.draw_axis((0, 0, 0), (0, 1.2, 0), "#4ecdc4", "Y")
        self.draw_axis((0, 0, 0), (0, 0, 1.2), "#ffe66d", "Z")

    def draw_axis(self, start, end, color, label):
        x1, y1, _ = self.project(start)
        x2, y2, _ = self.project(end)
        self.canvas.create_line(x1, y1, x2, y2, fill=color, width=2, arrow=tk.LAST)
        self.canvas.create_text(x2, y2, text=label, fill=color, font=("Segoe UI", 9, "bold"))

    def draw_frame(self, frame, length=0.22):
        origin = transform_point(frame)
        axes = [
            (transform_point(frame, (length, 0, 0)), "#f45b69"),
            (transform_point(frame, (0, length, 0)), "#4ecdc4"),
            (transform_point(frame, (0, 0, length)), "#ffe66d"),
        ]
        ox, oy, _ = self.project(origin)
        for end, color in axes:
            ex, ey, _ = self.project(end)
            self.canvas.create_line(ox, oy, ex, ey, fill=color, width=2, arrow=tk.LAST)

    def draw(self, add_trail=False):
        self.canvas.delete("all")
        joints, frames = forward_kinematics(self.joint_values)
        tool = joints[-1]
        if add_trail:
            self.trail.append(tool)
            self.trail = self.trail[-220:]

        self.draw_grid()
        self.draw_trail()

        segments = []
        for i in range(len(joints) - 1):
            avg_depth = (self.camera_point(joints[i])[1] + self.camera_point(joints[i + 1])[1]) * 0.5
            segments.append((avg_depth, i, joints[i], joints[i + 1]))
        for _depth, i, start, end in sorted(segments):
            x1, y1, _ = self.project(start)
            x2, y2, _ = self.project(end)
            self.canvas.create_line(x1, y1, x2, y2, fill="#080b10", width=18, capstyle=tk.ROUND)
            self.canvas.create_line(x1, y1, x2, y2, fill=self.link_colors[i % len(self.link_colors)], width=12, capstyle=tk.ROUND)

        for i, joint in enumerate(joints):
            x, y, depth = self.project(joint)
            radius = max(6, min(15, 11 - depth * 0.5))
            fill = "#edf2f4" if i < len(joints) - 1 else "#ffe66d"
            self.canvas.create_oval(x - radius, y - radius, x + radius, y + radius, fill=fill, outline="#080b10", width=2)
            self.canvas.create_text(x, y - radius - 10, text=str(i), fill="#d8dee9", font=("Segoe UI", 8, "bold"))

        for frame in frames[-3:]:
            self.draw_frame(frame)

        self.draw_readout(tool)

    def draw_trail(self):
        if len(self.trail) < 2:
            return
        points = []
        for p in self.trail:
            x, y, _ = self.project(p)
            points.extend([x, y])
        self.canvas.create_line(*points, fill="#ffe66d", width=2, smooth=True)

    def draw_readout(self, tool):
        reach = math.sqrt(sum(v * v for v in tool))
        self.readout.configure(
            text=(
                "End effector\n"
                f"X {tool[0]: .3f} m\n"
                f"Y {tool[1]: .3f} m\n"
                f"Z {tool[2]: .3f} m\n"
                f"Reach {reach: .3f} m"
            )
        )


def main():
    root = tk.Tk()
    try:
        root.call("tk", "scaling", 1.2)
    except tk.TclError:
        pass
    RobotArmApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
