# Medieval Castle Drawbridge Simulation - Technical Documentation

This document serves as the comprehensive technical reference manual for the **Smart Medieval Castle Drawbridge Simulation** project. It details the underlying computer graphics principles, mathematical derivations, custom rasterization algorithms, 2D affine matrix transformations, physics-based environment wave simulations, and the overall system design.

---

## 1. Project Overview & Architecture

The application is built using **C++** and **OpenGL / GLUT**. It consolidates all drawing, physics, and state updates in a single file ([main.cpp](file:///c:/Users/supan/OneDrive/DIU%20Materials/8th%20Semester/Summer%2026%20-%20Final/CSE412-Computer%20Graphics%20Lab/Medieval%20Castle%20Drawbridge/main.cpp)) designed with a **strict separation of animation logic and rendering pipelines** (**Animation Separation** course requirement):
1. **Simulation Update (`animationUpdate`)**: Triggered at $\approx 60$ FPS by GLUT’s timer callback. Updates all position coordinates, phase angles, rotation metrics, and state machines based on delta time ($dt$). No OpenGL drawing commands are called here.
2. **Rendering Pipeline (`display`)**: Triggered by frame repaints. Passively reads values from the global state struct to issue OpenGL geometric drawing commands. No physics or state updates occur during rendering.

### Global Data Structures
* **`SimState`**: The singular global state object tracks all variables:
  * `BridgeState state`: Current state inside the 8-state navigation FSM.
  * `float bridgeAngle`: The bascule lift deck angle (range: $0.0^\circ$ to $82.0^\circ$).
  * `float shipY`: Vertical position of the ship in world coordinates.
  * `float shipPhase`: Phase counter driving the ship's bobbing.
  * `float cloudOffset`, `birdPhase`, `flagPhase`, `wavePhase`: Phase timers driving environment animations.
  * `int theme`, `float themeBlend`: Dynamic cycles (Day, Sunset, Night) and transition interpolation progress.
  * `float zoomFactor`, `zoomX`, `zoomY`: Camera Ortho view bounds.
  * `int mirrorMode`: Toggles global horizontal reflection.
  * `int showDemoMode`: Toggles the F1 CG concepts checklist HUD.
* **`Theme`**: Houses RGB color configurations for the sky, ground, water, castle brick, roof, and sun/moon. Colors are interpolated during cycles.

---

## 2. Graphics Algorithms (Mathematical Implementations)

The project implements two core graphics algorithms from scratch instead of relying on default high-level library functions:

### 2.1 Midpoint Circle Algorithm (`drawCircleMidpoint`)
Used to render the **Sun**, the **Moon**, and the circular **Castle Shield Emblem** on the wall gate arch.
* **Derivation**:
  For a circle centered at $(0, 0)$ with radius $R$, the implicit equation is:
  $$f(x, y) = x^2 + y^2 - R^2 = 0$$
  We step $x$ from $0$ to $R/\sqrt{2}$ and compute a decision parameter $p$ representing the midpoint between candidate pixels:
  $$p_k = f\left(x_k + 1, y_k - \frac{1}{2}\right) = \left(x_k + 1\right)^2 + \left(y_k - \frac{1}{2}\right)^2 - R^2$$
  * Initial parameter:
    $$p_0 = \frac{5}{4} - R \approx 1 - R \quad (\text{using integer arithmetic})$$
  * If $p_k < 0$, the midpoint is inside the circle; we select the East pixel $(x_k + 1, y_k)$:
    $$p_{k+1} = p_k + 2x_{k+1} + 1$$
  * If $p_k \ge 0$, the midpoint is outside the circle; we select the South-East pixel $(x_k + 1, y_k - 1)$:
    $$p_{k+1} = p_k + 2x_{k+1} - 2y_{k+1} + 1$$
* **Scanning & Symmetrical Plotting**:
  The 8-way symmetry maps coordinates from the first octant to all other octants:
  $$\text{Points} = \{(x, y), (-x, y), (x, -y), (-x, -y), (y, x), (-y, x), (y, -x), (-y, -x)\}$$
* **Raster Scanline Filling**:
  To render filled circles (e.g. Sun, Moon, shield center), the custom implementation draws horizontal line spans between the symmetrical left and right boundary coordinates:
  $$\text{Span 1: } (-x, y) \rightarrow (x, y), \quad \text{Span 2: } (-x, -y) \rightarrow (x, -y)$$
  $$\text{Span 3: } (-y, x) \rightarrow (y, x), \quad \text{Span 4: } (-y, -x) \rightarrow (y, -x)$$
  This avoids drawing multiple hollow circles and yields a solid filled geometry.

### 2.2 Bresenham's Line Algorithm (`drawLineBresenham`)
Used to render the **lifting chains**, the **tower flagpoles**, and the **bridge deck wood plank joints**.
* **Derivation**:
  For a line with slope $0 \le m \le 1$ ($dx = x_2 - x_1$, $dy = y_2 - y_1$), the error parameter is:
  $$P_k = 2dy \cdot x - 2dx \cdot y + C$$
  * Initial parameter:
    $$P_0 = 2dy - dx$$
  * If $P_k < 0$, next point is $(x_k + 1, y_k)$, and $P_{k+1} = P_k + 2dy$.
  * If $P_k \ge 0$, next point is $(x_k + 1, y_k + 1)$, and $P_{k+1} = P_k + 2dy - 2dx$.
* **Multi-Octant Generalization**:
  To support all angles, vertical lines, and reverse directions, the implementation computes step signs:
  $$s_x = (x_2 > x_1) ? 1 : -1, \quad s_y = (y_2 > y_1) ? 1 : -1$$
  It updates coordinates using a single decision loop:
  ```cpp
  int err = dx - dy;
  while(true) {
      glVertex2f(x1, y1);
      if (x1 == x2 && y1 == y2) break;
      int e2 = 2 * err;
      if (e2 > -dy) { err -= dy; x1 += sx; }
      if (e2 <  dx) { err += dx; y1 += sy; }
  }
  ```

---

## 3. 2D Affine Mathematical Transformations

The application implements 2D geometric transformations using OpenGL’s matrix stack.

### 3.1 Translation
Moves an object from coordinate $(x, y)$ to $(x + t_x, y + t_y)$.
$$\begin{bmatrix} x' \\ y' \\ 1 \end{bmatrix} = \begin{bmatrix} 1 & 0 & t_x \\ 0 & 1 & t_y \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}$$
* **Ship**: Translates along a diagonal perspective depth path from foreground $(-8.0\text{f}, -55.0\text{f})$ to gate threshold $(0.0\text{f}, -5.0\text{f})$:
  `glTranslatef(shipX, sim.shipY, 0.0f)`
* **Clouds**: Translate horizontally across the sky:
  `glTranslatef(cloudOffset, y, 0.0f)`
* **Smoke Puffs**: Emitted from the chimney and translate upward:
  `glTranslatef(puffX, puffY, 0.0f)`

### 3.2 Rotation About an Arbitrary Pivot (Drawbridge Deck)
Rotates a body about the Z-axis at a specified pivot point $(x_p, y_p)$ by an angle $\theta$.
$$\begin{bmatrix} x' \\ y' \\ 1 \end{bmatrix} = \begin{bmatrix} 1 & 0 & x_p \\ 0 & 1 & y_p \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} \cos\theta & -\sin\theta & 0 \\ \sin\theta & \cos\theta & 0 \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} 1 & 0 & -x_p \\ 0 & 1 & -y_p \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}$$
The drawbridge hinge is located on the left bank at $x_p = -15.0\text{f}$, $y_p = -20.0\text{f}$. To rotate the deck:
```cpp
glTranslatef(-15.0f, -20.0f, 0.0f);        // 3. Translate back to hinge
glRotatef(-sim.bridgeAngle, 0.0f, 0.0f, 1.0f); // 2. Rotate deck counterclockwise
glTranslatef(15.0f, 20.0f, 0.0f);          // 1. Translate hinge to origin
```

