# SINX_STANDALONE_SWITCH_012: only ROM-independent runtime source files.
cmake_minimum_required(VERSION 3.18)
project(SiNXstralsStandalone C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
foreach(req SNES_ROOT SDL_SOURCE SDL_STATIC_LIB SDL_REVISION_INCLUDE)
  if(NOT DEFINED ${req})
    message(FATAL_ERROR "Required CMake variable missing: ${req}")
  endif()
endforeach()
set(R "${SNES_ROOT}/runner/src")
set(S "${R}/snes")
set(C "${CMAKE_CURRENT_BINARY_DIR}/compat")
file(MAKE_DIRECTORY "${C}")
# This pinned source's snes.c and cpu.c include a legacy SMW variables.h.
# Supply an empty, game-neutral shim; do not include any SMW code/data.
file(WRITE "${C}/variables.h" "#ifndef SINX_VARS_SHIM\n#define SINX_VARS_SHIM\n#endif\n")
set(SNES_DEVICE_SOURCES
  "${R}/cosim.c" "${R}/cosim_state.c" "${R}/audio_trace.c"
  "${R}/crc32.c" "${R}/sha256.c" "${R}/util.c"
  "${S}/apu.c" "${S}/cart.c" "${S}/cx4.c" "${S}/cpu.c"
  "${S}/dma.c" "${S}/dsp.c" "${S}/dsp1.c" "${S}/dsp1_hle.c"
  "${S}/joypad.c" "${S}/ppu.c" "${S}/ppu_legacy.c"
  "${S}/snes.c" "${S}/snes_other.c" "${S}/spc.c"
  "${S}/superfx.c" "${S}/ws_shadow.c" "${S}/interp816.c"
  "${S}/color_lut.c" "${S}/sa1.c" "${S}/sdd1.c")
add_executable(SiNXstralsStandalone
   "${CMAKE_CURRENT_LIST_DIR}/../src/sinx_switch_frontend.c"
   ${SNES_DEVICE_SOURCES})
target_include_directories(SiNXstralsStandalone PRIVATE
   "${SNES_ROOT}/cosim" "${R}" "${S}" "${C}"
   "${SDL_SOURCE}/include" "${SDL_REVISION_INCLUDE}")
target_compile_definitions(SiNXstralsStandalone PRIVATE
   SNES_COSIM=1 SNES_COSIM_REF=1 SNESRECOMP_TRACE=0
   SDL_MAIN_HANDLED=1 __SWITCH__=1)
# SINX_STANDALONE_012_PIC_FIX
# Compile every source in this target as relocation-safe AArch64 code.
# This applies to the interpreter AND all included SNES device C files.
target_compile_options(SiNXstralsStandalone PRIVATE
   -O2 -ffunction-sections -fdata-sections -Wno-implicit-function-declaration -fPIC)
# SINX_STANDALONE_012_CMAKE_LINKPATH_FIX
# Explicit library paths are also needed while CMake tests the cross compiler.
# Do not put the target-specific map file in global try-compile linker flags.
target_link_directories(SiNXstralsStandalone PRIVATE
   "$ENV{DEVKITPRO}/libnx/lib" "$ENV{DEVKITPRO}/portlibs/switch/lib")
target_link_options(SiNXstralsStandalone PRIVATE
   "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/standalone.map")
target_link_libraries(SiNXstralsStandalone PRIVATE
   "-Wl,--start-group" "${SDL_STATIC_LIB}"
   EGL GLESv2 glapi drm_nouveau stdc++ nx m z
   "-Wl,--end-group")
set_target_properties(SiNXstralsStandalone PROPERTIES
   OUTPUT_NAME "SiNXstralsStandalone" SUFFIX ".elf")
