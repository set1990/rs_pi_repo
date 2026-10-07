#!/bin/bash
set -e

TOOLCHAIN=../toolchains/rpi-zero2w.cmake
BUILD_DIR=build
LIB_NAME=libipc
BUILD_TYPE=Release
TARGET_PATH=../intercom_gpio_demon/external/libipc
H_PATH=include
CLEAN=0
ONLY=0
TARGET=0

function usage() {
   echo "Options:"
   echo " -c|--clean : clean build result"
   echo " -t|--target : deploy on target"
   echo " -o|--only : without build"
   echo " -h|--help : displays this message"
   echo ""
}

while [[ $# -gt 0 ]]; do
   key="$1"

   case $key in
      -h|--help)
         usage
         exit 0
         ;;
      -c|--clean)
         CLEAN=1
         shift
         ;;
      -t|--target)
         TARGET=1
         shift
         ;;
      -o|--only)
         ONLY=1
         shift
         ;;
      *)    # unknown option
         echo "Unkown option: $key"
         usage
         exit 1
         ;;
   esac
done


if [[ $CLEAN -eq 1 ]]; then
   echo "Cleaning result" 
   rm -rf build
   echo "Clean finished"
fi

if [[ $ONLY -eq 0 ]]; then
   echo "Building library $LIB_NAME ($BUILD_TYPE)..."
   
   cmake -B $BUILD_DIR \
         -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN \
         -DCMAKE_BUILD_TYPE=$BUILD_TYPE
         
   cmake --build $BUILD_DIR
   
   echo "Library $LIB_NAME build finished successfully."
fi

if [[ $TARGET -eq 1 ]]; then
   echo "Deploying to $TARGET_RPI..." 
   cp "$BUILD_DIR/$LIB_NAME.a" "$TARGET_PATH"
   cp "$H_PATH/$LIB_NAME.h" "$TARGET_PATH"
   echo "Deployment finished"
fi