### 3.3 Scaling
Scales an object’s dimensions by factor $s_x$ and $s_y$.
$$\begin{bmatrix} x' \\ y' \\ 1 \end{bmatrix} = \begin{bmatrix} s_x & 0 & 0 \\ 0 & s_y & 0 \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}$$
* **Ship Perspective Scaling**: Used to simulate 3D depth in a 2D viewport. As the ship's vertical coordinate (`sim.shipY`) approaches the background gate, its scale shrinks from $1.0\text{f}$ down to $0.01\text{f}$ using the perspective formula:
  `glScalef(shipScale, shipScale, 1.0f)`

### 3.4 2D Reflection (Mirror Mode)
Pressing **`M`** applies a reflection matrix transformation across the vertical Y-axis.
$$\begin{bmatrix} x' \\ y' \\ 1 \end{bmatrix} = \begin{bmatrix} -1 & 0 & 0 \\ 0 & 1 & 0 \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}$$
This transformation is applied to the root modelview matrix:
```cpp
if (sim.mirrorMode) {
    glScalef(-1.0f, 1.0f, 1.0f);
}
```
This mirrors the entire environment (castle, drawbridge, river, waves, sun, moon, ship) horizontally, while keeping the HUD readable.

---

## 4. Physics and Mathematical Simulations

