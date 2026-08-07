# 🏰 Pseudo-3D Medieval Castle Drawbridge Simulation

A **Computer Graphics** project built with **C++ / OpenGL (GLUT)** for CSE412.

This simulation models a medieval castle with a navigable river channel flowing from background to foreground. A horizontal drawbridge crosses the river canal, connecting roads on both banks. A sailing ship approaches from the foreground, stops, wait for the drawbridge to raise, sails under, vanishes inside the castle gatehouse, and the bridge closes behind it.

> This is a *graphics demonstration*, not a game. Every feature exists to show a specific Computer Graphics concept taught in the course.

---

## ✨ Features & Graphics Concepts Demonstrated

| Concept | Implementation Details |
|---|---|
| **Pseudo-3D Layout** | River canal, grass banks, and trees are drawn in perspective, shrinking towards the center gate. |
| **Depth Sorting** | Implements dynamic depth sorting based on ship Y position: when the ship is in the foreground, it is drawn in front of the road/bridge. As it sails past $y \ge -20\text{f}$, it transitions to being drawn behind the castle foreground walls, sliding naturally inside the gatehouse corridor. |
| **Horizontal bascule drawbridge** | The bridge crosses the river horizontally ($x \in [-15\text{f}, 15\text{f}]$) at $y = -20\text{f}$, connecting roads on the banks. It is hinged on the left bank and rotates counterclockwise to stand vertical on the side, keeping the navigation channel completely clear. |
| **Gallows post & chains** | A vertical wooden post on the left bank holds two iron chains. The chains dynamically adjust their length and angle to follow the outer corners of the bridge deck as it raises. |
| **Stern-view ship** | Redesigned the ship to show a realistic back-view (symmetrical wooden hull, centered cabin with two windows that glow at night, horizontal yardarm carrying a square sail, and steam chimney blowing smoke puffs). |
| **Detailed gatehouse interior** | The castle background corridor features a stone-paved floor, perspective side walls with brick joints, a glowing lantern with alpha-blended light halo, and a raised iron portcullis hanging from the ceiling. |
| **Aspect-ratio reshaping** | Window reshaping tracks window dimensions dynamically and preserves the $1.67$ scene aspect ratio without stretching, adding clean letterboxing/pillarboxing automatically. |
| **Translation & Rotation** | Ship sailing (`shipY`), clouds drifting (`cloudOffset`), birds gliding, smoke puffs rising, and flag waving. |
| **Color themes** | `D` key cycles **Day → Sunset → Night** with smooth color blending of sky, river, castle, and window glows. |
| **Window → Viewport** | `Z` zooms into the bridge, `X` returns — implemented by changing the ortho world window while keeping the pixel viewport aligned to the window. |

---

## 🎮 Controls

| Key | Action |
|---|---|
| `Space` | Pause / resume |
| `R` / `r` | Reset animation to initial state |
| `D` / `d` | Cycle Day → Sunset → Night → Day (smooth blend) |
| `Z` / `z` | Zoom into the bridge (viewport transform) |
| `X` / `x` | Reset to full scene |
| `+` / `=` | Increase animation speed |
| `-` / `_` | Decrease animation speed |

---

## 🧭 Finite State Machine (FSM)

The ship + bridge run on an 8-state finite state machine with strict navigation channel safety:

```
APPROACHING → WAITING_FOR_BRIDGE → OPENING → OPEN → PASSING_UNDER_BRIDGE → CLEARED_BRIDGE → CLOSING → RESET
```

1. **APPROACHING** — ship sails forward toward the closed bridge.
2. **WAITING_FOR_BRIDGE** — ship stops at a safe distance (`APPROACH_LINE = -28.0f`).
3. **OPENING** — bridge rotates up around its left hinge.
4. **OPEN** — bridge reaches its fully open angle.
5. **PASSING_UNDER_BRIDGE** — ship sails through the channel. Bridge is locked open.
6. **CLEARED_BRIDGE** — ship continues moving into the castle until it has completely vanished from sight (`sim.shipY >= 15.0f`).
7. **CLOSING** — bridge rotates back down. Ship remains stopped inside the castle.
8. **RESET** — ship resets to the foreground and the cycle repeats.

---

## 🔨 Building & Running

The project is structured as a clean single-file source codebase `main.cpp` for easy compilation.

### Windows (GCC / MinGW / build.bat)
A batch script `build.bat` is included to automatically compile and launch the simulation asynchronously using GCC:
```cmd
.\build.bat
```

### Windows (Visual Studio)
1. Create a new **Empty C++ project**.
2. Add `main.cpp` to the project.
3. Install freeglut (`vcpkg install freeglut` or download the binaries).
4. Link `freeglut.lib` + `opengl32.lib` (and add the include path).
5. Copy `freeglut.dll` next to your `.exe`.
6. Build & run.

### Linux
```bash
sudo apt install freeglut3-dev
g++ main.cpp -o castle_sim -lglut -lGL -lGLU
./castle_sim
```

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](file:///c:/Users/supan/OneDrive/DIU%20Materials/8th%20Semester/Summer%2026%20-%20Final/CSE412-Computer%20Graphics%20Lab/Medieval%20Castle%20Drawbridge/LICENSE) file for details.
