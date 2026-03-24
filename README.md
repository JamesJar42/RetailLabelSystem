# RetailLabelSystem

RetailLabelSystem (built as the `RetailLabeler` desktop app) is a Windows Qt application for generating and printing shelf-edge retail labels on A4 pages. It combines product data management, barcode-based queueing, print preview, and layout customization so store teams can move from product list to printable labels quickly.

The app is designed for day-to-day retail use where labels are updated frequently due to new products, promotions, and price changes.

## What the app does

RetailLabeler helps you:

- Manage a product catalog (name, barcode, price, original price, label quantity).
- Load product data from CSV, internal resource files, or Clover.
- Queue labels using manual edits, table operations, or fast barcode scan/paste flows.
- Preview print pages before sending to a printer.
- Tune label layout values (text size, positions, offsets) without recompiling.
- Save user preferences such as theme, data-source mapping, and print behavior.

## Core feature overview

### Product and queue management

- Table-based product editor in the main window.
- Search and filter helpers for large product sets.
- Queue quantity controls per product.
- Queue workspace (`Ctrl+Shift+P`) with:
	- Queue summary (queued products, total labels, estimated pages).
	- Bulk quantity operations (increase, decrease, remove selected, clear queue).
	- Optional auto-clear after print preview.
	- Undo support for destructive queue actions.

### Barcode workflow

- Barcode scan dialog (`Ctrl+L`) accepts scanner input or pasted multiline barcode lists.
- Supports quantity-per-scan setting.
- Matches products by barcode and increments queue quantities.
- Shows processing log for quick operator feedback.

### Print and layout workflow

- Print labels (`Ctrl+P`) opens preview and print flow.
- Label pages are generated for A4 dimensions using configurable layout parameters.
- Optional native print dialog mode.
- Optional Qt text rendering mode for label text.
- Automatic cleanup attempts run after print flow completes (with retry logic).

### Data source options

Configurable in Settings Workspace under Data Source:

- Default (Internal).
- CSV (External file path + configurable column mapping).
- Clover integration (token-based validation and OAuth-assisted connection flow).

### Visual options

- Light, Dark, or custom QSS theme file.
- Optional custom app icon.
- Optional custom logo used in generated labels.
- Accessibility/dev helper for focus debug logging.

## High-level architecture

- Qt 6 Widgets app entry in `src/run.cpp`.
- Main UI and workflows in `src/MainWindow.cpp`.
- Label generation and print orchestration in `src/labelSystem.cpp`.
- Product storage, CSV IO, Clover API, and lookups in `src/shop.cpp`.
- Product model in `src/product.cpp`.
- UI form in `forms/MainWindow.ui`.

### Build targets

- `RetailLabeler`: main GUI application.
- `retail_core`: static library for core logic.
- `labelSystem_tests`: unit test executable.

## Requirements

### Runtime (Windows)

- Windows 10/11 (x64).
- Installed printer (for physical printing).

### Build-time dependencies

- CMake 3.16+
- Visual Studio 2022 (MSVC)
- Qt 6 (Widgets, Core, Gui, PrintSupport, Network, Test)
- OpenCV 4.x

Optional for packaging:

- Qt Installer Framework (for IFW packaging via CPack)
- NSIS (if you choose NSIS generator)

## Build and run

### Configure and build (PowerShell example)

```powershell
cmake -S . -B build -DOpenCV_DIR="<path-to-opencv-install>" -DQt6_ROOT="<path-to-qt-root>"
cmake --build build --config Debug
```

For release:

```powershell
cmake --build build --config Release
```

### Run

Debug executable:

```powershell
.\build\Debug\RetailLabeler.exe
```

Release executable:

```powershell
.\build\Release\RetailLabeler.exe
```

Optional temporary theme override:

```powershell
.\build\Debug\RetailLabeler.exe --theme=dark
```

Supported values: `light`, `dark`, `file`.

## First-time setup and daily use

### 1) Choose data source

Open Settings Workspace from Options menu and configure Data Source:

- Default (Internal): use bundled/default behavior.
- CSV (External): choose a CSV path and set column indexes.
- Clover: configure merchant/token or OAuth values and test connectivity.

### 2) Validate visual and label output settings

In Settings Workspace:

- Pick theme (Light, Dark, or custom QSS file).
- Optionally set custom logo/icon paths.
- Open Label Layout and review layout values.
- Print options:
	- Use native print dialog.
	- Use Qt text rendering for labels.

### 3) Build queue

Use one or more methods:

- Edit queue quantities directly in the table.
- Use scan dialog (`Ctrl+L`) for one or many barcodes.
- Open Queue Workspace (`Ctrl+Shift+P`) for bulk queue operations.

### 4) Print

- Press `Ctrl+P` or use File -> Print Labels (Preview).
- Review preview/pages.
- Confirm clearing queue if prompted.

## Keyboard shortcuts

- `Ctrl+F`: focus search
- `Ctrl+N`: add product
- `Ctrl+P`: print labels
- `Ctrl+Shift+P`: open queue workspace
- `Ctrl+S`: save CSV
- `Ctrl+O`: load CSV
- `Ctrl+L`: scan barcodes
- `Ctrl+Z`: undo last destructive action
- `Del`: remove selected products
- `Esc`: clear selection or leave current field
- `F6`: cycle focus regions

## Data formats and mapping

### Label config file

The app reads label layout defaults from `resources/Config.txt`:

```text
labels 24 44 140 15 80 90 350 15 140 40
```

Value order is:

1. `TL` (text length threshold before wrapping)
2. `TS` (text size)
3. `PS` (price size)
4. `TX` (text X)
5. `TY` (text Y)
6. `PX` (price X)
7. `PY` (price Y)
8. `STX` (second-line text X)
9. `STY` (second-line text Y)
10. `XO` (page X offset)

These values can be overridden at runtime via settings and edited in-app using the layout editor.

### CSV expectations

Sample data in `resources/Database.csv` uses:

```csv
Barcode,Name,Price,Size,OriginalPrice,LabelFlag
123456789012,Sample Product,9.99,100g,12.99,0
999999999999,Sale Product,5.00,Each,8.00,1
```

Important mapping notes:

- CSV mode lets you map column indexes from settings.
- Header row can be enabled/disabled.
- Queue quantity is read from the configured label quantity column.
- Save-to-CSV writes a compact format including barcode/name/price/original price/label flag.

## Clover integration notes

The integration tab supports:

- Merchant ID and API token validation.
- Sandbox/production mode switching.
- OAuth-assisted connect flow with localhost callback.

If connection tests fail, verify:

- Token validity and merchant match.
- Sandbox flag consistency.
- Redirect URI and localhost callback availability.

## Logging and diagnostics

Depending on workflow and failure mode, the app may write logs such as:

- `resources/CrashTrace.log`
- `resources/DeletionLog.txt`
- OAuth debug logs in app-data paths during Clover connect flow

Use these logs when diagnosing print, cleanup, or integration issues.

## Tests

The project includes `labelSystem_tests` with coverage for:

- String/title-case utility behavior.
- Product and shop core operations.
- Label config round-trip and save behavior.
- Queue clearing behavior.
- Generated page deletion retry routine.

Example run after build:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

## Packaging and installer creation

### Basic package build

```powershell
cmake -S . -B build -DOpenCV_DIR="<path-to-opencv-install>" -DQt6_ROOT="<path-to-qt-root>"
cmake --build build --config Release
cd build
cpack
```

The project is configured for CPack/IFW by default on Windows.

### Optional: custom shortcut icon per build

Provide an icon path at configure time:

```powershell
cmake -S . -B build -DOpenCV_DIR="<path-to-opencv-install>" -DQt6_ROOT="<path-to-qt-root>" -DCPACK_SHORTCUT_ICON="<path-to-icon.ico>"
cmake --build build --config Release
cd build
cpack
```

If valid, this icon is packaged as `RetailLabeler.ico` and used for installer-created shortcuts.

## Troubleshooting quick list

- Qt or OpenCV not found at configure time:
	- Re-run CMake with `-DQt6_ROOT=...` and `-DOpenCV_DIR=...`.
- App starts but styles not applied:
	- Verify selected theme and resource QSS file availability.
- Print queue appears empty:
	- Confirm queue quantities are greater than zero.
- CSV load appears wrong:
	- Recheck column mapping indexes and header-row setting.
- Clover auth/validation issues:
	- Verify token, merchant, environment selection, and callback routing.

## License

See `LICENSE`.