### 4.1 Cloth Flag Physics (Travelling Wave)
The flags on the castle towers and the ship mast wave realistically. The wave is modelled as a travelling sine wave:
$$y(x, t) = A \cdot x \cdot \sin(\omega \cdot t - k \cdot x)$$
* $A = 0.8\text{f}$ (maximum flag wave amplitude).
* $x \in [0, 1]$ (normalized flag segment length. Multiplied directly by amplitude so the wave starts at $0.0\text{f}$ at the flagpole joint and reaches maximum amplitude at the tip).
* $\omega = 3.0\text{f}$ (wave frequency, driven by `sim.flagPhase`).
* $k = 5.0\text{f}$ (spatial frequency determining wave frequency along the flag).

### 4.2 Scrolling Water Wave Ripples
Perspective ripples are drawn across the river canal as lines. The y-coordinate bobs according to:
$$y(x, t) = y_{\text{center}} + \text{amplitude} \cdot \sin(x \cdot 0.4\text{f} + \text{wavePhase} \cdot 1.5\text{f})$$
Wave amplitude is scaled down for distant ripples (`amp * (1.0 - v * 0.7)`) to simulate perspective.

### 4.3 Ship Bobbing
The ship bobs on the water by applying a tiny rotation around the hull center:
$$\theta_{\text{bob}} = 1.5^\circ \cdot \sin(\text{shipPhase})$$
This is implemented in `drawShip` as:
```cpp
glTranslatef(0.0f, -10.0f, 0.0f);
glRotatef(1.5f * sinf(sim.shipPhase), 0.0f, 0.0f, 1.0f);
glTranslatef(0.0f, 10.0f, 0.0f);
```

---

## 5. Projection & Aspect Ratio Calculations

### Aspect-Ratio Reshaping (`reshape` and `setupProjection`)
Prevents object stretching when the user resizes the window.
* **Design Aspect Ratio**: $1.67$ (width $200.0\text{f}$ from $-100.0\text{f}$ to $100.0\text{f}$, height $120.0\text{f}$ from $-60.0\text{f}$ to $60.0\text{f}$).
* **Letterboxing/Pillarboxing Calculation**:
  When window width `w` and height `h` change:
  $$\text{aspect} = \frac{w}{h}$$
  * If $\text{aspect} > 1.67$ (wider window):
    $$\text{viewW} = 120.0\text{f} \cdot \text{aspect}, \quad \text{viewH} = 120.0\text{f}$$
    This adds side margins (**pillarboxes**).
  * If $\text{aspect} < 1.67$ (taller window):
    $$\text{viewW} = 200.0\text{f}, \quad \text{viewH} = \frac{200.0\text{f}}{\text{aspect}}$$
    This adds top/bottom margins (**letterboxes**).
  * Projection bounds are configured under Ortho2D centered on zoom offsets:
    ```cpp
    gluOrtho2D(sim.zoomX - viewW / (2 * zoom), sim.zoomX + viewW / (2 * zoom),
               sim.zoomY - viewH / (2 * zoom), sim.zoomY + viewH / (2 * zoom));
    ```

---

## 6. Finite State Machine (FSM) & Bounding Box Logic

The ship and drawbridge animations are synchronized through an 8-state FSM in `updateBridgeStateMachine`:

| State | Name | Description |
|---|---|---|
| **`0`** | `APPROACHING` | Ship sails forward. Bridge remains closed. |
| **`1`** | `WAITING_FOR_BRIDGE` | Ship stops at `APPROACH_LINE` ($-28.0\text{f}$). Bridge begins opening. |
| **`2`** | `OPENING` | Bridge deck rotates up towards $82.0^\circ$. |
| **`3`** | `OPEN` | Bridge reaches $82.0^\circ$. Transition to ship passing. |
| **`4`** | `PASSING_UNDER_BRIDGE` | Ship sails through the bridge canal. Bridge is locked open. |
| **`5`** | `CLEARED_BRIDGE` | Ship's rear clears `CLEAR_LINE` ($-15.0\text{f}$). Bridge remains open. |
| **`6`** | `CLOSING` | Ship Y coordinate reaches $15.0\text{f}$ (inside castle). Bridge closes. |
| **`7`** | `RESET` | Bridge deck reaches $0.0^\circ$. Short delay before ship respawns. |

