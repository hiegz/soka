# fmt: off

import os
import sys
import numpy as np
import matplotlib.pyplot as plt

# ----------------------------
# Read user input
# ----------------------------

usage = f"Usage {os.path.basename(__file__)} <file.png>"

if len(sys.argv) != 2:
    print(usage)
    sys.exit(0)

out = sys.argv[1]

if not out.endswith(".png"):
    print("output file must be a .png")
    print(usage)
    sys.exit(1)

# ----------------------------
# Render particles
# ----------------------------

N         = 100  # number of particles
BOX       = 10.0 # box size
positions = np.random.rand(N, 2) * BOX

fig, ax = plt.subplots(figsize=(6, 6))

ax.set_xlim(0, BOX)
ax.set_ylim(0, BOX)
ax.set_aspect("equal")
ax.set_xticks([])
ax.set_yticks([])

ax.scatter(positions[:, 0], positions[:, 1], s=70)

plt.savefig(out, dpi=200)
plt.close()

print(f"Saved to {out}")
