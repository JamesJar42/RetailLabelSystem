# RetailLabelSystem
A system to create shelf edge retail labels printed on A4 paper.

## Packaging / Creating an installer (Windows - NSIS)

Prerequisites:
- CMake 3.16+
- Visual Studio / MSVC toolchain (for building)
- Qt 6 (matching the project's find_package settings)
- OpenCV installed and available; pass -DOpenCV_DIR to CMake if not auto-detected
- NSIS (makensis) installed if you want to generate an NSIS installer

Build and create an NSIS installer (example):

```powershell
# Configure and build Release (adjust OpenCV_DIR and Qt6_ROOT if needed)
cmake -S . -B build -DOpenCV_DIR="D:/opencv/build/install" -DQt6_ROOT="D:/Qt/6.7.3/msvc2022_64"
cmake --build build --config Release

# Create an NSIS installer using CPack
cd build
cpack -G NSIS
```

After successful packaging the installer will be created in the `build` directory (filename follows the pattern `RetailLabeler-<version>-win64.exe` by default).

If `cpack` reports missing `makensis` or NSIS not found, install NSIS and ensure `makensis.exe` is on PATH.

### Per-build custom shortcut icon

You can supply a build-time custom icon that the installer will use for Start Menu and Desktop shortcuts.
Pass the icon path when configuring CMake (builder-side) using the `CPACK_SHORTCUT_ICON` cache variable:

```powershell
cmake -S . -B build -DOpenCV_DIR="D:/opencv/build/install" -DQt6_ROOT="D:/Qt/6.7.3/msvc2022_64" -DCPACK_SHORTCUT_ICON="C:/path/to/myicon.ico"
cmake --build build --config Release
cd build
cpack -G NSIS
```

If the icon file exists it will be installed into the package `bin/` directory as `RetailLabeler.ico` and the installer will create shortcuts using that icon.