### Bounding Box Intersection Logic
To prevent collision between the ship mast and the bridge deck, rotation is frozen if any part of the ship's bounding box is inside the channel bounds:
$$\text{Channel Range} = [x_{\text{start}}, x_{\text{end}}] = [-25.0\text{f}, -15.0\text{f}]$$
$$\text{Ship Range} = [y_{\text{rear}}, y_{\text{front}}] = [\text{shipY} - 20 \cdot \text{scale}, \text{shipY} + 5 \cdot \text{scale}]$$
* **Overlap Condition**:
  $$\text{Overlap} = (y_{\text{front}} \ge x_{\text{start}}) \land (y_{\text{rear}} \le x_{\text{end}})$$
  If `Overlap` is true, bridge rotation is frozen.

### Painter's Algorithm (Occlusion Depth-Sorting)
Occlusion order is swapped based on the ship's position:
* If $y_{\text{ship}} < -20.0\text{f}$ (foreground):
  `drawCastleBackground` $\rightarrow$ `drawCastleForeground` $\rightarrow$ `drawBridge` $\rightarrow$ `drawShip` $\rightarrow$ `drawTrees`
* If $y_{\text{ship}} \ge -20.0\text{f}$ (inside castle):
  `drawCastleBackground` $\rightarrow$ `drawShip` $\rightarrow$ `drawCastleForeground` $\rightarrow$ `drawBridge` $\rightarrow$ `drawTrees`

---

## 7. Functions Reference Directory

### Main Application Lifecycle
* **`int main(int argc, char** argv)`**
  Initializes GLUT window size, position, display modes (`Double Buffer`, `RGB`), sets callbacks, and starts the main event loop.
* **`void initGL()`**
  Sets the clear color to black (`0.0f, 0.0f, 0.0f, 1.0f`), configures 2D alpha blending, and enables anti-aliasing hints.
* **`void initSim()`**
  Resets all `SimState` variables (angle, positions, timers, theme, scale) to their default initial states.
* **`void reshape(int w, int h)`**
  Caches active window dimensions and updates viewport coordinates.
* **`void timer(int value)`**
  A recurring GLUT timer callback (60 FPS) that advances the simulation clock.

### Core Drawing Pipeline
* **`void display()`**
  Coordinates matrix stacks, projection bounds, background clear buffers, global reflection scale, depth layers (Painter's Algorithm), HUD rendering, and buffer swapping.
* **`void drawScene(const Theme*)`**
  Renders sky gradient, moon/sun, stars, cloud meshes, perspective grass ground banks, and birds.
* **`void drawRiver(const Theme*)`**
  Renders the blue water canal and scrolling wave ripples.
* **`void drawBridge(const Theme*)`**
  Renders road segments, gallows post, wood deck planks, and lifting chains.
* **`void drawShip(const Theme*)`**
  Renders stern cabin details, glowing windows, mast, sail, flags, steam chimneys, and rising smoke puffs.
* **`void drawTrees()`**
  Draws perspective-scaled landscape trees on top of castle walls.
* **`void drawHUD()`**
  Renders screen-space textual state information (speed, zoom, active state).
* **`void drawDemoOverlay()`**
  Renders the F1 menu checklist detailing implemented CG course concepts with live active status checkmarks.

### Input Management
* **`void keyboard(unsigned char key, int x, int y)`**
  Processes alphanumeric keystrokes:
  * `Space`: Pause/resume simulation.
  * `R` / `r`: Reset simulation to initial state.
  * `D` / `d`: Cycle Day $\rightarrow$ Sunset $\rightarrow$ Night theme transitions.
  * `M` / `m`: Toggle horizontal Mirror Mode.
  * `Z` / `z`: Zoom camera viewport into the bridge.
  * `X` / `x`: Reset camera viewport.
  * `+` / `=`: Speed up simulation.
  * `-` / `_`: Slow down simulation.
* **`void specialKeys(int key, int x, int y)`**
  Captures functional key presses, mapping `F1` to toggle the CG concepts overlay.
* **`void initInput()`**
  Registers normal keyboard and special key callback listeners with GLUT.
