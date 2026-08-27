import os
import sys
import numpy as np
from scipy.io import loadmat

# ============================================================
# Configuration
# ============================================================

BIN_FILE = r"C:\Users\YUVANESH\Desktop\MatBin\testdata\15_75.bin"
MAT_FILE = r"C:\Users\YUVANESH\Downloads\MatBin\output\15_75.mat"

VARIABLE_NAME = "data"

# BIN format used by your test
BIN_DTYPE = "<u2"   # little-endian uint16


# ============================================================
# Check files
# ============================================================

if not os.path.isfile(BIN_FILE):
    print("ERROR: BIN file not found:")
    print(BIN_FILE)
    sys.exit(1)

if not os.path.isfile(MAT_FILE):
    print("ERROR: MAT file not found:")
    print(MAT_FILE)
    sys.exit(1)


# ============================================================
# Read BIN
# ============================================================

print("Reading BIN file...")
print(f"BIN: {BIN_FILE}")

bin_size = os.path.getsize(BIN_FILE)

if bin_size % 2 != 0:
    print(
        f"ERROR: BIN size ({bin_size:,} bytes) is not divisible by 2."
    )
    sys.exit(1)

bin_data = np.fromfile(BIN_FILE, dtype=BIN_DTYPE)


# ============================================================
# Read MAT
# ============================================================

print("Reading MAT file...")
print(f"MAT: {MAT_FILE}")

try:
    mat = loadmat(MAT_FILE)
except Exception as e:
    print("ERROR: Could not read MAT file:")
    print(e)
    sys.exit(1)


# ============================================================
# Check variable
# ============================================================

if VARIABLE_NAME not in mat:
    print(f"ERROR: MAT variable '{VARIABLE_NAME}' not found.")

    print("Available variables:")
    for name in mat:
        if not name.startswith("__"):
            print(f"  {name}")

    sys.exit(1)


mat_data = np.asarray(mat[VARIABLE_NAME]).ravel()


# ============================================================
# Display information
# ============================================================

print()
print("============================================")
print("FILE INFORMATION")
print("============================================")

print(f"BIN size       : {bin_size:,} bytes")
print(f"BIN elements   : {bin_data.size:,}")
print(f"BIN dtype      : {bin_data.dtype}")

print()

print(f"MAT variable   : {VARIABLE_NAME}")
print(f"MAT shape      : {mat[VARIABLE_NAME].shape}")
print(f"MAT elements   : {mat_data.size:,}")
print(f"MAT dtype      : {mat_data.dtype}")
print(f"MAT file size  : {os.path.getsize(MAT_FILE):,} bytes")


# ============================================================
# Compare number of elements
# ============================================================

print()
print("============================================")
print("SIZE CHECK")
print("============================================")

if bin_data.size != mat_data.size:
    print("FAIL: Number of elements is different.")
    print(f"BIN: {bin_data.size:,}")
    print(f"MAT: {mat_data.size:,}")
    sys.exit(1)

print("PASS: Number of elements matches.")


# ============================================================
# Compare data type
# ============================================================

print()
print("============================================")
print("DATA TYPE CHECK")
print("============================================")

if bin_data.dtype != mat_data.dtype:
    print("WARNING: NumPy dtypes differ.")
    print(f"BIN dtype: {bin_data.dtype}")
    print(f"MAT dtype: {mat_data.dtype}")
else:
    print("PASS: Data type matches.")


# ============================================================
# Compare every value
# ============================================================

print()
print("============================================")
print("DATA COMPARISON")
print("============================================")

same = np.array_equal(bin_data, mat_data)

if same:
    print("PASS: ALL VALUES MATCH.")
else:
    print("FAIL: DATA MISMATCH DETECTED.")

    # Find mismatching positions
    mismatch_indices = np.flatnonzero(bin_data != mat_data)

    print(f"Total mismatches: {mismatch_indices.size:,}")

    # Show first 20 mismatches
    print()
    print("First mismatches:")

    for index in mismatch_indices[:20]:
        print(
            f"Index {index:,}: "
            f"BIN={bin_data[index]}  "
            f"MAT={mat_data[index]}"
        )


# ============================================================
# First / last samples
# ============================================================

print()
print("============================================")
print("SAMPLE CHECK")
print("============================================")

print("First 20 BIN values:")
print(bin_data[:20])

print()
print("First 20 MAT values:")
print(mat_data[:20])

print()
print("Last 20 BIN values:")
print(bin_data[-20:])

print()
print("Last 20 MAT values:")
print(mat_data[-20:])


# ============================================================
# Final result
# ============================================================

print()
print("============================================")
print("FINAL RESULT")
print("============================================")

if same and bin_data.size == mat_data.size:
    print("✅ CONVERSION VERIFIED")
    print("The MAT file contains the same values as the BIN file.")
    sys.exit(0)
else:
    print("❌ CONVERSION FAILED")
    print("The MAT file does not exactly match the BIN file.")
    sys.exit(1)