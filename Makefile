# Makefile for Android project using local gradlew
# Works best on Git Bash / MSYS2 / WSL

SHELL := /usr/bin/env bash

# -------- Configurable variables (can be overridden: make TARGET VAR=value) --------
MODULE ?= app
BUILD_TYPE ?= Debug         # Debug or Release
GRADLEOPT ?=
GRADLEW ?= ./gradlew $(GRADLEOPT)

# BUILD_SYSTEM_DIR: this repository (gradle / cmake / sources). Fixed.
# PROJECT_DIR     : per-app project folder (app-config.json + resources + build output).
#                   Defaults to BUILD_SYSTEM_DIR (sample mode) when env var is unset.
# Both are exported so gradle / cmake subprocesses see them.
BUILD_SYSTEM_DIR := $(realpath $(CURDIR))
PROJECT_DIR ?= $(BUILD_SYSTEM_DIR)
export BUILD_SYSTEM_DIR
export PROJECT_DIR

# Build output root under the per-project folder
BUILD_ROOT := $(PROJECT_DIR)/build/android

# Try to detect SDK dir from local.properties (Windows path -> POSIX path)
# local.properties always lives in BUILD_SYSTEM_DIR (it's about the build env, not the project).
SDK_DIR := $(shell grep -E '^sdk.dir=' "$(BUILD_SYSTEM_DIR)/local.properties" 2>/dev/null | head -n1 | cut -d'=' -f2- | sed 's/\\\\/\//g')
ADB_CAND := $(SDK_DIR)/platform-tools/adb
ADB ?= $(if $(wildcard $(ADB_CAND)),$(ADB_CAND),adb)

# Resolve APP_CONFIG_FILE (mirrors gradle: local.properties > env var > ${PROJECT_DIR}/app-config.json)
LOCAL_APP_CONFIG := $(shell grep -E '^APP_CONFIG_FILE=' "$(BUILD_SYSTEM_DIR)/local.properties" 2>/dev/null | head -n1 | cut -d'=' -f2-)
APP_CONFIG_FILE := $(or $(strip $(LOCAL_APP_CONFIG)),$(APP_CONFIG_FILE),$(PROJECT_DIR)/app-config.json)

# Read applicationId from the resolved app-config file (shell expands ${ENV_VAR} in path)
APP_ID ?= $(shell grep -o '"applicationId"[[:space:]]*:[[:space:]]*"[^"]*"' "$(APP_CONFIG_FILE)" 2>/dev/null | sed 's/.*"\([^"]*\)"$$/\1/')

GRADLE_ASSEMBLE := :$(MODULE):assemble$(BUILD_TYPE)
GRADLE_INSTALL  := :$(MODULE):install$(BUILD_TYPE)
GRADLE_BUNDLE   := :$(MODULE):bundle$(BUILD_TYPE)

.PHONY: help vars build assemble bundle install reinstall uninstall run debug stop logcat clean clean-cmake distclean test connected devices check-adb check-appid release-debug install-release-debug pad-install pad-fast pad-aab pad-package repl repl-py play-status play-upload play-promote play-rollout play-notes-new play-notes-check

