# KSC to JPEG Converter

This project is a C-based utility for converting `.ksc` files (used in Knight Online) into `.jpeg` image format.  
⚠️ The core logic is implemented, but **several features are pending implementation**. Also, the program hasn't been tested due to the unavailability of `.ksc` files.

---

## ❗ Disclaimer

> ⚠️ The tool has not been tested as no valid `.ksc` files were available during development. Knight Online is not installed, and no `.ksc` samples could be located.

---

## 🛠️ Features

### ✅ Implemented:
- Basic conversion from `.ksc` to `.jpeg`.

### ❌ Not Yet Implemented:

1. **Recursive Search**  
   Automatically search for `.ksc` files in subdirectories.

2. **Skip Already Converted Files**  
   Skip processing if a corresponding `.jpeg` file already exists.

3. **Logging System**  
   Log each conversion (success/failure) to a file for auditing or debugging.

4. **Conversion Time Measurement**  
   Measure and display how long each conversion takes.

5. **Auto-Open Output**  
   Automatically open the converted file once the process is complete.

6. **Header Validation**  
   Validate input files to ensure they are real `.ksc` files before processing.

7. **Custom Output File Name/Path**  
   Allow the user to specify a different output filename or path.

---

## 📦 Requirements

- A C compiler (e.g., GCC)  
- Knight Online `.ksc` files (not included)

---

## 🔧 Compilation

```bash
gcc -o ksc_converter main.c
