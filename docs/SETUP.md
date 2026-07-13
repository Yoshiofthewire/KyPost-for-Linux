# Local tooling gaps

Checked on this machine during repo bootstrap (Task 1). Nothing here is
installed by this pass — this is a checklist for later phases.

- **`extra-cmake-modules`** (pacman package `extra/extra-cmake-modules`) —
  verify: `pkg-config --exists ECM` (exit 1 today) or
  `pacman -Q extra-cmake-modules` (reports "not found" today). Currently
  optional: the top-level `CMakeLists.txt` does
  `find_package(ECM QUIET NO_MODULE)` and only extends
  `CMAKE_MODULE_PATH` if found. Needed once a task starts using ECM's
  convenience macros (e.g. `ecm_add_test`, `KDECompilerSettings`).

- **`flatpak-builder`** — verify: `which flatpak-builder` (not found today).
  Blocks the Flatpak packaging skeleton (`packaging/flatpak/`, future
  phase).

- **`clickable`** (pip package) — verify: `which clickable` (not found
  today). Blocks the Clickable/Ubuntu Touch packaging skeleton
  (`packaging/click/`, future phase).

- **A QtWebEngine package (Qt6 variant) + the `io.qt.qtwebengine.BaseApp`
  Flatpak extension** — verify: `pacman -Qs qtwebengine` (no match today) /
  `flatpak list | grep qtwebengine` (no match today, no Flatpak remotes
  configured). Blocks HTML mail rendering (future phase), not needed for
  this pass.

- **`kunifiedpush`** (KDE/kunifiedpush Connector library + host distributor
  daemon) — verify: `pkg-config --modversion KUnifiedPush` (not found
  today) or `pacman -Qs kunifiedpush` (no match today). Blocks the
  KUnifiedPush push proofs (future phase, needs the user's real
  distributor/account anyway).
