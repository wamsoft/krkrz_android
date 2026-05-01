#!/bin/bash

VERSION="3.4.4"
ZIP_FILE="SDL3-devel-$VERSION-android.zip"
URL="https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/$ZIP_FILE"
GROUP_ID="com.sdl"
ARTIFACT_ID="SDL3"

LIBS_DIR="$(pwd)/app/libs"
if [ ! -d "$LIBS_DIR" ]; then
  echo "Error: $LIBS_DIR directory does not exist."
  exit 1
fi

TEMP_DIR=$(mktemp -d)
echo "Working in $TEMP_DIR"

pushd "$TEMP_DIR" > /dev/null

echo "Downloading $URL..."
curl -L -O "$URL"

echo "Unzipping $ZIP_FILE..."
unzip -q "$ZIP_FILE"

# Find AAR
AAR_FILE=$(find . -name "*.aar" | head -n 1)

if [ -z "$AAR_FILE" ]; then
    echo "Error: AAR file not found."
    popd > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

echo "Found AAR: $AAR_FILE"

cp "$AAR_FILE" "$LIBS_DIR/"
echo "Copied $AAR_FILE to $LIBS_DIR"

popd > /dev/null
rm -rf "$TEMP_DIR"
