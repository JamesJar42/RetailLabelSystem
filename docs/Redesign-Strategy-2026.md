# RetailLabelSystem Redesign Strategy (2026)

## 1. Objective
Redesign the full application to match modern desktop conventions for data-heavy workflows while preserving your current product model, printing flow, and external CSV/Clover sync logic.

Primary UX goals:
- Faster product lookup and queueing.
- Safer editing with fewer accidental changes.
- Better visibility of system status (loading, sync, save, print readiness).
- Consistent visual hierarchy across main window and dialogs.
- Keyboard-first efficiency for power users.
- Accessibility compliance baseline (WCAG 2.2 AA-inspired behavior for desktop UI).

## 2. Research Basis
This strategy is based on cross-platform guidance and practical enterprise patterns:
- Nielsen Norman Group: 10 usability heuristics (status visibility, recognition over recall, error prevention, user control).
- W3C WCAG 2.2 quick reference: contrast, focus visibility, keyboard operability, error identification.
- Microsoft Windows app design + controls guidance: command surfaces, list/detail patterns, adaptive layout, consistent Windows conventions.
- Material 3 adaptive layout concepts: reveal/reposition/swap components by available window size.
- Carbon Data Table guidance: toolbar actions, search/filter/sort behavior, batch actions, pagination, row interaction patterns.
- GNOME HIG styling guidance: light/dark + high contrast support and minimal custom styling for maintainability.

## 3. Current-State UX Audit (Your App)
Observed in current implementation:
- Main shell is table + top search + lower action buttons.
- Many key tasks are spread across menubar items, toolbar actions, table inline edits, and dialogs.
- Scan and queue flows are present but fragmented across multiple entry points.
- Theme support exists but focus styles and state messaging can be stronger.
- Table supports edit directly, but validation and commit flow are implicit.

Main friction risks:
- Command discoverability: users must remember where actions live.
- Inline editing safety: accidental edits can happen without explicit save/undo affordance.
- Status transparency: sync/loading/reload states rely mostly on subtle messaging.
- Dense workflows: no persistent quick filters, saved views, or strong batch action zone.

## 4. Target Information Architecture
Reframe the app into three explicit work areas:

1) Product Workspace (default)
- Primary: searchable/editable product table.
- Secondary: quick filter rail and batch action bar.
- Purpose: maintain catalog and queue print quantities.

2) Print Queue Workspace
- Focused table showing only queued rows.
- Queue-level controls: clear queue, increment/decrement, remove selected, print preview.
- Purpose: reduce mistakes before printing.

3) Settings Workspace
- Organized tabs: Data Source, Visual Theme, Label Layout, Integrations.
- Purpose: remove scattered settings from menus and place into one coherent control center.

## 5. Modern Interaction Model
### 5.1 Command Surfaces
Adopt clear command hierarchy:
- Primary top command bar: Add, Print Queue, Print Preview, Sync.
- Secondary contextual row actions: Edit, Duplicate, Remove, Queue +/-.
- Batch action bar appears only when rows are selected.
- Overflow menu for less frequent operations.

Why: aligns with Windows/Fluent command conventions and reduces cognitive load.

### 5.2 Table Behavior
Keep table as core UI, but modernize behavior:
- Sticky header and persistent sort indicators.
- Multi-column sorting support (optional phase 2).
- Filter chips (In Queue, Price Changed, Missing Barcode, etc.).
- Inline edit with explicit commit states:
  - edited but not saved (dot indicator)
  - invalid value (inline error state)
  - saved (brief success indicator)
- Row density switch: Comfortable / Compact.

### 5.3 Search and Filtering
- Unified search box with instant filtering.
- Advanced filter drawer for field-specific filters.
- Saved views (example: "Print Today", "No Barcode", "Discounted").

### 5.4 Feedback and Status
- Non-blocking status strip for sync progress and background tasks.
- Inline info/warning bars near affected area (instead of only message boxes).
- Confirm destructive actions with undo option where feasible.