help:
	@echo "Android Makefile (gradlew wrapper)"
	@echo "Targets:"
	@echo "  make build           - Assemble $(BUILD_TYPE) (assemble$(BUILD_TYPE))"
	@echo "  make install         - Install $(BUILD_TYPE) on device/emulator"
	@echo "  make release-debug   - Assemble Release APK signed with debug key (assembleReleaseDebugSigned)"
	@echo "  make install-release-debug - Install Release APK signed with debug key"
	@echo "  make pad-install     - Build AAB + install locally via bundletool --local-testing (PAD)"
	@echo "  make pad-fast        - Fast PAD iteration: reinstall code only, re-push changed packs"
	@echo "  make pad-aab         - Build the Play Store AAB (base + PAD asset packs)"
	@echo "  make pad-package     - Build a portable installer (app.apks + bundletool + scripts)"
	@echo "  make run             - Install and launch app (launcher activity)"
	@echo "  make debug           - Install and launch with debugger wait (-D)"
	@echo "  make uninstall       - Uninstall app via adb"
	@echo "  make logcat          - Tail adb logcat"
	@echo "  make repl            - Forward + connect to the on-device TJS REPL via nc (Debug build)"
	@echo "  make repl-py         - Same, using the richer Python client (edit/history/multiline)"
	@echo "  make clean           - Clean build (gradle clean)"
	@echo "  make clean-cmake     - Remove CMake artifacts (.cxx, intermediates/cxx, vcpkg installs)"
	@echo "  make distclean       - clean + clean-cmake (full reset)"
	@echo "  make test            - Run unit tests (test$(BUILD_TYPE)UnitTest)"
	@echo "  make connected       - Run instrumentation tests (connected$(BUILD_TYPE)AndroidTest)"
	@echo "  make devices         - List adb devices"
	@echo "  make play-status     - Show Play Console track state (read-only, no console needed)"
	@echo "  make play-upload     - Upload release AAB to a track (TRACK=internal ROLLOUT=... STATUS=...)"
	@echo "  make play-promote    - Promote a versionCode between tracks (FROM= TO= ROLLOUT=...)"
	@echo "  make play-rollout    - Change staged rollout (TRACK= FRACTION=... | HALT=1)"
	@echo "  make play-notes-new  - Generate release-note skeletons for all languages (VERSION=N)"
	@echo "  make play-notes-check- Verify release notes are complete and <=500 chars (VERSION=N)"
	@echo "  make vars            - Print resolved variables"
	@echo "Variables (override as needed): MODULE, BUILD_TYPE, APP_ID, ADB, GRADLEW"
	@echo "Examples:"
	@echo "  make build BUILD_TYPE=Release"
	@echo "  make run APP_ID=com.example.app"

vars:
	@echo MODULE=$(MODULE)
	@echo BUILD_TYPE=$(BUILD_TYPE)
	@echo GRADLEW=$(GRADLEW)
	@echo BUILD_SYSTEM_DIR=$(BUILD_SYSTEM_DIR)
	@echo PROJECT_DIR=$(PROJECT_DIR)
	@echo BUILD_ROOT=$(BUILD_ROOT)
	@echo SDK_DIR=$(SDK_DIR)
	@echo ADB=$(ADB)
	@echo APP_CONFIG_FILE=$(APP_CONFIG_FILE)
	@echo APP_ID=$(APP_ID)

check-adb:
	@command -v "$(ADB)" >/dev/null 2>&1 || { echo "ERROR: adb not found (ADB=$(ADB))"; exit 1; }

check-appid:
	@test -n "$(strip $(APP_ID))" || { echo "ERROR: APP_ID not detected. Override with APP_ID=com.example.app"; exit 1; }

# ----- Build & Install -----

build assemble:
	$(GRADLEW) $(GRADLE_ASSEMBLE)

bundle:
	$(GRADLEW) $(GRADLE_BUNDLE)

# ----- Play Asset Delivery (PAD) -----

# Build an AAB (base + asset packs) and install it locally via bundletool --local-testing.
# Uses BUILD_TYPE (default Debug). See docs/android-play-asset-delivery.md.
pad-install: check-adb
	VARIANT=$(BUILD_TYPE) PROJECT_DIR="$(PROJECT_DIR)" ./scripts/pad-local-install.sh

# Fast PAD iteration (Debug only): rebuild + reinstall the base APK (code) via a plain assembleDebug
# with -PpadLocalTesting, and re-push ONLY the asset packs whose content changed (reusing the packs
# already on the device otherwise). Run `make pad-install` once first to seed the packs + markers.
# See docs/android-play-asset-delivery.md and docs/android-adb-tips.md.
pad-fast: check-adb check-appid
	PROJECT_DIR="$(PROJECT_DIR)" APP_ID="$(APP_ID)" ./scripts/pad-fast-install.sh

