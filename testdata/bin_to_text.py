import os
import sys
import hashlib
import re
import math
import numpy as np

# ============================================================
# Configuration
# ============================================================

BIN_FILE = r"C:\Users\YUVANESH\Desktop\MatBin\testdata\15_75.bin"

REPORT_FILE = (
    r"C:\Users\YUVANESH\Desktop\MatBin\testdata"
    r"\15_75_format_report.txt"
)


# The program automatically determines the matrix dimensions.
# These are NOT input dimensions.
#
# We only limit how many factor pairs are printed.
MAX_DIMENSION_CANDIDATES = 100

# Amount of data used for statistical format detection.
SAMPLE_SIZE = 1024 * 1024

# HEX dump size.
HEX_BYTES = 256


# ============================================================
# FILE UTILITIES
# ============================================================

def read_bytes(path, offset=0, size=None):

    with open(path, "rb") as f:

        if offset:
            f.seek(offset)

        if size is None:
            return f.read()

        return f.read(size)


def calculate_sha256(path):

    sha256 = hashlib.sha256()

    with open(path, "rb") as f:

        while True:

            block = f.read(1024 * 1024)

            if not block:
                break

            sha256.update(block)

    return sha256.hexdigest()


# ============================================================
# HEX DUMP
# ============================================================

def make_hex_dump(data, width=16):

    lines = []

    for offset in range(0, len(data), width):

        chunk = data[offset:offset + width]

        hex_part = " ".join(
            f"{b:02X}" for b in chunk
        )

        ascii_part = "".join(
            chr(b) if 32 <= b <= 126 else "."
            for b in chunk
        )

        lines.append(
            f"{offset:08X}  "
            f"{hex_part:<47}  "
            f"|{ascii_part}|"
        )

    return "\n".join(lines)


# ============================================================
# ASCII STRING DETECTION
# ============================================================

def find_strings(data, minimum_length=4):

    pattern = rb"[\x20-\x7E]{%d,}" % minimum_length

    return [
        m.group().decode(
            "ascii",
            errors="replace"
        )
        for m in re.finditer(pattern, data)
    ]


# ============================================================
# POSSIBLE DIMENSIONS
# ============================================================

def find_dimensions(element_count):

    dimensions = []

    root = math.isqrt(element_count)

    for rows in range(1, root + 1):

        if element_count % rows == 0:

            cols = element_count // rows

            dimensions.append(
                (rows, cols)
            )

    return dimensions


def select_likely_dimensions(dimensions):

    if not dimensions:
        return []

    # Prefer square matrices.
    square = [
        d for d in dimensions
        if d[0] == d[1]
    ]

    if square:
        return square

    # Otherwise prefer dimensions that are reasonably balanced.
    ranked = sorted(
        dimensions,
        key=lambda d: abs(d[0] - d[1])
    )

    return ranked[:MAX_DIMENSION_CANDIDATES]


# ============================================================
# NUMERIC FORMAT ANALYSIS
# ============================================================

FORMATS = {

    "uint8": "<u1",
    "int8": "<i1",

    "uint16 little-endian": "<u2",
    "uint16 big-endian": ">u2",

    "int16 little-endian": "<i2",
    "int16 big-endian": ">i2",

    "uint32 little-endian": "<u4",
    "uint32 big-endian": ">u4",

    "int32 little-endian": "<i4",
    "int32 big-endian": ">i4",

    "float32 little-endian": "<f4",
    "float32 big-endian": ">f4",

    "float64 little-endian": "<f8",
    "float64 big-endian": ">f8",
}


def analyze_format(data, format_name):

    dtype = np.dtype(
        FORMATS[format_name]
    )

    usable = (
        len(data)
        -
        (len(data) % dtype.itemsize)
    )

    if usable <= 0:
        return None

    values = np.frombuffer(
        data[:usable],
        dtype=dtype
    )

    numeric = values.astype(
        np.float64
    )

    finite = np.isfinite(
        numeric
    )

    finite_ratio = float(
        finite.mean()
    )

    if values.size > 1:

        differences = np.abs(
            np.diff(numeric)
        )

        median_difference = float(
            np.median(differences)
        )

        mean_difference = float(
            np.mean(differences)
        )

    else:

        median_difference = 0
        mean_difference = 0

    return {

        "dtype": dtype,

        "count": values.size,

        "min": values.min().item(),

        "max": values.max().item(),

        "mean": float(values.mean()),

        "median": float(
            np.median(values)
        ),

        "std": float(
            values.std()
        ),

        "unique": int(
            np.unique(values).size
        ),

        "finite_ratio": finite_ratio,

        "median_difference":
            median_difference,

        "mean_difference":
            mean_difference,

        "values": values,
    }


