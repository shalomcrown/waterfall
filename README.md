# Waterfall

Waterfall is a small Qt application for watching a live spectrogram. It reads the microphone, computes a Hann-windowed FFT, and scrolls the result across the window. New columns appear at the right edge and older ones move left. Low frequencies are at the bottom.

It builds for Linux (DEB), Windows (NSIS installer, cross-compiled from Linux), and Android (arm64 APK and Android App Bundle).

Source: https://github.com/shalomcrown/waterfall

**Save** writes the recent audio in a format you choose. WAV and MP3 are always available. A device that can encode AAC or Opus offers those as well. Stop keeps that audio in memory so you can still save it. The next Start replaces it.

**Load** opens a WAV or MP3 file. The first button becomes **Play**, and **Pause** while the file is playing. The waterfall follows the file. **Live** stops the file and switches back to the microphone.

MP3 is encoded with [LAME](https://lame.sourceforge.io/) 3.100, which is licensed under the LGPL-2.1. The build downloads that release and compiles it in. The source of that release is the corresponding source offer. A sample rate LAME cannot encode, including 96 kHz and 192 kHz, is refused, and WAV is still available.

## Using it

Press **Start** to capture the selected input and draw the waterfall. **Stop** ends capture. **Configure** opens Settings. **About** shows this document, with the build version above it. Changes in Settings are stored immediately.

| Setting | What it does |
| --- | --- |
| Input / Output | Audio devices. Output is stored for later. Playback is not implemented. |
| Sample rate | 8 kHz through 192 kHz, limited to what the input device allows. Default 48 kHz. |
| FFT size | 1024 up to 32768. Default 2048. A longer window separates frequencies more finely (the bins are the sample rate divided by the size, so 48 kHz at 2048 gives 23 Hz). |
| Overlap | 0%, 25%, 50%, 75%, 87.5%, 93.75%, or 96.875%. Default 50%. Higher overlap scrolls faster. |
| Frequency range | 4, 8, 12, 16, or 24 kHz, or the full range up to Nyquist. Default 8 kHz. Speech fits in 4–8 kHz. Birdsong usually needs more. |
| Frequency scale | Linear or logarithmic. Default linear. Logarithmic gives the low end more of the picture. |
| Log floor | Lowest frequency on a logarithmic scale: 5, 10, 20, 50, or 100 Hz. Default 5 Hz. |
| Save window | How much recent audio Save keeps: 1, 2, 5, 10, or 30 minutes. Default 5 minutes. |
| Save format | WAV and MP3 on every platform. AAC and Opus when this device can encode them. Default WAV. Save remembers the last choice. |
| Keep screen on | Stops the display from sleeping while the app is open. On Android this is a window flag. On Linux it asks the session not to go idle. |
| Heatmap | Gray, Viridis, Magma, Inferno, Plasma, Turbo, Jet, Hot, or Cool. Default Viridis. |

Overlap, frequency range, and the save window apply the next time you press Start. Keep screen on applies as soon as you toggle it. The frequency scale and its floor apply when you close Settings, and the waterfall is drawn again from the recorded sound so the colors match the axis. Save uses the sample rate the device actually captured. Choosing an existing file replaces that file. On Android, Save asks for the format and then opens the system document picker, so a folder such as Downloads can be written.

The scale on the left is frequency. The scale along the bottom is time. During live capture it shows the clock, as `HH:mm:ss`, or `HH:mm` when the labels are a minute or more apart, or with milliseconds when they are less than a second apart. When a file is loaded it shows the position in the file instead, as `mm:ss`. The right edge is now, and the left edge is the oldest time that width can show. Shorter tick marks sit between the numbered labels on both scales. A logarithmic frequency scale starts at the log floor instead of 0 Hz.

### Zooming and panning

The picture is drawn from the recorded sound, not from a stored image. When you pan or zoom, only the part of the picture that is not already on screen is computed again. Capture does not stop while you look around, and Save keeps everything in the save window.

| | Touch screen | Mouse and keyboard |
|---|---|---|
| Pan | Drag with one finger. | Drag with the left button, or use the wheel, or the Left and Right keys. |
| Zoom time | Pinch sideways. | Ctrl and the wheel, or the plus and minus keys. |
| Zoom frequency | Pinch up and down. | Ctrl, Shift, and the wheel, or Shift with the plus and minus keys. |
| Zoom both | Pinch diagonally. | |
| Back to normal | Tap Latest. | Double-click or Home. |

Panning back in time pins the view. A Latest button appears at the top right, and Latest or the End key returns to the live edge. Panning forward to the live edge, or zooming so the right edge reaches it, also returns to following. Zoomed in, a column is drawn wider once it cannot show any more detail. Zoomed out, the columns are taken further apart, so a short sound can fall between them. The time scale and the frequency scale move with the picture.

Zooming in on frequency also gives the picture a longer FFT: each halving of the shown frequency span doubles the window, up to 32768 samples, so the bins get narrower as fast as the picture is magnified. The spacing of the columns does not change, so the overlap rises by itself. A note at the top left always gives the FFT size, the width of a bin in Hz, and the overlap. While the zoom is not the configured one, a Reset zoom button appears under it (double-click or the Home key do the same), and it returns to the FFT size and overlap chosen in Settings, the whole frequency axis, and the live edge. Zooming back out shortens the window again. A longer window cannot be drawn for the first moments of a recording, which stay blank until enough sound exists, and a very long window takes longer to draw again. When a column has more frequency bins than the screen has pixel rows, each row shows the strongest bin it covers, so a narrow peak stays visible.

Only sound that is still in the save window can be drawn again, so panning back stops at the oldest sound it holds. A file is drawn from the whole file once it has started playing, so you can pan to any part that has played.

The first time you start, or when you open Settings, the app asks for microphone access if the system has not granted it. On Android you can open the app's system settings from that dialog.

## Build

The scripts expect to be run from the repository root on Linux. CMake 4.4 or newer is required. `install-prereqs.sh` installs it when the system copy is older.

Linux uses the distribution Qt (`qt6-base-dev` and `qt6-multimedia-dev`). Windows uses Qt 6.6.3 and Android uses Qt 6.11.3, both from [aqtinstall](https://github.com/miurahr/aqtinstall) and pinned in `scripts/prereqs.env`. The Android pins match the ntpclock project, so one install serves both: Qt 6.11.3, NDK r27 (27.2.12479018), JDK 21, build-tools 36.0.0, compile and target SDK 36, minimum SDK 28, and 16 KB page alignment, as Google Play requires.

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
./build-android.sh           # dist/waterfall-<version>-arm64-v8a.apk and .aab
./build-android.sh --full    # dist/waterfall-<version>-full-arm64-v8a.apk and .aab
```

Each build stamps `0.1.0.YYYYMMDD.HHMM.<git>` into the binary and the package name. `waterfall --version` prints the same string.

The Linux package installs a desktop entry and icon, so the app shows up in the Ubuntu launcher and sidebar. The Android launcher icon is the same artwork. `install-prereqs.sh --mode android` installs ImageMagick, which the Android build uses to turn that SVG into the launcher sizes.

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

Every Android build produces both a signed APK, for USB install and Firebase, and a signed App Bundle (`.aab`), which is what Google Play accepts. The script then checks that both are zip-aligned for 16 KB pages, that every native library is 16 KB aligned, and that the APK targets SDK 36 with minimum SDK 28 under the package `com.kirayim.waterfall`. The password is an environment variable. It is not stored in the repo.

```bash
export WATERFALL_KEYSTORE_PASSWORD='…'   # required, at least 6 characters
export WATERFALL_KEY_PASSWORD='…'        # only if the key password differs
export WATERFALL_KEYSTORE="$HOME/.android/waterfall-release.keystore"  # default
export WATERFALL_KEY_ALIAS=waterfall     # default
./build-android.sh            # locked build, for Google Play
./build-android.sh --full     # full version, for Firebase
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

To publish that APK to Firebase App Distribution:

```bash
firebase login          # once per machine
./upload-firebase.sh    # newest signed full APK in dist/
```

`--full` compiles the unlocked app. About shows "Full version" under the build stamp. `./upload-firebase.sh` accepts only that APK. A build without `--full` stays locked, which is the one for Google Play.

The default Firebase Android app is `1:199263152632:android:4db0f5d1770b2da1254e0d`. `--app` or `FIREBASE_APP_ID` selects a different one. The APK's package name, `com.kirayim.waterfall`, has to be the package registered for that Firebase app.

With no `--groups` or `--testers`, the build is shared with the testers already on the project. When the project has no testers, the APK is still uploaded and is not shared. `--groups` takes the group alias, not the display name. A missing alias makes Firebase return 404 after the upload, so the script checks the alias first. `--notes` or `--notes-file` sets the release notes. A CI login can use `FIREBASE_TOKEN` from `firebase login:ci`, or `GOOGLE_APPLICATION_CREDENTIALS` for a service account. The token is not stored in the repo.

`./install-prereqs.sh --mode android` installs the Firebase CLI.