# Build the Play Store AAB (default Release variant; override with BUILD_TYPE=...).
pad-aab:
	VARIANT=$(if $(filter Debug,$(BUILD_TYPE)),Release,$(BUILD_TYPE)) PROJECT_DIR="$(PROJECT_DIR)" ./scripts/pad-build-aab.sh

# Build a portable installer folder (app.apks + bundletool + install scripts) that can be
# copied to any machine with Java + adb and installed without this repo. See the script.
pad-package:
	VARIANT=$(BUILD_TYPE) PROJECT_DIR="$(PROJECT_DIR)" ./scripts/pad-package-installer.sh

# ----- Google Play Console 公開自動化 (Play Developer API 直叩き) -----
# 前提: サービスアカウント鍵を local.properties の PLAY_SERVICE_ACCOUNT_JSON に設定 +
#       python -m pip install -r scripts/requirements-play.txt。詳細 docs/android-play-publishing.md。
PY ?= python
TRACK ?= internal
PLAY_PUBLISH := PROJECT_DIR="$(PROJECT_DIR)" BUILD_SYSTEM_DIR="$(BUILD_SYSTEM_DIR)" APP_CONFIG_FILE="$(APP_CONFIG_FILE)" $(PY) "$(BUILD_SYSTEM_DIR)/scripts/play-publish.py"

# 各トラックの現状 (versionCode / 状態 / 公開率 / ノート) を照会 (読み取り専用・コンソール不要)
play-status:
	$(PLAY_PUBLISH) status

# release AAB をアップロードしトラックへ割り当て。TRACK=(既定 internal) STATUS= ROLLOUT= AAB= で調整。
#   例: make play-upload                       (internal へ全公開)
#       make play-upload TRACK=production ROLLOUT=0.1   (本番へ10%段階公開)
play-upload:
	$(PLAY_PUBLISH) upload --track "$(TRACK)" $(if $(STATUS),--status "$(STATUS)",) $(if $(ROLLOUT),--rollout "$(ROLLOUT)",) $(if $(AAB),--aab "$(AAB)",) $(if $(DRYRUN),--dry-run,)

# 既存 versionCode を別トラックへ昇格 (再アップロード不要)。FROM= TO= (必須) ROLLOUT= 任意。
#   例: make play-promote FROM=internal TO=production ROLLOUT=0.1
play-promote:
	$(PLAY_PUBLISH) promote --from "$(FROM)" --to "$(TO)" $(if $(ROLLOUT),--rollout "$(ROLLOUT)",) $(if $(DRYRUN),--dry-run,)

# 段階公開率の増減 / 完了 / 停止。TRACK=(既定 production) FRACTION= または HALT=1。
#   例: make play-rollout FRACTION=0.5 / make play-rollout FRACTION=1.0 / make play-rollout HALT=1
play-rollout: TRACK := production
play-rollout:
	$(PLAY_PUBLISH) rollout --track "$(TRACK)" $(if $(HALT),--halt,--fraction "$(FRACTION)") $(if $(DRYRUN),--dry-run,)

# 全サポート言語のリリースノート雛形を一括生成 / 検査。VERSION=(既定 app-config.json の versionCode)。
play-notes-new:
	$(PLAY_PUBLISH) notes-new $(if $(VERSION),--version "$(VERSION)",)

play-notes-check:
	$(PLAY_PUBLISH) notes-check $(if $(VERSION),--version "$(VERSION)",)

# Release APK signed with the debug key (no release keystore required)
release-debug:
	$(GRADLEW) :$(MODULE):assembleReleaseDebugSigned

install-release-debug: check-adb
	$(GRADLEW) :$(MODULE):installReleaseDebugSigned

install: check-adb
	$(GRADLEW) $(GRADLE_INSTALL)

reinstall: check-adb
	-$(ADB) uninstall "$(APP_ID)" >/dev/null 2>&1 || true
	$(GRADLEW) $(GRADLE_INSTALL)

