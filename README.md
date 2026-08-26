# MatBin - Cross-Platform Binary-to-MATLAB Converter

**MatBin** is a production-grade, high-throughput desktop application built with **C++17**, **Qt 6**, and the **MATIO** C library. It ingests raw numerical binary telemetry streams (`.bin`, `.dat`), automatically resolves matrix dimensions without manual user entry, validates data integrity, and serializes payloads into structured MATLAB workspace containers (`.mat`).

---

## 1. Core Mechanics & Technical Architecture

### 1. Batch Queue & File Routing Mechanics
Managing bulk data extraction requires a blend of automated batch processing and granular user overrides:
* **Batch Queue (`QListWidget`):** Acts as the central container holding the sequence of active export tasks or datasets. Users can reorder, add, or remove entries dynamically before triggering a bulk operation.
* **Destination Routing (`QFileDialog::getExistingDirectory`):** Establishes the root directory target for automated batch outputs, ensuring all processed elements stream into a unified destination path.
* **Single-File Overrides (`QFileDialog::getSaveFileName`):** Intercepts the automated queue workflow when a user requires a bespoke path or custom filename for a specific high-priority dataset, bypassing the default batch directory.

### 2. Numerical Precision & Stream Endianness
Data serialization relies on strict type definitions and byte-ordering configurations to maintain cross-platform compatibility:
* **Memory Byte Widths ($S_p \in \{4, 8\}$):** Controls the allocation footprint per element based on the selected target type:
  * `Double64` / `Int64`: Allocated at $S_p = 8$ bytes.
  * `Single32` / `Int32`: Allocated at $S_p = 4$ bytes.
* **Stream Endianness:** Determines the multi-byte serialization order:
  * **Little-Endian:** Least significant byte stored first (native to x86/x64 and ARM architectures).
  * **Big-Endian:** Most significant byte stored first (frequently used in network protocols and legacy systems).

### 3. Channel Layout & Dimensionality ($M$)
Data shaping dictates how arrays are interpreted by downstream analysis software or export formats:
* **1D Vector Mode ($1 \times E$):** Treats the dataset as a continuous stream of $E$ elements, ideal for single-sensor time-series logs or flattened signals.
* **2D Matrix Mode ($M \times N$):** Structures data into $M$ parallel channels across $N$ samples per channel, crucial for multi-channel recordings (e.g., EEG or multi-sensor arrays).

### 4. MATLAB Workspace Variable Integration
To bridge Qt applications with MATLAB environments, dynamic variable instantiation is used:
* **Workspace Symbol (`"data"`):** Defines the exact variable name string used when injecting memory buffers directly into the MATLAB workspace. This allows external scripts or automated analysis pipelines to immediately hook into the variable without manual renaming.
> **Note:** Ensure that the matrix mode dimensions ($M \times N$) match the expected array shape of the workspace symbol to prevent MATLAB dimensional mismatch errors during import.

---

## 2. Ingestion Dynamics & Automated Dimension Resolution

```
┌────────────────────────┐      ┌───────────────────────────────┐      ┌─────────────────────────┐
│   Raw Binary Stream    │ ───► │  Automated Dimension & Stream │ ───► │ MATIO Serialization     │
│ (.bin, .dat, 4/8-byte) │      │  Validation Engine (E, N, M)  │      │ MATLAB v5 Container     │
└────────────────────────┘      └───────────────────────────────┘      └─────────────────────────┘
```

1. **Total Numerical Elements ($E$):**
   $$E = \frac{S_{\text{file}}}{S_p}$$

2. **Total Record Count ($N$):**
   $$N = \frac{E}{M} = \frac{S_{\text{file}}}{M \cdot S_p}$$

3. **Stream Modulo Alignment Validation:**
   $$S_{\text{file}} \pmod{M \cdot S_p} == 0$$

4. **Data Payload & IEEE 754 Validation:**
   Evaluates floating-point elements with `std::isnan()` and `QDataStream::status()` to catch bit-reversal or stream corruption errors during ingestion.

---

## 3. UI Implementation Snippet

