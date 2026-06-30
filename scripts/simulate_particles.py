# fmt: off

import sys
import os
from   tqdm import tqdm
import numpy as np
import matplotlib.pyplot as plt
from   matplotlib.animation import FuncAnimation, PillowWriter

def bar(iterable=None, total=None, desc=None):
    return tqdm(iterable, total=total, desc=desc, ascii=" =", bar_format="{desc} [{bar:50}] {n_fmt}/{total_fmt}")

# ----------------------------
# Temperature & Cooling
# ----------------------------

T_MIN  = 0.05
T_MAX  = 10.0
T0     = None # user-defined
T_END  = None # user-defined

def time_warp(i, k = 50):
    return 1 / (1 + np.exp(-k * (i - 0.5)))

def linear_cooling(i):
    return T0 * (1 - i) + T_END * i

def no_cooling(i):
    _ = i
    return T0

# ----------------------------
# Read user input
# ----------------------------

usage = f"Usage {os.path.basename(__file__)} <hot|cold|hot-to-cold> <animation-file.gif>"

if len(sys.argv) != 3:
    print(usage)
    sys.exit(0)

mode = sys.argv[1]
out  = sys.argv[2]

if mode == "hot":
    T0    = T_MAX
    T_END = T_MAX
    mode  = no_cooling

elif mode == "cold":
    T0    = T_MIN
    T_END = T_MIN
    mode  = no_cooling

elif mode == "hot-to-cold":
    T0    = T_MAX
    T_END = T_MIN
    mode  = linear_cooling

else:
    print(f"Unknown mode: {mode}")
    print(usage)
    sys.exit(1)

if not out.endswith(".gif"):
    print("animation file must be a .gif")
    print(usage)
    sys.exit(1)

# ----------------------------
# Simulation parameters
# ----------------------------

np.random.seed()

N       = 100  # number of particles
EQ      = 300  # equilibration time
BOX     = 10.0 # box size
DT      = 0.03 # timestep
FPS     = 60
SECONDS = 10
FRAMES  = FPS * SECONDS

# ----------------------------
# Initial particle positions
# ----------------------------

positions   = np.random.rand (N, 2) * BOX
velocities  = np.random.randn(N, 2) * T0

def speed():
    return np.linalg.norm(velocities, axis=1)

def kinetic_energy():
    return 0.5 * np.mean(np.sum(velocities**2, axis=1))

# ----------------------------
# Figure
# ----------------------------

fig, ax = plt.subplots(figsize=(6, 6))

fig.subplots_adjust(left=0, right=1, top=1, bottom=0)

ax.set_xlim(0, BOX)
ax.set_ylim(0, BOX)
ax.set_aspect("equal")
ax.set_xticks([])
ax.set_yticks([])

scatter = ax.scatter(
    positions[:, 0],
    positions[:, 1],
    s=70,
    cmap="coolwarm",
    c=speed(),
    vmin=T_MIN,
    vmax=T_MAX
)

info = ax.text(
    0.98, 0.98, "",
    transform=ax.transAxes,
    ha="right",
    va="top",
    fontsize=12,
    color="white",
    bbox=dict(
        boxstyle="round,pad=0.35,rounding_size=0.15",
        facecolor=(0, 0, 0, 0.65),  # semi-transparent dark panel
        edgecolor=(1, 1, 1, 0.15),
        linewidth=1,
    ),
)

# ----------------------------
# Force computation
# ----------------------------

def compute_forces(pos):
    forces = np.zeros_like(pos)

    for i in range(N):
        for j in range(i + 1, N):
            r    = pos[j] - pos[i]
            dist = np.linalg.norm(r)
            dist = max(dist, 0.15)

            direction = r / dist
            strength  = 1.2 / dist**2 # Repulsive force

            f = strength * direction

            forces[i] -= f
            forces[j] += f

    return forces

# ----------------------------
# Animation update
# ----------------------------

def step(i, cooling_schedule):
    global positions, velocities

    gamma       = 0.5 # damping
    t           = cooling_schedule(time_warp(i))
    forces      = compute_forces(positions)
    velocities += (forces - gamma * velocities) * DT
    sigma       = np.sqrt(2 * gamma * t / DT)
    velocities += np.random.randn(N, 2) * sigma * DT
    positions  += velocities * DT

    for dim in [0, 1]:
        lo = positions[:, dim] < 0
        hi = positions[:, dim] > BOX
        ob = lo | hi

        velocities[ob, dim] *= -1
        positions [:,  dim]  = np.clip(positions[:, dim], 0, BOX)

    scatter.set_offsets(positions)
    scatter.set_array(speed())

    info.set_text(f"$T = {t:.2f}$, $E_\\text{{avg}} = {kinetic_energy():.2f}$")

    return scatter, info

def update(frame, cooling_schedule):
    return step(frame / (FRAMES - 1), cooling_schedule)

# ----------------------------
# Equilibration
# ----------------------------

for i in bar(range(EQ), desc="Equilibrating"):
    step(i, no_cooling)

# ----------------------------
# Create animation
# ----------------------------

ani = FuncAnimation(
    fig,
    lambda frame: update(frame, mode),
    frames=FRAMES,
    interval=30,
    blit=False,
)

# ----------------------------
# Save Animation
# ----------------------------

class Writer(PillowWriter):
    def setup(self, fig, outfile, dpi=None):
        super().setup(fig, outfile, dpi)
        self.bar = bar(total=FRAMES, desc="Rendering    ")
        self.counter = 1;

    def grab_frame(self, **kwargs):
        super().grab_frame(**kwargs)
        self.bar.update()
        self.counter += 1

    def finish(self):
        self.bar.close()
        print("Flushing ...")
        super().finish()

ani.save(
    out,
    writer=Writer(fps=FPS),
    dpi=200,
)

print(f"Animation saved to {out}")
