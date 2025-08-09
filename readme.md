# KSC to JPEG Converter

A cross-platform C utility to convert Knight Online `.ksc` screenshot files into standard `.jpeg` images.

The tool supports single-file conversion, full recursive directory scanning, skipping already-converted files, logging, and more — on both **Windows** and **Linux**.

---

## ⚠️ Disclaimer

> Knight Online `.ksc` files are **not** included. You must obtain them from your own installation or screenshots.  
> This tool is for personal use; check the game’s terms before sharing converted images.

---

## 🛠️ Features

### ✅ Implemented

- **Basic `.ksc` → `.jpeg` conversion** using the original Knight Online decryption algorithm.
- **Recursive search** (`-r [dir]`) — scan subdirectories automatically.
- **Skip already converted files** — avoids overwriting unless `--force` is implemented in future.
- **Logging system** (`--log <file>`) — records successes and errors with timestamps.
- **Conversion time measurement** — shows time taken per file.
- **Auto-open output** (`--open`) — launches the converted JPEG after processing.
- **Custom output directory** (`-o <outdir>`) — preserves directory structure when mirroring.
- **Cross-platform compatibility** — tested with GCC/Clang on Linux and MinGW/MSVC on Windows.
- **`-h` / `--help`** — display usage info.

### 💡 Planned / Possible Small Additions

- **Dry-run mode** (`--dry-run`) to preview what would be converted.
- **Progress counter** (total processed, converted, skipped, errors).
- **Overwrite flag** (`--force`) to re-convert even if JPEG exists.
- **JPEG header validation** after conversion.

---

## 📦 Requirements

- A C99-compatible compiler:
  - **Windows:** MSVC, MinGW, or similar
  - **Linux:** GCC or Clang
- Knight Online `.ksc` files

---

## 🔧 Compilation

### Linux / macOS
```bash
gcc -O2 -Wall -Wextra -o ksc2jpeg ksc2jpeg.c
```

### Windows (MinGW)
```bash
gcc -O2 -Wall -Wextra -o ksc2jpeg.exe ksc2jpeg.c
```

### Windows (MSVC)
```bat
cl /O2 /W4 /EHsc ksc2jpeg.c shell32.lib
```

---

## 🚀 Usage

**Single file:**
```bash
ksc2jpeg input.ksc output.jpeg
```

**Recursive from current directory:**
```bash
ksc2jpeg -r
```

**Recursive from a specific directory, output to another:**
```bash
ksc2jpeg -r /path/to/screens -o /path/to/output
```

**With logging and auto-open:**
```bash
ksc2jpeg -r --log convert.log --open
```

**Help:**
```bash
ksc2jpeg -h
```

---

## 📄 License

GNU General Public License v3.0 — see `LICENSE` for details.