```cpp
// 1. Numerical Precision Selector
dataTypeComboBox = new QComboBox();
dataTypeComboBox->addItem(tr("64-bit Double Precision Float (double)"), static_cast<int>(BinToMatConverter::DataType::Double64));
dataTypeComboBox->addItem(tr("32-bit Single Precision Float (float)"), static_cast<int>(BinToMatConverter::DataType::Single32));
dataTypeComboBox->addItem(tr("32-bit Signed Integer (int32_t)"), static_cast<int>(BinToMatConverter::DataType::Int32));
dataTypeComboBox->addItem(tr("64-bit Signed Integer (int64_t)"), static_cast<int>(BinToMatConverter::DataType::Int64));

// 2. Stream Endianness Selector
endiannessComboBox = new QComboBox();
endiannessComboBox->addItem(tr("Little-Endian (x86-64 / ARM64 Native)"), static_cast<int>(BinToMatConverter::Endianness::LittleEndianMode));
endiannessComboBox->addItem(tr("Big-Endian (Network Order)"), static_cast<int>(BinToMatConverter::Endianness::BigEndianMode));

// 3. Channels Per Frame Selector
channelsSpinBox = new QSpinBox();
channelsSpinBox->setRange(1, 1024);
channelsSpinBox->setValue(1);

// 4. Workspace Variable Name Input
varNameLineEdit = new QLineEdit("data");
varNameLineEdit->setPlaceholderText(tr("MATLAB matrix variable name"));
```

---

## 4. MATIO Serialization Engine (Deep Dive)

### Low-Level C-API Execution Flow

```
Mat_CreateVer()  --->  Mat_VarCreate()  --->  Mat_VarWrite()  --->  Mat_VarFree()  --->  Mat_Close()
```

1. **`Mat_CreateVer(filename, header, MAT_FT_MAT5)`**
   * Creates or overwrites target file on disk using MATLAB Version 5 container format (`MAT_FT_MAT5`).
2. **`Mat_VarCreate(name, matClass, matType, rank, dims, dataPtr, opt)`**
   * Allocates `matvar_t` descriptor for `MAT_C_DOUBLE`, `MAT_C_SINGLE`, `MAT_C_INT32`, or `MAT_C_INT64`.
3. **`Mat_VarWrite(matfp, var, MAT_COMPRESSION_NONE)`**
   * Streams contiguous data buffer to disk without compression overhead.
4. **`Mat_VarFree(var)`** & **`Mat_Close(matfp)`**
   * Releases wrapper metadata and safely closes file handle.

---

## 5. Output Generation & MATLAB Usage

```matlab
% Load generated MAT container
load('telemetry_file.mat');

% Display dimensions (M channels x N records)
size(data)

% Access Channel 1
channel_1 = data(1, :);

% Access Channel 2
channel_2 = data(2, :);

% Plot signals
plot(data(1, :), data(2, :));
grid on;
```

---

## 6. Building from Source

### Prerequisites
- **Compiler:** MSVC 2022 (x64), GCC 10+, or Clang 11+
- **Build System:** CMake 3.16+ & Ninja / MSBuild
- **Framework:** Qt 6.0+ (`Core`, `Widgets`, `Concurrent`, `Svg`)

### Build Commands
```cmd
cmake -S . -B build -G Ninja
cmake --build build
Developer Command Prompt Commands Executed
1. Release Target Build
cmd

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S C:\Users\YUVANESH\Desktop\MatBin -B C:\Users\YUVANESH\Desktop\MatBin\build\Desktop_Qt_6_11_1_MSVC2022_64bit_Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 -G Ninja
cmake --build C:\Users\YUVANESH\Desktop\MatBin\build\Desktop_Qt_6_11_1_MSVC2022_64bit_Release --config Release
2. Debug Target Build
cmd

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S C:\Users\YUVANESH\Desktop\MatBin -B C:\Users\YUVANESH\Desktop\MatBin\build\Desktop_Qt_6_11_1_MSVC2022_64bit_Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 -G Ninja
cmake --build C:\Users\YUVANESH\Desktop\MatBin\build\Desktop_Qt_6_11_1_MSVC2022_64bit_Debug --config Debug
```

The compiled binary will be generated at `build/matbin.exe`.
