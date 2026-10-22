<!-- source_hash: e0e630298e35 -->
# krkrz_android — Kirikiri Z for Android

[日本語](README.md) | English

A wrapper repository for running Kirikiri Z (krkrz) on Android. It builds with Gradle + the NDK
(CMake) and uses SDL3 for the system layer.

- The engine and plugin sources come from [krkrz_dev](https://github.com/wamsoft/krkrz_dev) through
  `KRKRZ_BASE`. This repository holds only the Android-specific build definitions and packaging
- The per-title settings (`app-config.json`) and assets can live in a separate folder (the project
  folder). This repository can also be built on its own as the bundled sample
- `cmake` (plugin configuration) and `assetPack` (asset collection) share their format with
  krkrz_ios / krkrz_linux / krkrz_gdk / krkrz_web

## Requirements

- Android SDK and NDK (the NDK version is `ndkVersion` in `app-config.json`)
- JDK 17 or later (needed to run Gradle 9)
- [vcpkg](https://github.com/microsoft/vcpkg) — set the environment variable `VCPKG_ROOT` to where it is installed
- [krkrz_dev](https://github.com/wamsoft/krkrz_dev) — clone it next to this repository and set the
  environment variable `KRKRZ_BASE` to their parent folder
- A shell with GNU Make (Git Bash / MSYS2 / WSL and so on; needed to use the Makefile)

## Setup

1. Clone krkrz_dev and this repository into the same folder

   ```
   <KRKRZ_BASE>/
   ├── krkrz_dev/        ← git clone --recursive https://github.com/wamsoft/krkrz_dev.git
   └── krkrz_android/    ← this repository
   ```

2. Set the environment variables `VCPKG_ROOT` and `KRKRZ_BASE`
3. Put the SDL3 `.aar` in `app/libs/`. Running `install-sdl-aar.sh` downloads a fixed version of SDL3
   and puts it there (every `.aar` in `app/libs/` is loaded automatically)
4. Write the Android SDK location (`sdk.dir`) in `local.properties` (opening the project in Android
   Studio creates it)

The engine is referenced at `${KRKRZ_BASE}/krkrz_dev/src/core` and the plugins at
`${KRKRZ_BASE}/krkrz_dev/src/plugins` (`app/src/main/cpp/CMakeLists.txt`).

## Building and running

```bash
make build      # build the sample bundled with this repository (Debug)
make run        # install it on the device and start it
make logcat     # show the log

make build PROJECT_DIR=/path/to/mygame   # build your own project folder
```

APKs are written to `${PROJECT_DIR}/build/android/app/outputs/apk/<variant>/`. The main targets are
below. All targets and variables, and how to call Gradle directly, are in
[docs/android-build.md](docs/android-build.md) (in Japanese).

| Task | Command |
|---|---|
| Build (Debug / Release) | `make build` / `make build BUILD_TYPE=Release` |
| AAB for Google Play | `make bundle BUILD_TYPE=Release` |
| Install and start / start waiting for a debugger | `make run` / `make debug` |
| Release configuration signed with the debug key | `make release-debug` |
| Remove the CMake outputs (after changing plugins or the project) | `make clean-cmake` |

## Project folder (PROJECT_DIR)

This repository is a shared build system; the settings and assets of the title you build can live
in a separate folder.

| Variable | Meaning |
|---|---|
| `BUILD_SYSTEM_DIR` | The root of this repository. The Makefile / settings.gradle determine it automatically |
| `PROJECT_DIR` | The project folder. `${PROJECT_DIR}/app-config.json` is read and output goes to `${PROJECT_DIR}/build/android/`. When not set, this repository itself (the sample) |
| `KRKRZ_BASE` | The parent folder of krkrz_dev |

- Relative paths in `app-config.json` are based on `PROJECT_DIR`. To point inside this repository, write `${BUILD_SYSTEM_DIR}/...`
- `local.properties` (SDK location and signing information) is a build-environment setting, so only the one at the root of this repository is read
- After switching projects, run `make clean-cmake` once (the previous CMake cache stays in `app/.cxx/`)

## app-config.json

Specifies the application ID, version, SDK, plugins and assets. The file at the root of the
repository is both the sample and the template. Each key is described in
[docs/android-config.md](docs/android-config.md) (in Japanese).

| Key | Description |
|---|---|
| `applicationId` / `versionCode` / `versionName` | Application ID and version |
| `compileSdk` / `targetSdk` / `minSdk` / `ndkVersion` | SDK and NDK versions |
| `apk.baseName` | The APK file name |
| `manifestPlaceholders` | Replacements for `${KEY}` in `AndroidManifest.xml` |
| `cmake` | Plugins to build in (format shared by the wrappers) |
| `assetPack` | Asset collection (format shared by the wrappers). `resOverride` overrides res (icon, app name) |
| `padPacks` | Large data delivered through Play Asset Delivery |

For the format of `cmake` / `assetPack` / `${...}` path expansion, see
[Shared settings of the platform wrappers](https://wamsoft.github.io/krkrz_dev/en/topics/core/platform_config/)
on the documentation site.

## Release and Google Play

| Task | Details (in Japanese) |
|---|---|
| Sign with the release key | [docs/android-signing.md](docs/android-signing.md) |
| Deliver large data through Play Asset Delivery (`padPacks`) | [docs/android-play-asset-delivery.md](docs/android-play-asset-delivery.md) |
| Use Google Play Games (sign-in, achievements, cloud save) | [docs/android-play-games.md](docs/android-play-games.md) |
| Screen reader (TalkBack) support (the `"a11y"` plugin, on by default) | [docs/android-accessibility.md](docs/android-accessibility.md) (Japanese) |
| Upload and publish to the Play Console from `make` | [docs/android-play-publishing.md](docs/android-play-publishing.md) |

Play Games is turned on or off just by including `"playgames"` in `cmake.plugins` of
`app-config.json` (when off, neither the dependency libraries nor the Manifest entries are included).

## Making it your own app

The app name, the icon (including the adaptive icon), your own Activity and adding JNI functions are
covered in [docs/android-customize.md](docs/android-customize.md) (in Japanese).

## Development features

- **REPL** — an interactive TJS shell over adb (`make repl`; Debug builds only). [docs/android-repl.md](docs/android-repl.md)
- **adb operations** — checking and deleting files on the device, app state. [docs/android-adb-tips.md](docs/android-adb-tips.md)

## Documentation

The documents under `docs/` are in Japanese.

| Document | Contents |
|---|---|
| [docs/android-build.md](docs/android-build.md) | All Makefile targets and variables, calling Gradle directly |
| [docs/android-config.md](docs/android-config.md) | Each key of `app-config.json`, specifying the project folder |
| [docs/android-signing.md](docs/android-signing.md) | Release signing, release builds signed with the debug key, signing in CI |
| [docs/android-customize.md](docs/android-customize.md) | App name, icon, your own Activity, JNI |
| [docs/android-play-asset-delivery.md](docs/android-play-asset-delivery.md) | Play Asset Delivery (delivering large data) |
| [docs/android-pad-fast-iteration.md](docs/android-pad-fast-iteration.md) | How `make pad-fast` works |
| [docs/android-play-games.md](docs/android-play-games.md) | Google Play Games Services |
| [docs/android-accessibility.md](docs/android-accessibility.md) | Screen reader (TalkBack) support (Japanese) |
| [docs/android-play-publishing.md](docs/android-play-publishing.md) | Automating publishing to the Google Play Console |
| [docs/android-jni-plugin.md](docs/android-jni-plugin.md) | Writing a JNI plugin |
| [docs/android-repl.md](docs/android-repl.md) | REPL over adb |
| [docs/android-adb-tips.md](docs/android-adb-tips.md) | adb operations |

The documentation for Kirikiri Z itself is on the [documentation site](https://wamsoft.github.io/krkrz_dev/en/).

## License

This repository (build scripts, the Android sources (Java / C++), tools and templates) is under the [MIT License](LICENSE).

The following, referenced or fetched at build time, follow their own licenses; the license of this
repository does not extend to them.

- The Kirikiri Z engine and plugins (each submodule under krkrz_dev)
- SDL3 (zlib License) and the dependency libraries fetched through vcpkg
- The Android SDK / NDK and the libraries fetched through Gradle (such as the Google Play libraries)

Include the license notices required in your distribution on the title side, according to the engine,
plugins and libraries you use.
