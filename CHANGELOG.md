# Changelog — waterfall

All notable changes are documented here. Format: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) · Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Guiding principle:** A changelog is written for humans — your users and teammates — not for machines.  
> Document *what changed and why it matters*, not what files you touched.

---

## [Unreleased]

> Work merged but not yet shipped. Move entries to a versioned section on release.

### Added
- Zooming in on frequency lengthens the FFT used to draw the picture, so the zoom shows real extra detail instead of larger pixels. A note at the top left of the waterfall always shows the FFT size, the bin width, and the overlap, which rises automatically as the window grows. A Reset zoom button appears while zoomed and returns to the configured FFT size and overlap.
- A row of the waterfall shows the strongest of the frequency bins it covers, so narrow peaks no longer drop out when the FFT has more bins than the screen has pixels.
- FFT sizes 8192, 16384, and 32768, and overlaps of 93.75% and 96.875%, in Settings.
- The window is a waterfall display with Start, Stop, Save, Load, Configure, and About along the bottom. About shows this README, with the build version above it. The version is `0.1.0`, the build date and time, and the git revision. Save writes the recent audio in a format you choose. WAV and MP3 are always available, and a device that can encode AAC or Opus offers those too. Load opens a WAV or MP3 file and plays it through the waterfall. During playback the buttons are Play or Pause, and Live. New color columns appear at the right edge and older columns shift left immediately. Both scales draw shorter tick marks between the numbered labels. The frequency scale can be logarithmic, with a floor that defaults to 5 Hz.
- Zoom and pan. One finger or the left mouse button drags the picture. A sideways pinch zooms time, an up and down pinch zooms frequency, and a diagonal pinch zooms both. Ctrl and the wheel zoom time, Ctrl, Shift, and the wheel zoom frequency, and the keyboard has plus, minus, arrows, Home, and End. Capture continues while the view is back in time, and a Latest button returns to the live edge. The picture is drawn from the recorded sound, so only the part that is not yet on screen is computed after a pan or zoom.
- The time scale shows the clock (`HH:mm:ss`) during live capture and the position in the file (`mm:ss`) for a loaded file. Both scales move with the zoomed view.
- The Android launcher icon is the waterfall artwork, including an adaptive icon.
- `./upload-firebase.sh` publishes the signed APK to Firebase App Distribution. `./install-prereqs.sh --mode android` installs the Firebase CLI. `./build-android.sh --full` builds the unlocked APK for that upload, and About shows "Full version". A locked build is refused.
- Configure opens settings for the input and output devices, sample rate, FFT size, and heatmap palette. Changes are stored immediately. Save and Load do not do anything yet.
- Settings has an explicit Close button. On Android the page is full screen and the exit control is a Back button.
- Start captures the selected input, buffers the samples, and draws a Hann-windowed FFT column on the waterfall. Overlap defaults to 50% and can be changed in Settings. Columns are released on the hop interval so the scroll stays even.
- The Linux package installs a desktop entry and a waterfall icon, and the running window uses that icon in the dock.
- Settings can keep the screen on, and can limit the displayed frequency range. The plot has a frequency scale on the left and a time scale along the bottom.

### Changed
- Android builds use the same toolchain as ntpclock: Qt 6.11.3, NDK r27, JDK 21, build-tools 36.0.0. The app targets SDK 36 with minimum SDK 28 and 16 KB page alignment, which Google Play requires. The package name stays `com.kirayim.waterfall`. The minimum Android version rises from 6 to 9.
- `./build-android.sh` builds a signed Android App Bundle on every run, next to the signed APK, and checks the SDK levels, the package name, and the 16 KB alignment of both.
- Changing the frequency scale or its floor draws the waterfall again from the recorded sound instead of clearing it. Start now begins a fresh picture, and resizing the window draws the same sound again at the new size.

### Fixed
- Save on Android no longer fails when the chosen folder is a document location such as Downloads. The file is written with the grant from the system document picker.
- Time labels are spaced for the full width of the display from the first column, so they no longer pile up on the right while the plot is filling.
- Stop keeps the captured audio. Save still works after Stop. The next Start replaces that audio.

