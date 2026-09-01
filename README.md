# MatBin - Cross-Platform Binary-to-MATLAB MAT File Converter v1.3.0

**Code by:** Yuvanesh (MC1)  
**Primary Use:** Conversions of Tercom `.bin` elevation/telemetry binary logs to MATLAB binary v5 (`.mat`) files.

**MatBin** is a high-throughput, production-grade desktop application built with **C++17**, **Qt 6**, and the **MATIO** C library. It ingests raw numerical binary telemetry streams (`.bin`, `.dat`), automatically resolves payload precision and optimal matrix dimensions ($R \times C$), provides an interactive 3-tab content inspector (**Data**, **Hex**, **Visual Plot**), and serializes payloads into structured MATLAB workspace containers (`.mat`).

---

## 1. Key Features & Capabilities

### 1. Smart Payload Inspection & Auto-Detection
- **Numerical Precision Inspection**: Identifies element data width automatically upon file attachment:
  - `Double64` (64-bit double precision float - 8 bytes)
  - `Single32` (32-bit single precision float - 4 bytes)
  - `Int64` / `Uint64` (64-bit signed/unsigned integer - 8 bytes)
  - `Int32` / `Uint32` (32-bit signed/unsigned integer - 4 bytes)
  - `Int16` / `Uint16` (16-bit signed/unsigned integer - 2 bytes)
  - `Int8` / `Uint8` (8-bit signed/unsigned integer - 1 byte)
- **Endianness Auto-Detection**: Distinguishes between native Little-Endian (x86-64 / ARM64) and Big-Endian (Network Order) byte sequences.
- **Manual Overrides**: Complete manual override controls available for both precision and byte ordering.

### 2. Dynamic Matrix Dimension Factor Selection ($R \times C$)
- **Auto-Detect Square & Natural Factors**: Automatically calculates the optimal square or natural factor pair ($R \approx \sqrt{N}$). For example, `15_75.bin` ($12,960,000$ `uint16` values) is automatically detected and converted into a **`3600 × 3600`** MAT matrix.
- **Factor Dropdown Selector**: Dynamically calculates and lists all mathematically valid divisor dimensions for the selected binary file:
  - `Auto Detect (3600 × 3600)`
  - `1 × 12,960,000 (Row Vector)`
  - `2 × 6,480,000`
  - `3 × 4,320,000`
  - ...
  - `3600 × 3600 (Square Matrix)`
  - ...
  - `12,960,000 × 1 (Column Vector)`

### 3. Interactive Content Viewer & 2D Heatmap Inspector
Select any file in the Batch Queue and click **View Contents** to open the 3-tab inspection window:
- **`Data` Tab**: Formatted 2D matrix table displaying actual numerical values.
- **`Hex` Tab**: Raw 16-byte hex editor byte dump (`Offset  00 01 ... |ASCII|`) with instant performance streaming.
- **`Visual Plot` Tab**: Graphical 2D heatmap matrix renderer accompanied by an explicit **Color Shades Legend**:
  - **Deep Blue / Indigo**: Minimum payload values (Low amplitude/intensity).
  - **Cyan / Teal**: Lower mid-range values.
  - **Green / Emerald**: Mid-range baseline values.
  - **Yellow / Amber**: Upper mid-range values.
  - **Bright Red / Crimson**: Maximum payload values (High amplitude/intensity).
- **Responsive Window Sizing & Controls**: Features native **Minimize (_)**, **Maximize (□)**, and **Close (X)** title bar controls, with dynamic screen geometry scaling to guarantee no off-screen overflow.

### 4. High-Throughput MATIO Engine & Batch Processing
- **Queue Management**: Add, remove, or clear queued files dynamically.
- **Export Destination**: Default output directory set to `C:\Users\<User>\Downloads\MatBin\output` with automatic directory creation.
- **Zero Compression Overhead**: Fast C-API serialization into MATLAB Version 5 (`MAT_FT_MAT5`) containers.

---

## 2. Technical Architecture & Ingestion Pipeline

```
┌─────────────────────────────────┐      ┌──────────────────────────────────┐      ┌─────────────────────────────┐
│      Raw Binary File Input      │ ───► │   Smart Payload Inspector &      │ ───► │     MATIO C Serialization   │
│   (.bin, .dat, 1 to 8-byte)     │      │   Dynamic Factor Engine (R x C)  │      │     MATLAB v5 Container     │
└─────────────────────────────────┘      └──────────────────────────────────┘      └─────────────────────────────┘
```

### Element Calculation & Modulo Validation
1. **Total Usable Bytes ($S_{\text{usable}}$):**
   $$S_{\text{usable}} = S_{\text{file}} - (S_{\text{file}} \pmod{S_p})$$

2. **Total Element Count ($N$):**
   $$N = \frac{S_{\text{usable}}}{S_p}$$

3. **Matrix Dimensions ($R \times C$):**
   $$R \cdot C = N \quad \text{where } R \in \text{Divisors}(N)$$

---

## 3. MATLAB Integration & Data Import

To load generated `.mat` files in MATLAB:

```matlab
% Load processed MAT container
load('C:\Users\<User>\Downloads\MatBin\output\15_75.mat');

% Inspect variable dimensions (e.g. 3600 x 3600)
size(data)

% Display summary statistics
mean_val = mean(data(:));
max_val  = max(data(:));

% Render 2D heatmap visualization in MATLAB
imagesc(data);
colorbar;
title('Tercom Elevation Data (15_75)');
```

---

## 4. Building from Source

### Prerequisites
- **Compiler:** MSVC 2022 (x64), GCC 10+, or Clang 11+
- **Build Tool:** CMake 3.16+ & Ninja / MSBuild
- **Framework:** Qt 6.0+ (`Core`, `Widgets`, `Concurrent`, `Svg`, `Gui`)
- **Dependency:** MATIO C-Library (bundled under `third_party/matio`)

### Developer Command Prompt Build (MSVC x64)

```cmd
:: 1. Release Target Build
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S . -B build/Desktop_Qt_6_11_1_MSVC2022_64bit_Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 -G Ninja
cmake --build build/Desktop_Qt_6_11_1_MSVC2022_64bit_Release --config Release

:: 2. Debug Target Build
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S . -B build/Desktop_Qt_6_11_1_MSVC2022_64bit_Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 -G Ninja
cmake --build build/Desktop_Qt_6_11_1_MSVC2022_64bit_Debug --config Debug
```

The generated executable will be produced at `build/Desktop_Qt_6_11_1_MSVC2022_64bit_Release/matbin.exe`.
