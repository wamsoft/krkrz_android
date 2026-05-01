# Makefile for Android project using local gradlew
# Works best on Git Bash / MSYS2 / WSL

SHELL := /usr/bin/env bash

# -------- Configurable variables (can be overridden: make TARGET VAR=value) --------
MODULE ?= app
BUILD_TYPE ?= Debug         # Debug or Release
GRADLEOPT ?=
GRADLEW ?= ./gradlew $(GRADLEOPT)

# Try to detect SDK dir from local.properties (Windows path -> POSIX path)
SDK_DIR := $(shell grep -E '^sdk.dir=' local.properties 2>/dev/null | head -n1 | cut -d'=' -f2- | sed 's/\\\\/\//g')
ADB_CAND := $(SDK_DIR)/platform-tools/adb
ADB ?= $(if $(wildcard $(ADB_CAND)),$(ADB_CAND),adb)

# Read applicationId from app-config.json
APP_ID ?= $(shell grep -o '"applicationId"[[:space:]]*:[[:space:]]*"[^"]*"' app-config.json 2>/dev/null | sed 's/.*"\([^"]*\)"$$/\1/')

GRADLE_ASSEMBLE := :$(MODULE):assemble$(BUILD_TYPE)
GRADLE_INSTALL  := :$(MODULE):install$(BUILD_TYPE)
GRADLE_BUNDLE   := :$(MODULE):bundle$(BUILD_TYPE)

.PHONY: help vars build assemble bundle install reinstall uninstall run debug stop logcat clean test connected devices check-adb check-appid release-debug install-release-debug

help:
	@echo "Android Makefile (gradlew wrapper)"
	@echo "Targets:"
	@echo "  make build           - Assemble $(BUILD_TYPE) (assemble$(BUILD_TYPE))"
	@echo "  make install         - Install $(BUILD_TYPE) on device/emulator"
	@echo "  make release-debug   - Assemble Release APK signed with debug key (assembleReleaseDebugSigned)"
	@echo "  make install-release-debug - Install Release APK signed with debug key"
	@echo "  make run             - Install and launch app (launcher activity)"
	@echo "  make debug           - Install and launch with debugger wait (-D)"
	@echo "  make uninstall       - Uninstall app via adb"
	@echo "  make logcat          - Tail adb logcat"
	@echo "  make clean           - Clean build"
	@echo "  make test            - Run unit tests (test$(BUILD_TYPE)UnitTest)"
	@echo "  make connected       - Run instrumentation tests (connected$(BUILD_TYPE)AndroidTest)"
	@echo "  make devices         - List adb devices"
	@echo "  make vars            - Print resolved variables"
	@echo "Variables (override as needed): MODULE, BUILD_TYPE, APP_ID, ADB, GRADLEW"
	@echo "Examples:"
	@echo "  make build BUILD_TYPE=Release"
	@echo "  make run APP_ID=com.example.app"

vars:
	@echo MODULE=$(MODULE)
	@echo BUILD_TYPE=$(BUILD_TYPE)
	@echo GRADLEW=$(GRADLEW)
	@echo SDK_DIR=$(SDK_DIR)
	@echo ADB=$(ADB)
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

# ----- Clean & Tests -----

clean:
	$(GRADLEW) clean

test:
	$(GRADLEW) :$(MODULE):test$(BUILD_TYPE)UnitTest

connected: check-adb
	$(GRADLEW) :$(MODULE):connected$(BUILD_TYPE)AndroidTest

# ----- Utilities -----

devices: check-adb
	$(ADB) devices -l
