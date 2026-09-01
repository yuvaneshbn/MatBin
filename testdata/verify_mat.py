import os
import sys
import numpy as np
from scipy.io import loadmat

# ============================================================
# Configuration
# ============================================================

MAT_FILE = r"C:\Users\YUVANESH\Desktop\MatBin\testdata\15_75.mat"
TXT_FILE = r"C:\Users\YUVANESH\Desktop\MatBin\testdata\15_75_mat_contents.txt"

# Variable to export
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

print("Reading MAT file...")
print(f"File: {MAT_FILE}")

try:
    mat = loadmat(MAT_FILE)
except Exception as e:
    print("ERROR: Could not read MAT file:")
    print(e)
    sys.exit(1)


# ============================================================
# Show variables
# ============================================================

print()
print("Variables found in MAT file:")

for name, value in mat.items():
    if not name.startswith("__"):
        print(
            f"  {name}: "
            f"shape={value.shape}, "
            f"dtype={value.dtype}"
        )


# ============================================================
# Check requested variable
# ============================================================

if VARIABLE_NAME not in mat:
    print()
    print(f"ERROR: Variable '{VARIABLE_NAME}' not found.")
    sys.exit(1)


data = np.asarray(mat[VARIABLE_NAME])


# ============================================================
# Create output directory
# ============================================================

output_dir = os.path.dirname(TXT_FILE)

if output_dir:
    os.makedirs(output_dir, exist_ok=True)


# ============================================================
# Write complete MAT data to TXT
# ============================================================

print()
print("Writing complete MAT data to TXT...")

with open(TXT_FILE, "w", encoding="utf-8") as f:

    f.write("MAT FILE CONTENTS\n")
    f.write("=" * 70 + "\n")
    f.write(f"Source MAT file : {MAT_FILE}\n")
    f.write(f"Variable        : {VARIABLE_NAME}\n")
    f.write(f"Shape           : {data.shape}\n")
    f.write(f"Data type       : {data.dtype}\n")
    f.write(f"Total elements  : {data.size:,}\n")
    f.write("\n")

    f.write("DATA\n")
    f.write("=" * 70 + "\n")

    # --------------------------------------------------------
    # Preserve 2D structure
    # --------------------------------------------------------

    if data.ndim == 2:

        for row in data:
            f.write(
                " ".join(
                    str(value)
                    for value in row
                )
            )
            f.write("\n")

    else:

        for value in data.ravel():
            f.write(f"{value}\n")


# ============================================================
# Result
# ============================================================

print()
print("=" * 70)
print("CONVERSION COMPLETE")
print("=" * 70)

print(f"MAT file       : {MAT_FILE}")
print(f"Variable       : {VARIABLE_NAME}")
print(f"Shape          : {data.shape}")
print(f"Data type      : {data.dtype}")
print(f"Elements       : {data.size:,}")
print(f"TXT file       : {TXT_FILE}")
print(f"TXT size       : {os.path.getsize(TXT_FILE):,} bytes")

print("=" * 70)