uninstall: check-adb check-appid
	-$(ADB) uninstall "$(APP_ID)" || true

# ----- Run & Debug -----

run: install check-adb check-appid
	@echo "Launching $(APP_ID) via monkey..."
	$(ADB) shell monkey -p "$(APP_ID)" -c android.intent.category.LAUNCHER 1

# Start MAIN/LAUNCHER with debugger (-D). Does not require explicit activity name.
debug: install check-adb check-appid
	@echo "Launching $(APP_ID) in debug (waiting for debugger)..."
	$(ADB) shell am start -D \
	  -a android.intent.action.MAIN \
	  -c android.intent.category.LAUNCHER \
	  -p "$(APP_ID)"
	@echo "Now attach your Java debugger to the app process."

stop: check-adb check-appid
	-$(ADB) shell am force-stop "$(APP_ID)" || true

logcat: check-adb
	$(ADB) logcat

# ----- REPL (adb 経由の TJS 対話シェル, Debug ビルド専用) -----
# アプリ (Debug) が起動中に叩く。socket 名は KrkrzActivity と同じ規則 <APP_ID>.repl。
# TCP 転送してから nc で接続する (nc が無ければ転送だけして案内を出す)。
REPL_PORT ?= 9099
repl: check-adb check-appid
	$(ADB) forward tcp:$(REPL_PORT) localabstract:$(APP_ID).repl
	@echo "REPL forwarded: tcp:$(REPL_PORT) -> localabstract:$(APP_ID).repl"
	@echo "(Debug ビルドで起動中のこと: make run。 1 行 TJS を入力して Enter、Ctrl+C で終了)"
	@if command -v nc >/dev/null 2>&1; then \
		nc 127.0.0.1 $(REPL_PORT); \
	else \
		echo "nc が見つかりません。任意の TCP クライアントで 127.0.0.1:$(REPL_PORT) へ接続してください"; \
	fi

# Python 版クライアント (行編集 / 履歴 / 複数行 / 外部エディタ / 再接続)。
# forward はここ (git bash) で行い、client は port に繋ぐだけにする
# (Windows の python へ posix パスの adb を渡さずに済む)。
PYTHON ?= python
repl-py: check-adb check-appid
	$(ADB) forward tcp:$(REPL_PORT) localabstract:$(APP_ID).repl
	$(PYTHON) scripts/krkrz-repl.py --port $(REPL_PORT)

# ----- Clean & Tests -----

clean:
	$(GRADLEW) clean

# Remove CMake-generated artifacts (configure cache, build files, vcpkg installs).
# Use this when CMake state goes weird (e.g. stale plugin paths after editing app-config.json
# cmake section, vcpkg cache corruption, ABI/triplet mismatches, etc.). The next build will
# fully reconfigure CMake (which means re-downloading vcpkg deps too — slower).
# NOTE: .cxx/ is created by AGP under the module directory and cannot be relocated, so it
# stays under ${BUILD_SYSTEM_DIR}/app/.cxx regardless of PROJECT_DIR. When switching projects,
# this must be cleaned to avoid stale CMake/vcpkg state from the previous project.
clean-cmake:
	@echo "Removing CMake artifacts..."
	rm -rf "$(BUILD_SYSTEM_DIR)/app/.cxx"
	rm -rf "$(BUILD_ROOT)/app/intermediates/cxx"
	rm -rf "$(BUILD_ROOT)/app/intermediates/cmake"
	rm -rf "$(BUILD_ROOT)/app/generated/cmake"

# Full reset: gradle clean + CMake artifacts. Equivalent to a fresh checkout build.
distclean: clean clean-cmake
	@echo "Full clean done. Next build will re-run vcpkg install and CMake configure."

test:
	$(GRADLEW) :$(MODULE):test$(BUILD_TYPE)UnitTest

connected: check-adb
	$(GRADLEW) :$(MODULE):connected$(BUILD_TYPE)AndroidTest

# ----- Utilities -----

devices: check-adb
	$(ADB) devices -l
