# Waterfall

Waterfall is a small Qt application for watching a live spectrogram. It reads the microphone, computes a Hann-windowed FFT, and scrolls the result across the window. New columns appear at the right edge and older ones move left. Low frequencies are at the bottom.

It builds for Linux (DEB), Windows (NSIS installer, cross-compiled from Linux), and Android (arm64 APK).

Save and Load are on the window but do not do anything yet.

## Using it

Press **Start** to capture the selected input and draw the waterfall. **Stop** ends capture. **Configure** opens Settings. Changes in Settings are stored immediately.

| Setting | What it does |
| --- | --- |
| Input / Output | Audio devices. Output is stored for later. Playback is not implemented. |
| Sample rate | 8 kHz through 192 kHz, limited to what the input device allows. Default 48 kHz. |
| FFT size | 1024, 2048, or 4096. Default 2048. |
| Overlap | 0%, 25%, 50%, 75%, or 87.5%. Default 50%. Higher overlap scrolls faster. |
| Frequency range | 4, 8, 12, 16, or 24 kHz, or the full range up to Nyquist. Default 8 kHz. Speech fits in 4–8 kHz. Birdsong usually needs more. |
| Keep screen on | Stops the display from sleeping while the app is open. On Android this is a window flag. On Linux it asks the session not to go idle. |
| Heatmap | Gray, Viridis, Magma, Inferno, Plasma, Turbo, Jet, Hot, or Cool. Default Viridis. |

Overlap and frequency range apply the next time you press Start. Keep screen on applies as soon as you toggle it.

The scale on the left is frequency. The scale along the bottom is time and moves with the scroll. The right edge is now.

The first time you start, or when you open Settings, the app asks for microphone access if the system has not granted it. On Android you can open the app's system settings from that dialog.

## Build

The scripts expect to be run from the repository root on Linux. CMake 4.4 or newer is required. `install-prereqs.sh` installs it when the system copy is older.

Linux uses the distribution Qt (`qt6-base-dev` and `qt6-multimedia-dev`). Windows and Android use Qt 6.6.3 from [aqtinstall](https://github.com/miurahr/aqtinstall), pinned in `scripts/prereqs.env`.

### Prerequisites

```bash
./install-prereqs.sh                  # Linux, Windows cross tools, and Android
./install-prereqs.sh --mode linux
./install-prereqs.sh --mode windows
./install-prereqs.sh --mode android
./install-prereqs.sh --dry-run
```

`--mode android` also installs a udev rule so adb can open a phone that is plugged in. Unplug and replug the phone after that.

### Packages

```bash
./build-linux.sh       # dist/waterfall-<version>-Linux.deb
./build-windows.sh     # dist/waterfall-<version>-win64.exe
./build-android.sh     # dist/waterfall-<version>-arm64-v8a.apk
```

The Linux package installs a desktop entry and icon, so the app shows up in the Ubuntu launcher and sidebar.

### Run from a build tree

```bash
./scripts/run-tests.sh
./build/linux-debug/waterfall
```

`./build-linux.sh` leaves a release binary at `build/linux-release/waterfall`.

### Tests

```bash
./scripts/run-tests.sh
```

`./build-linux.sh` runs the same tests against the release build before it packs the DEB.

### Android signing and install

The Android build signs the APK. The password is an environment variable. It is not stored in the repo.

```bash
export WATERFALL_KEYSTORE_PASSWORD='…'   # required, at least 6 characters
export WATERFALL_KEY_PASSWORD='…'        # only if the key password differs
export WATERFALL_KEYSTORE="$HOME/.android/waterfall-release.keystore"  # default
export WATERFALL_KEY_ALIAS=waterfall     # default
./build-android.sh
```

Create the key once, if you do not already have one:

```bash
keytool -genkeypair -v \
  -keystore "$HOME/.android/waterfall-release.keystore" \
  -alias waterfall \
  -keyalg RSA -keysize 2048 -validity 10000
```

`keytool` will not create a keystore with an empty password.

To install onto a phone over USB, turn on USB debugging, accept the computer prompt, then:

```bash
./install-apk.sh
```

With no argument it installs the newest signed APK in `dist/`. You can also pass a path.
