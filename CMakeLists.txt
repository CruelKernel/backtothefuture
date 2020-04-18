cmake_minimum_required(VERSION 3.12)

if (NOT DEFINED NDK_ROOT)
    if (DEFINED ENV{NDK_ROOT})
        set(NDK_ROOT "$ENV{NDK_ROOT}")
    else ()
        message(FATAL_ERROR "Please define NDK_ROOT to point to your NDK path!")
    endif ()
endif ()

find_program(ZIP NAMES zip)

# Set the tool chain file
set(CMAKE_TOOLCHAIN_FILE ${NDK_ROOT}/build/cmake/android.toolchain.cmake)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM latest)
set(ANDROID_STL none)
set(ANDROID_LD lld)

add_compile_options(-Wall -Wextra -pedantic -Werror)

set(DEFAULT_FUTURE_DATE "2099-12" CACHE STRING "Select os_patch_level date.")
option(SELF_PACK_EXECUTABLES "Self-pack executables." ON)

set(UPDATE_ZIP "backtothefuture-${DEFAULT_FUTURE_DATE}.zip")

project(BackToTheFuture)

set(CMAKE_C_FLAGS_RELEASE "-O2 -flto")
set(ANDROID_PIE FALSE)
link_libraries("-static")

add_executable(change_os_version change_os_version.c)

if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(
        TARGET change_os_version
        POST_BUILD
        COMMAND "${ANDROID_TOOLCHAIN_PREFIX}strip" --strip-all change_os_version
        COMMENT "Stripping the executables"
        VERBATIM
    )
    if (SELF_PACK_EXECUTABLES)
      include("${CMAKE_ROOT}/Modules/FindSelfPackers.cmake")
      if (SELF_PACKER_FOR_EXECUTABLE)
          add_custom_command(
              TARGET change_os_version
              POST_BUILD
              COMMAND ${SELF_PACKER_FOR_EXECUTABLE} -9q ${SELF_PACKER_FOR_EXECUTABLE_FLAGS} change_os_version
              COMMENT "Packing the executables"
              VERBATIM
          )
      endif ()
    endif ()
endif ()

if (NOT CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/META-INF/com/google/android/update-binary
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/META-INF/com/google/android/update-binary
                                         ${CMAKE_CURRENT_BINARY_DIR}/META-INF/com/google/android/update-binary
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/META-INF/com/google/android/update-binary
    )
endif ()

add_custom_target(zip
    DEPENDS change_os_version
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/META-INF/com/google/android/update-binary
    COMMAND ${ZIP} ${UPDATE_ZIP} change_os_version META-INF/com/google/android/update-binary
    COMMENT "Preparing ${UPDATE_ZIP}"
    VERBATIM
)