---

## [2.36.0] — 2026-06-09

### Added
- **Backwards compatibility policy** — every expert agent and playbook now checks for BC breaks before writing any code. A structured `⚠️ BC BREAK` notice (what changes, who is affected, severity, migration path) is required and blocks implementation until explicitly approved. Defined once in `BEST-PRACTICES.md`; enforced across all 9 experts, the Critic `[BC]` dimension, and 5 playbooks (add-feature, bug-fix, api-integration, release, refactor).
- **Critic `[BC]` review dimension** — 8th dimension added to the adversarial review. Checks API contract changes, field removals, env/config renames, exported interface changes, auth mechanism changes, and whether the BC notice was issued and approved. Non-migratable break = Critical; undocumented migratable break = High.
- **Team adoption presentation** — `presentation/team-adoption.html` (12-slide deck) and `presentation/STORY-PLAN.md` (story beats, live demo script, objection handling) for onboarding developer teams.

### Changed
- **Release playbook BC scan** — Step 3b now scans all commits since last tag for BC breaks before semver determination. Any BC break forces a Major bump; blocks if bump is downgraded without explicit approval.
- **Cursor multi-model documentation** — `.cursor/README.md` and `SYNC-POINTS.md` now document that model switches within Cursor (GPT-4o → Claude → Gemini) are invisible to the platform and should be treated as mini-handoffs: run session-end before switching models mid-task.

---

<!-- ─── VERSION HISTORY ─────────────────────────────────────────────────────────

Copy this block for each release (newest version always at the top):

## [X.Y.Z] — YYYY-MM-DD

### Added
- New capability that users can now do, didn't exist before

### Changed
- Existing behavior that works differently now (describe the delta, not the implementation)

### Deprecated
- Feature still works but will be removed in a future version — include migration path

### Removed
- Feature or endpoint removed; describe the replacement if one exists

### Fixed
- Bug that was affecting users: what broke, under what condition, now resolved

### Security
- Vulnerability patched (include CVE identifier if applicable)

─── AUTHORING RULES ──────────────────────────────────────────────────────────

✓  One line per change — if a change needs a paragraph, it belongs in the release notes
✓  User-visible only — skip internal refactors, test updates, CI tweaks unless they affect behavior
✓  Omit empty sections — if nothing was removed, drop the Removed section entirely
✓  Breaking changes → put in Removed or Changed AND mark clearly: "⚠ Breaking:"
✓  Link to issues/PRs inline where relevant: "Fixed crash on empty input (#123)"
✓  Use past tense: "Added", "Fixed", "Removed" — not "Adds", "Fixes", "Removes"
✓  Semver bump guide: Added/Changed new behavior = minor · Fixed only = patch · Breaking = major

─── EXAMPLE ENTRY ────────────────────────────────────────────────────────────

## [2.1.0] — 2026-03-15

### Added
- Export to PDF now supports password protection
- New `/api/v2/reports` endpoint with pagination and filtering

### Changed
- Dashboard load time reduced from ~4s to <400ms by switching to server-side pagination
- ⚠ Breaking: `/api/v1/reports` removed — migrate to `/api/v2/reports` (see docs/migration.md)

### Fixed
- Fixed crash when uploading files larger than 50 MB on slow connections (#412)
- Corrected timezone handling for users in UTC-offset regions (#389)

### Security
- Patched stored XSS vulnerability in comment field (CVE-2026-10234)

──────────────────────────────────────────────────────────────────────────────
-->

<!-- Version comparison links — update after each release (replace YOUR_ORG/YOUR_REPO) -->
<!-- [Unreleased]: https://github.com/YOUR_ORG/YOUR_REPO/compare/vLAST...HEAD -->
<!-- [X.Y.Z]:      https://github.com/YOUR_ORG/YOUR_REPO/compare/vPREV...vX.Y.Z -->