### 5.5 Keyboard and Power-User Flow
Recommended shortcuts:
- Ctrl+F: focus search.
- Ctrl+N: add product.
- Ctrl+P: open print queue/preview.
- Ctrl+S: save/export current state.
- Del: remove selected row(s) with confirm.
- Enter: edit focused cell.
- Esc: cancel inline edit or close transient panel.

## 6. Visual System Direction
Define a visual language that feels modern but practical:
- Typography scale with clear hierarchy (App title, section title, body, caption).
- Tokenized spacing rhythm (4/8/12/16/24).
- Strong neutral base with one accent color for actions.
- Semantic colors only for status (success/warning/error/info), never meaning by color alone.
- Larger click/focus targets for high-frequency controls.

Theme requirements:
- Light and dark parity.
- High-contrast compatibility checks.
- Distinct, visible keyboard focus ring in all themes.

## 7. Accessibility Baseline (Desktop Interpretation of WCAG 2.2 AA)
Must-have acceptance criteria:
- Text contrast >= 4.5:1 for normal text.
- Non-text interactive affordances >= 3:1 contrast against adjacent colors.
- Fully operable by keyboard; no keyboard trap.
- Visible focus indicator at all times for focused controls.
- Error states include plain-language correction guidance.
- No critical information conveyed by color alone.
- Minimum practical target size for frequent actions.

## 8. Dialog and Workflow Simplification
Current dialogs should be refactored into a consistent pattern:
- Header: title + short purpose text.
- Body: grouped controls with labels and helper text.
- Footer: primary action on right, secondary/cancel on left.
- Live validation before submit.

Apply to:
- Print dialog
- Database settings
- Visual settings
- Config editor
- Barcode scan/add-to-queue dialog

## 9. Proposed Qt Architecture Changes
UI-level technical strategy:
- Move from QTableWidget toward model-driven QTableView + QAbstractTableModel for scalability.
- Add QSortFilterProxyModel for robust search/filter/sort composition.
- Centralize command actions in QAction registry for toolbar/menu/shortcuts consistency.
- Create a ThemeManager with semantic tokens (rather than ad hoc per-widget style overrides).
- Introduce Notification/Status service for in-app info bars/toasts.

Result: cleaner state management and easier future redesign iteration.

## 10. Phased Implementation Roadmap
### Phase 0: UX Foundation (1-2 weeks)
- Define design tokens (color, spacing, type, radius, focus).
- Build command map and keyboard map.
- Consolidate settings IA and workspace navigation wireframes.
- Deliverables: token file(s), UX wireframes, component checklist.

### Phase 1: Main Window Modernization (2-3 weeks)
- New shell layout: command bar + filter/search row + table + status strip.
- Implement batch action bar and selection model.
- Improve row editing states and validation messaging.
- Deliverables: reworked MainWindow, updated QSS token usage.

### Phase 2: Queue and Print Experience (2 weeks)
- Dedicated queue workspace/view.
- Safer pre-print review with page count/status summary.
- Better progress and retry feedback for print/delete lifecycle.
- Deliverables: new queue flow and print readiness checklist UI.

### Phase 3: Settings Consolidation (1-2 weeks)
- Replace scattered options with single Settings workspace/dialog.
- Add import/export profile for settings.
- Deliverables: coherent settings UX, reduced menu complexity.

### Phase 4: Accessibility and Polish (1-2 weeks)
- Keyboard-only QA pass.
- Contrast/focus audit in light/dark/high contrast.
- Micro-interactions and visual polish.
- Deliverables: accessibility checklist pass, final UI refinement.

## 11. Redesign KPIs
Measure redesign impact with practical metrics:
- Time to queue and print 10 labels.
- Error rate in price/quantity edits.
- Number of clicks for common tasks (scan, edit, print).
- Success rate for keyboard-only operation.
- User-reported confidence in print preview and queue state.

## 12. Immediate Next Sprint Recommendation
If you want to start now, begin with this vertical slice:
1) Introduce new shell layout with command bar and status strip.
2) Move search + quick filters into dedicated toolbar region.
3) Add batch selection actions for queue operations.
4) Add explicit edit validation states in table cells.

This gives visible modernization fast, with low risk to core printing logic.