# ============================================================
# FORMAT SCORE
# ============================================================

def format_score(name, result):

    if result is None:
        return -1

    score = 0.0

    # Valid numeric values.
    score += (
        result["finite_ratio"] * 30
    )

    # Data should not be completely constant.
    if result["unique"] > 1:
        score += 20

    # Prefer reasonable numeric ranges.
    minimum = result["min"]
    maximum = result["max"]

    if np.isfinite(minimum) and np.isfinite(maximum):

        if maximum != minimum:
            score += 20

    # Special preference for the format already strongly
    # supported by your 15_75.bin test.
    if name == "uint16 little-endian":

        if 0 <= minimum <= 65535:
            score += 20

        if 0 <= maximum <= 65535:
            score += 10

    return score


# ============================================================
# PAYLOAD OFFSET SEARCH
# ============================================================

def test_offsets(file_size):

    offsets = set()

    # Common alignment/header sizes.
    common = [
        0,
        1,
        2,
        4,
        8,
        16,
        32,
        64,
        128,
        256,
        512,
        1024,
        2048,
        4096,
        8192,
        16384,
        32768,
        65536,
    ]

    for offset in common:

        if offset < file_size:
            offsets.add(offset)

    return sorted(offsets)


def offset_matrix_matches(file_size, offset):

    remaining = file_size - offset

    if remaining < 1:
        return None

    # Test common element sizes.
    for bytes_per_element, name in [
        (1, "8-bit"),
        (2, "16-bit"),
        (4, "32-bit"),
        (8, "64-bit"),
    ]:

        if remaining % bytes_per_element == 0:

            count = (
                remaining //
                bytes_per_element
            )

            dimensions = find_dimensions(
                count
            )

            if dimensions:

                return (
                    name,
                    count,
                    dimensions
                )

    return None


# ============================================================
# MAIN
# ============================================================

