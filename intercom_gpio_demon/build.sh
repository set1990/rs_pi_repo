#!/bin/bash
set -e
TOOLCHAIN=./toolchains/rpi-zero2w.cmake
BUILD_DIR=build
TARGET_USER=root
TARGET_RPI=RspiZ.local
TARGET_PATH_APP=/root
TARGET_PATH_LIB=/lib
APP_NAME=intercom_gpio_demon
DEBUG=Release
CLEAN=0
TARGET=0
ONLY=0
RUN=0

function usage() {
   echo "Options:"
   echo " -d|--debug : build with symbols"
   echo " -c|--clean : clean build result"
   echo " -t|--target : deploy on target"
   echo " -r|--run : run gdb serwer"
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
      -d|--debug)
         DEBUG=Debug
         shift
         ;;
      -c|--clean)
         CLEAN=1
         shift
         ;;
      -t|--target)
         TARGET=1
         shift
         ;;
      -r|--run)
         RUN=1
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
   echo "Building type $DEBUG..."
   cmake -B $BUILD_DIR -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN -DCMAKE_BUILD_TYPE=$DEBUG -DCMAKE_CXX_FLAGS="-O0" 
   cmake --build $BUILD_DIR
   echo "Build finished"
fi

if [[ $TARGET -eq 1 ]]; then
   echo "Deploying to $TARGET_RPI..."
   scp "$BUILD_DIR/$APP_NAME" "$TARGET_USER@$TARGET_RPI:$TARGET_PATH_APP/"
   scp "run_debug.sh" "$TARGET_USER@$TARGET_RPI:$TARGET_PATH_APP/"
   echo "Deployment finished"
fi

if [[ $RUN -eq 1 ]]; then
   echo "Run for debug on $TARGET_RPI..."
   ssh $TARGET_USER@$TARGET_RPI "export LD_LIBRARY_PATH=/root && ./run_debug.sh"
fi


