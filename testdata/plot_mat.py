import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import loadmat

# ============================================================
# Configuration
# ============================================================

MAT_FILE = r"C:\Users\YUVANESH\Downloads\MatBin\output\15_75.mat"
VARIABLE_NAME = "data"

# ============================================================
# Check MAT file
# ============================================================

if not os.path.isfile(MAT_FILE):
    print("ERROR: MAT file not found:")
    print(MAT_FILE)
    sys.exit(1)

# ============================================================
# Load MAT file
# ============================================================

print("Loading MAT file...")

try:
    mat = loadmat(MAT_FILE)
except Exception as e:
    print("ERROR: Could not read MAT file:")
    print(e)
    sys.exit(1)

if VARIABLE_NAME not in mat:
    print(f"ERROR: Variable '{VARIABLE_NAME}' not found.")
    print("Available variables:")

    for name in mat:
        if not name.startswith("__"):
            print(f"  {name}")

    sys.exit(1)

# ============================================================
# Extract data
# ============================================================

data = np.asarray(mat[VARIABLE_NAME]).ravel()

print()
print("============================================")
print("MAT DATA")
print("============================================")
print(f"Variable : {VARIABLE_NAME}")
print(f"Elements : {data.size:,}")
print(f"Dtype    : {data.dtype}")
print(f"Minimum  : {data.min()}")
print(f"Maximum  : {data.max()}")
print(f"Mean     : {data.mean():.3f}")

# ============================================================
# Plot full dataset
# ============================================================

plt.figure(figsize=(14, 6))

plt.plot(data)

plt.title("MAT File Data")
plt.xlabel("Sample Index")
plt.ylabel("Value")
plt.grid(True)

plt.tight_layout()
plt.show()