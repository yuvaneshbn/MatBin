import os
import numpy as np

BIN_FILE = r"C:\Users\YUVANESH\Desktop\MatBin\testdata\15_75.bin"
CSV_FILE = r"C:\Users\YUVANESH\Downloads\MatBin\output\15_75.csv"

# Your BIN format
DTYPE = "<u2"   # little-endian uint16


# ------------------------------------------------------------
# Check input
# ------------------------------------------------------------

if not os.path.isfile(BIN_FILE):
    raise FileNotFoundError(f"BIN file not found: {BIN_FILE}")

os.makedirs(os.path.dirname(CSV_FILE), exist_ok=True)


# ------------------------------------------------------------
# Read the ENTIRE BIN file
# ------------------------------------------------------------

print("Reading entire BIN file...")

data = np.fromfile(BIN_FILE, dtype=DTYPE)

print(f"BIN size : {os.path.getsize(BIN_FILE):,} bytes")
print(f"Values   : {data.size:,}")


# ------------------------------------------------------------
# Write the ENTIRE dataset to CSV
# ------------------------------------------------------------

print("Writing all values to CSV...")

with open(CSV_FILE, "w", encoding="utf-8", newline="") as f:
    f.write("data\n")

    for value in data:
        f.write(f"{int(value)}\n")


# ------------------------------------------------------------
# Verify
# ------------------------------------------------------------

print()
print("============================================")
print("FULL CONVERSION COMPLETE")
print("============================================")
print(f"BIN : {BIN_FILE}")
print(f"CSV : {CSV_FILE}")
print(f"BIN values : {data.size:,}")
print(f"CSV exists : {os.path.isfile(CSV_FILE)}")
print(f"CSV size   : {os.path.getsize(CSV_FILE):,} bytes")
print("============================================")