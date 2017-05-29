cmake_minimum_required(VERSION 2.8)

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif ()

project(openH264wrapper)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)

message("DEBUG: CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message("DEBUG: CMAKE_SYSTEM_NAME      = ${CMAKE_SYSTEM_NAME}")

macro (my_check_function_exists arg result)
  check_function_exists(${arg} ${result})
  if(${result} STREQUAL "")
    set(${result} 0)
  endif()
endmacro (my_check_function_exists)

macro (my_check_include_files arg result)
  check_include_files(${arg} ${result})
  if(${result} STREQUAL "")
    set(${result} 0)
  endif()
endmacro (my_check_include_files)

include(CheckTypeSize)
check_type_size("void*" SIZEOF_VOID_P BUILTIN_TYPES_ONLY)
message("DEBUG: SIZEOF_VOID_P      = ${SIZEOF_VOID_P}")
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(OptimizeForArchitecture)
OptimizeForArchitecture()

my_check_function_exists(GetProcessAffinityMask GETPROCESSAFFINITYMASK_FOUND)
my_check_function_exists(gettimeofday           GETTIMEOFDAY_FOUND)
my_check_function_exists(sched_getaffinity      SCHED_GETAFFINITY_FOUND)
my_check_function_exists(strerror_r             STRERROR_R_FOUND)
my_check_function_exists(sysconf                SYSCONF_FOUND)
my_check_function_exists(usleep                 USLEEP_FOUND)
my_check_function_exists(localtime_r            LOCALTIME_R_FOUND)
my_check_function_exists(gmtime_r               GMTIME_R_FOUND)

my_check_include_files(fcntl.h                  FCNTL_H_FOUND)
my_check_include_files(pthread.h                PTHREADS_FOUND)
my_check_include_files(unistd.h                 UNISTD_H_FOUND)
my_check_include_files(windows.h                WINDOWS_H_FOUND)

#find asm compiler
option (USE_YASM "Use YASM. If YASM is not enabled the assembly implementation will be disabled." ON)
if (USE_YASM)
  find_package (Yasm)
endif ()

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
    configure_file(platform/arm/config.h ${PROJECT_BINARY_DIR}/config.h)
else()
  configure_file(platform/x86/config.h.in ${PROJECT_BINARY_DIR}/config.h)
  configure_file(platform/x86/config.asm.in ${PROJECT_BINARY_DIR}/config.asm)
endif()

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
	enable_language(ASM)
	add_definitions(
		-DEXTERN_ASM=
	)
endif()

if(WIN32)
    add_definitions(
        -Dsnprintf=avpriv_snprintf
        -Dvsnprintf=avpriv_vsnprintf
        -Dinline=__inline
        -Drestrict=__restrict
    )
endif()

#define asm sources
if(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )
if(YASM_FOUND)
set(YASM_NAMES
    libavutil/x86/cpuid.asm
    libavutil/x86/emms.asm
    libavcodec/x86/h264_chromamc.asm
    libavcodec/x86/h264_chromamc_10bit.asm
    libavcodec/x86/h264_deblock.asm
    libavcodec/x86/h264_deblock_10bit.asm
    libavcodec/x86/h264_idct.asm
    libavcodec/x86/h264_idct_10bit.asm
    libavcodec/x86/h264_weight.asm
    libavcodec/x86/h264_weight_10bit.asm
    libavcodec/x86/h264_intrapred.asm
    libavcodec/x86/h264_intrapred_10bit.asm
    libavcodec/x86/h264_qpel_8bit.asm
    libavcodec/x86/h264_qpel_10bit.asm
    libavcodec/x86/fpel.asm
    libavcodec/x86/qpel.asm

)
endif(YASM_FOUND)
endif(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )

if(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )
set(COMMON_YASM_ARGS
    -I./
    -I "${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${PROJECT_BINARY_DIR}/config.asm"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/libavfilter/x86/"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/libavutil/x86/"
    -DPIC
)
endif(NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l") )

if(YASM_FOUND)
if(APPLE)
    set(YASM_ARGS -f macho64 -m amd64 -DPREFIX ${COMMON_YASM_ARGS})
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    set(YASM_ARGS -f elf     -m amd64 ${COMMON_YASM_ARGS})
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
    set(YASM_ARGS -f elf     -DX86_32 ${COMMON_YASM_ARGS})
else()
    set(YASM_ARGS -f win32   -m amd64 ${COMMON_YASM_ARGS})
endif()

#compile all asm files
foreach(_asm_file ${YASM_NAMES})
    set(YASM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/${_asm_file}")
    get_filename_component(BASENAME ${YASM_SRC} NAME_WE)
    set(YASM_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.o")
    add_custom_command(
        OUTPUT ${YASM_OBJ}
        COMMAND "${YASM_EXECUTABLE}"
        ARGS ${YASM_ARGS} -o ${YASM_OBJ} ${YASM_SRC}
        DEPENDS ${YASM_SRC}
    )
    set(YASM_OBJECTS ${YASM_OBJECTS} ${YASM_OBJ})
endforeach()
endif(YASM_FOUND)

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    if(MINGW)
        AddCompilerFlag("-arch x86_64 -m64" C_FLAGS Vc_ARCHITECTURE_FLAGS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-Bsymbolic")
    elseif(NOT APPLE)
        string(REGEX MATCH "clang*" CLANG_COMPILER "${CMAKE_C_COMPILER}")
        if ("${CLANG_COMPILER}" STREQUAL "clang")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
        else()
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-Bsymbolic")
        endif()
    endif()
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
    #add_definitions("-DX86_32")
endif()

add_definitions("-DPIC")
AddCompilerFlag("-fpic" C_FLAGS Vc_ARCHITECTURE_FLAGS)
AddCompilerFlag("-fno-tree-vectorize" C_FLAGS Vc_ARCHITECTURE_FLAGS)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${Vc_ARCHITECTURE_FLAGS}")

project(libavutil)
set(libfilenames
    libavutil/avstring.c
    libavutil/atomic.c
    libavutil/base64.c
    libavutil/bprint.c
    libavutil/buffer.c
    libavutil/channel_layout.c
    libavutil/cpu.c
    libavutil/crc.c
    libavutil/des.c
    libavutil/dict.c
    libavutil/display.c
    libavutil/error.c
    libavutil/eval.c
    libavutil/file_open.c
    libavutil/frame.c
    libavutil/imgutils.c
    libavutil/intmath.c
    libavutil/log.c
    libavutil/log2_tab.c
    libavutil/mathematics.c
    libavutil/md5.c
    libavutil/mem.c
    libavutil/opt.c
    libavutil/parseutils.c
    libavutil/pixdesc.c
    libavutil/rational.c
    libavutil/random_seed.c
    libavutil/rc4.c
    libavutil/samplefmt.c
    libavutil/sha.c
    libavutil/stereo3d.c
    libavutil/time.c
    libavutil/timecode.c
    libavutil/utils.c
    libavcodec/allcodecs.c
    libavcodec/audioconvert.c
    libavcodec/avdct.c
    libavcodec/avpacket.c
    libavcodec/avpicture.c
    libavcodec/bitstream.c
    libavcodec/bitstream_filter.c
    libavcodec/bitstream_filters.c
    libavcodec/bsf.c
    libavcodec/codec_desc.c
    libavcodec/d3d11va.c
    libavcodec/dirac.c
    libavcodec/dv_profile.c
    libavcodec/imgconvert.c
    libavcodec/jni.c
    libavcodec/mathtables.c
    libavcodec/mediacodec.c
    libavcodec/mpeg12framerate.c
    libavcodec/options.c
    libavcodec/mjpegenc_huffman.c
    libavcodec/parser.c
    libavcodec/profiles.c
    libavcodec/qsv_api.c
    libavcodec/raw.c
    libavcodec/resample.c
    libavcodec/resample2.c
    libavcodec/utils.c
    libavcodec/vorbis_parser.c
    libavcodec/xiph.c
    libavcodec/h264dec.c
    libavcodec/h264_cabac.c
    libavcodec/h264_cavlc.c
    libavcodec/h264_direct.c
    libavcodec/h264_loopfilter.c
    libavcodec/h264_mb.c
    libavcodec/h264_picture.c
    libavcodec/h264_refs.c
    libavcodec/h264_sei.c
    libavcodec/h264_slice.c
    libavcodec/h264data.c)

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
list(APPEND libfilenames
    libavutil/arm/cpu.c
    libavutil/arm/asm.S
    libavcodec/arm/h264chroma_init_arm.c
    libavcodec/arm/h264dsp_init_arm.c
    libavcodec/arm/h264pred_init_arm.c
    libavcodec/arm/h264qpel_init_arm.c
    libavcodec/arm/h264cmc_neon.S
    libavcodec/arm/h264dsp_neon.S
    libavcodec/arm/h264idct_neon.S
    libavcodec/arm/h264pred_neon.S
    libavcodec/arm/h264qpel_neon.S
    libavcodec/arm/hpeldsp_neon.S)
else()
list(APPEND libfilenames
    libavutil/x86/cpu.c
    libavcodec/x86/h264chroma_init.c
    libavcodec/x86/h264dsp_init.c
    libavcodec/x86/h264_intrapred_init.c
    libavcodec/x86/h264_qpel.c)
endif()
if(WIN32)
list(APPEND libfilenames
    compat/strtod.c
    compat/msvcrt/snprintf.c
)
endif()
if(WIN32 OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l"))
    option (BUILD_SHARED_LIBS "enabled static library instead of shared" ON)
else()
    option (BUILD_SHARED_LIBS "enabled static library instead of shared" OFF)
endif()

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
add_library(openH264wrapper ${libfilenames} ${YASM_OBJECTS})
if(CRYPTOPP_FOUND)
target_link_libraries(openH264wrapper cryptopp)
else()
target_link_libraries(openH264wrapper m)
endif()
