set(ARM_FLAGS "-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${ARM_FLAGS}" CACHE STRING "Flags for C compiler" FORCE)
set(CMAKE_CXX_FLAGS "${ARM_FLAGS}" CACHE STRING "Flags for C++ compiler" FORCE)
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)