def main():

    # --------------------------------------------------------
    # Check input
    # --------------------------------------------------------

    if not os.path.isfile(BIN_FILE):

        print(
            "ERROR: BIN file not found:"
        )

        print(BIN_FILE)

        sys.exit(1)

    os.makedirs(
        os.path.dirname(REPORT_FILE),
        exist_ok=True
    )

    # --------------------------------------------------------
    # File information
    # --------------------------------------------------------

    file_size = os.path.getsize(
        BIN_FILE
    )

    sha256 = calculate_sha256(
        BIN_FILE
    )

    # --------------------------------------------------------
    # Read beginning
    # --------------------------------------------------------

    first_bytes = read_bytes(
        BIN_FILE,
        0,
        min(HEX_BYTES, file_size)
    )

    # --------------------------------------------------------
    # Read end
    # --------------------------------------------------------

    last_offset = max(
        0,
        file_size - HEX_BYTES
    )

    last_bytes = read_bytes(
        BIN_FILE,
        last_offset,
        HEX_BYTES
    )

    # --------------------------------------------------------
    # Statistical sample
    # --------------------------------------------------------

    sample = read_bytes(
        BIN_FILE,
        0,
        min(
            SAMPLE_SIZE,
            file_size
        )
    )

    # --------------------------------------------------------
    # Build report
    # --------------------------------------------------------

    report = []

    report.append(
        "=" * 78
    )

    report.append(
        "AUTOMATIC BIN FILE FORMAT ANALYZER"
    )

    report.append(
        "=" * 78
    )

    report.append("")

    report.append(
        f"File       : {BIN_FILE}"
    )

    report.append(
        f"File size  : {file_size:,} bytes"
    )

    report.append(
        f"SHA-256    : {sha256}"
    )

    report.append("")

    # ========================================================
    # HEADER / HEX
    # ========================================================

    report.append(
        "1. FIRST 256 BYTES"
    )

    report.append(
        "-" * 78
    )

    report.append(
        make_hex_dump(first_bytes)
    )

    report.append("")

    report.append(
        "2. LAST 256 BYTES"
    )

    report.append(
        "-" * 78
    )

    report.append(
        make_hex_dump(last_bytes)
    )

    report.append("")

    # ========================================================
    # STRINGS
    # ========================================================

    report.append(
        "3. POSSIBLE METADATA / TEXT STRINGS"
    )

    report.append(
        "-" * 78
    )

    strings = find_strings(
        sample
    )

    if strings:

        for string in strings[:100]:

            report.append(
                f"  {string}"
            )

    else:

        report.append(
            "  No printable ASCII strings detected."
        )

    report.append("")

    # ========================================================
    # DATATYPE ANALYSIS
    # ========================================================

    report.append(
        "4. AUTOMATIC DATATYPE ANALYSIS"
    )

    report.append(
        "-" * 78
    )

    results = []

    for name in FORMATS:

        try:

            result = analyze_format(
                sample,
                name
            )

            if result is not None:

                score = format_score(
                    name,
                    result
                )

                results.append(
                    (
                        score,
                        name,
                        result
                    )
                )

        except Exception:
            pass

    results.sort(
        reverse=True,
        key=lambda x: x[0]
    )

    for score, name, result in results:

        report.append(
            f"{name}"
        )

        report.append(
            f"  score           : {score:.2f}"
        )

        report.append(
            f"  sample elements : {result['count']:,}"
        )

        report.append(
            f"  minimum         : {result['min']}"
        )

        report.append(
            f"  maximum         : {result['max']}"
        )

        report.append(
            f"  mean            : {result['mean']:.6f}"
        )

        report.append(
            f"  median          : {result['median']:.6f}"
        )

        report.append(
            f"  unique values   : {result['unique']:,}"
        )

        report.append(
            f"  finite ratio    : {result['finite_ratio']:.6f}"
        )

        report.append("")

    # ========================================================
    # BEST FORMAT
    # ========================================================

    best_score, best_name, best_result = results[0]

    report.append(
        "5. MOST LIKELY DATA FORMAT"
    )

    report.append(
        "-" * 78
    )

    report.append(
        f"Format     : {best_name}"
    )

    report.append(
        f"Confidence score: {best_score:.2f}"
    )

    report.append("")

    # ========================================================
    # ELEMENT COUNT
    # ========================================================

    element_size = (
        best_result["dtype"].itemsize
    )

    element_count = (
        file_size // element_size
    )

    remainder = (
        file_size % element_size
    )

    report.append(
        "6. AUTOMATIC ELEMENT COUNT"
    )

    report.append(
        "-" * 78
    )

    report.append(
        f"Element size : {element_size} bytes"
    )

    report.append(
        f"Elements     : {element_count:,}"
    )

    report.append(
        f"Remainder    : {remainder} bytes"
    )

    report.append("")

    # ========================================================
    # AUTOMATIC DIMENSIONS
    # ========================================================

    dimensions = find_dimensions(
        element_count
    )

    report.append(
        "7. AUTOMATIC MATRIX DIMENSION DISCOVERY"
    )

    report.append(
        "-" * 78
    )

    if dimensions:

        report.append(
            f"Number of factor pairs: {len(dimensions):,}"
        )

        report.append("")

        # Show dimensions nearest to square.
        ranked = sorted(
            dimensions,
            key=lambda d: abs(d[0] - d[1])
        )

        for rows, cols in ranked[
            :MAX_DIMENSION_CANDIDATES
        ]:

            marker = ""

            if rows == cols:
                marker = "  <-- SQUARE MATRIX"

            report.append(
                f"{rows:,} x {cols:,}{marker}"
            )

    else:

        report.append(
            "No matrix dimensions found."
        )

    report.append("")

    # ========================================================
    # EXACT SQUARE DETECTION
    # ========================================================

    root = math.isqrt(
        element_count
    )

    report.append(
        "8. SQUARE MATRIX TEST"
    )

    report.append(
        "-" * 78
    )

    if root * root == element_count:

        report.append(
            f"YES: {root:,} x {root:,}"
        )

        report.append(
            "The element count is a perfect square."
        )

    else:

        report.append(
            "NO: Element count is not a perfect square."
        )

        report.append(
            f"Integer square root: {root:,}"
        )

    report.append("")

    # ========================================================
    # OFFSET INVESTIGATION
    # ========================================================

    report.append(
        "9. PAYLOAD OFFSET INVESTIGATION"
    )

    report.append(
        "-" * 78
    )

    report.append(
        "Testing common header/alignment offsets..."
    )

    offset_matches = []

    for offset in test_offsets(
        file_size
    ):

        result = offset_matrix_matches(
            file_size,
            offset
        )

        if result:

            dtype_label, count, dims = result

            offset_matches.append(
                (
                    offset,
                    dtype_label,
                    count,
                    dims
                )
            )

    if offset_matches:

        for offset, dtype_label, count, dims in offset_matches:

            report.append(
                f"Offset {offset:,} bytes:"
            )

            report.append(
                f"  {dtype_label}"
            )

            report.append(
                f"  Elements: {count:,}"
            )

            nearest = sorted(
                dims,
                key=lambda d: abs(d[0] - d[1])
            )[:5]

            report.append(
                f"  Likely dimensions: {nearest}"
            )

            report.append("")

    else:

        report.append(
            "No additional common-offset matrix "
            "interpretations found."
        )

    # ========================================================
    # UINT16 FULL MATRIX ANALYSIS
    # ========================================================

    report.append(
        "10. FULL UINT16 LITTLE-ENDIAN MATRIX TEST"
    )

    report.append(
        "-" * 78
    )

    if (
        file_size % 2 == 0
        and element_count == file_size // 2
    ):

        # Read the complete file only for the uint16 test.
        matrix_data = np.fromfile(
            BIN_FILE,
            dtype="<u2"
        )

        dimensions_u16 = find_dimensions(
            matrix_data.size
        )

        if dimensions_u16:

            square = [
                d for d in dimensions_u16
                if d[0] == d[1]
            ]

            if square:

                rows, cols = square[0]

                matrix = matrix_data.reshape(
                    rows,
                    cols
                )

                report.append(
                    f"Detected square matrix: "
                    f"{rows:,} x {cols:,}"
                )

                report.append("")

                report.append(
                    "Top-left 5 x 5:"
                )

                report.append(
                    str(matrix[:5, :5])
                )

                report.append("")

                report.append(
                    "Top-right 5 x 5:"
                )

                report.append(
                    str(matrix[:5, -5:])
                )

                report.append("")

                report.append(
                    "Bottom-left 5 x 5:"
                )

                report.append(
                    str(matrix[-5:, :5])
                )

                report.append("")

                report.append(
                    "Bottom-right 5 x 5:"
                )

                report.append(
                    str(matrix[-5:, -5:])
                )

                report.append("")

                report.append(
                    "Matrix statistics:"
                )

                report.append(
                    f"  Minimum : {matrix.min()}"
                )

                report.append(
                    f"  Maximum : {matrix.max()}"
                )

                report.append(
                    f"  Mean    : {matrix.mean():.6f}"
                )

                report.append(
                    f"  Median  : {np.median(matrix):.6f}"
                )

                report.append(
                    f"  Unique  : {np.unique(matrix).size:,}"
                )

                report.append("")

                report.append(
                    "First 40 values:"
                )

                report.append(
                    " ".join(
                        str(int(v))
                        for v in matrix_data[:40]
                    )
                )

                report.append("")

                report.append(
                    "Last 40 values:"
                )

                report.append(
                    " ".join(
                        str(int(v))
                        for v in matrix_data[-40:]
                    )
                )

    else:

        report.append(
            "File is not aligned to uint16."
        )

    # ========================================================
    # CONCLUSION
    # ========================================================

    report.append(
        "11. AUTOMATIC CONCLUSION"
    )

    report.append(
        "-" * 78
    )

    if (
        file_size == 2 * 3600 * 3600
    ):

        report.append(
            "The file size exactly matches "
            "3600 x 3600 x 2 bytes."
        )

        report.append(
            "Therefore a 3600 x 3600 uint16 "
            "matrix is strongly supported."
        )

    elif root * root == element_count:

        report.append(
            f"The detected element count forms a "
            f"square matrix: {root} x {root}."
        )

    else:

        report.append(
            "No unique matrix structure can be "
            "proven from file size alone."
        )

    report.append("")

    report.append(
        "IMPORTANT:"
    )

    report.append(
        "Datatype and dimensions can often be inferred "
        "from binary structure and file size."
    )

    report.append(
        "However, row-major vs column-major ordering "
        "cannot always be determined from raw bytes alone."
    )

    report.append("")

    report.append(
        "=" * 78
    )

    report.append(
        "END OF ANALYSIS"
    )

    report.append(
        "=" * 78
    )

    # ========================================================
    # Save
    # ========================================================

    with open(
        REPORT_FILE,
        "w",
        encoding="utf-8"
    ) as f:

        f.write(
            "\n".join(report)
        )

    print(
        "\n".join(report)
    )

    print()
    print("=" * 78)
    print("FULL REPORT SAVED:")
    print(REPORT_FILE)
    print("=" * 78)


if __name__ == "__main__":
    main()