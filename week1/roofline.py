import numpy as np
import matplotlib.pyplot as plt

PEAK   = 1440.0   # GFLOP/s, NEON all-core peak (verify FMA-unit count!)
BW_L2  = 120.0    # GB/s, measured --bw L2 plateau
BW_DR  = 100.0    # GB/s, measured --bw DRAM plateau

ai = np.logspace(-1, 3, 400)               # 0.1 .. 1000 FLOP/byte
roof_dram = np.minimum(PEAK, BW_DR * ai)
roof_l2   = np.minimum(PEAK, BW_L2 * ai)

fig, ax = plt.subplots(figsize=(7, 5))
ax.loglog(ai, roof_dram, label="DRAM 100 GB/s", color="#D85A30")
ax.loglog(ai, roof_l2,   label="L2 120 GB/s", color="#1D9E75", ls="--")
ax.axhline(PEAK, color="#888780", ls=":", label="compute peak 1440")

# measured points (predicted AI, MEASURED GFLOP/s)
ax.scatter(0.25, 34.8, color="#E24B4A", zorder=5, label="naive (meas 34.8)")
ax.scatter(128,  29.0, color="#378ADD", marker="D", zorder=5, label="blocked B=512 (meas 29.0)")
# predicted blocked point, for contrast
ax.scatter(128, PEAK, facecolors="none", edgecolors="#378ADD", marker="D", zorder=5,
           label="blocked (predicted, peak)")

ax.set_xlabel("operational intensity (FLOP/byte)")
ax.set_ylabel("attainable GFLOP/s")
ax.set_ylim(10, 2000)
ax.legend(fontsize=8, loc="lower right")
ax.set_title("matmul roofline — N=2048, M4 Max (predicted vs measured)")
fig.tight_layout()
fig.savefig("roofline.png", dpi=150)