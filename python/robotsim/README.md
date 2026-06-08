# 6 DOF Robot Arm Simulator

A dependency-free Python 3D robot arm simulator. It uses Tkinter for the app window, a small perspective renderer, and forward kinematics for six revolute joints.

## Run

```powershell
py robot_arm_sim.py
```

If `py` is not available, use:

```powershell
python robot_arm_sim.py
```

## Controls

- Drag the 3D view to orbit the camera.
- Use the mouse wheel to zoom.
- Move the six joint sliders to pose the arm.
- Press `Play` or the spacebar to animate the joints.
- Use `Home` to reset the arm and `Clear Trail` to remove the tool path.

## Files

- `robot_arm_sim.py` - main app and forward kinematics implementation.
