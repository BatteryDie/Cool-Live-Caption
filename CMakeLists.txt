cmake_minimum_required(VERSION 3.20)

project(CoolLiveCaptions LANGUAGES CXX)

# Some third-party deps expect project version variables; provide defaults.
set(CMAKE_PROJECT_VERSION "0.1" CACHE STRING "" FORCE)
set(PROJECT_VERSION_MAJOR 0 CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ONNXRUNTIME_ROOT "" CACHE PATH "Path to ONNX Runtime prebuilt package (contains include/ and lib/)")
set(APRIL_ASR_ROOT "" CACHE PATH "Path to april-asr prebuilt package (contains include/ and lib/)")
set(APRIL_ASR_LIB_NAME "aprilasr" CACHE STRING "Linker name of the april-asr library (without prefix/suffix)")

set(LIBRARIES_DIR "${CMAKE_BINARY_DIR}/libraries")

include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)

set(ONNX_VERSION "1.23.2")
if(WIN32)
  set(ONNX_ARCHIVE "onnxruntime-win-x64-${ONNX_VERSION}.zip")
  set(ONNX_DIR "${LIBRARIES_DIR}/onnxruntime-win-x64-${ONNX_VERSION}")
  set(ONNX_EXPECTED_LIB "onnxruntime.dll")
elseif(APPLE)
  set(ONNX_ARCHIVE "onnxruntime-osx-universal2-${ONNX_VERSION}.tgz")
  set(ONNX_DIR "${LIBRARIES_DIR}/onnxruntime-osx-universal2-${ONNX_VERSION}")
  set(ONNX_EXPECTED_LIB "libonnxruntime.dylib")
else()
  set(ONNX_ARCHIVE "onnxruntime-linux-x64-${ONNX_VERSION}.tgz")
  set(ONNX_DIR "${LIBRARIES_DIR}/onnxruntime-linux-x64-${ONNX_VERSION}")
  set(ONNX_EXPECTED_LIB "libonnxruntime.so")
endif()

if(NOT ONNXRUNTIME_ROOT OR NOT EXISTS "${ONNXRUNTIME_ROOT}/lib/${ONNX_EXPECTED_LIB}")
  set(ONNX_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/${ONNX_ARCHIVE}")

  FetchContent_Declare(onnxruntime_prebuilt
    URL ${ONNX_URL}
    SOURCE_DIR ${ONNX_DIR}
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
  )
  FetchContent_MakeAvailable(onnxruntime_prebuilt)

  if(EXISTS "${ONNX_DIR}/include" AND EXISTS "${ONNX_DIR}/lib/${ONNX_EXPECTED_LIB}")
    set(ONNXRUNTIME_ROOT "${ONNX_DIR}" CACHE PATH "Path to ONNX Runtime prebuilt package (contains include/ and lib/)" FORCE)
    set(ONNXRuntime_ROOT_DIR "${ONNX_DIR}" CACHE PATH "")
    set(ENV{ONNX_ROOT} "${ONNX_DIR}")
    set(ENV{ONNXRuntime_ROOT_DIR} "${ONNX_DIR}")
  endif()
endif()

if(NOT APRIL_ASR_ROOT)
  set(APRIL_ASR_DIR "${LIBRARIES_DIR}/april-asr-main")
  FetchContent_Declare(april_asr_source
    URL https://github.com/abb128/april-asr/archive/refs/heads/main.zip
    SOURCE_DIR ${APRIL_ASR_DIR}
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
  )
  FetchContent_MakeAvailable(april_asr_source)

  if(WIN32)
    file(COPY ${CMAKE_SOURCE_DIR}/cmake/endian_compat.h DESTINATION ${APRIL_ASR_DIR}/src/file)
    file(RENAME ${APRIL_ASR_DIR}/src/file/endian_compat.h ${APRIL_ASR_DIR}/src/file/endian.h)
    file(COPY ${CMAKE_SOURCE_DIR}/cmake/endian_compat.h DESTINATION ${APRIL_ASR_DIR})
    file(RENAME ${APRIL_ASR_DIR}/endian_compat.h ${APRIL_ASR_DIR}/endian.h)
  endif()

  if(EXISTS "${APRIL_ASR_DIR}/include" AND EXISTS "${APRIL_ASR_DIR}/lib")
    set(APRIL_ASR_ROOT "${APRIL_ASR_DIR}" CACHE PATH "Path to april-asr prebuilt package (contains include/ and lib/)" FORCE)
  endif()
endif()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

set(GLAD_PROFILE "core" CACHE STRING "")
set(GLAD_API "gl=3.0" CACHE STRING "")
set(GLAD_GENERATOR "c" CACHE STRING "")
set(GLAD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.8
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_obj
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui::imgui ALIAS imgui_obj)

target_include_directories(imgui_obj PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${glfw_SOURCE_DIR}/include
  ${glad_SOURCE_DIR}/include
)

target_compile_definitions(imgui_obj PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui_obj PUBLIC glfw glad)

set(SOURCES
  src/main.cpp
  src/app_update.cpp
  src/caption.cpp
  src/april_asr.cpp
  src/transcription.cpp
  src/model.cpp
  src/profanity.cpp
  include/caption.h
  include/april_asr.h
  include/transcription.h
  include/model.h
  include/profanity.h
  include/app_update.h
)

set(PLATFORM_SOURCES)
if(WIN32)
  list(APPEND PLATFORM_SOURCES src/audio_win.cpp include/audio_win.h)
elseif(APPLE)
  list(APPEND PLATFORM_SOURCES src/audio_mac.cpp include/audio_mac.h)
else()
  list(APPEND PLATFORM_SOURCES src/audio_linux.cpp include/audio_linux.h)
endif()

if(WIN32)
  add_executable(coollivecaptions WIN32
    ${SOURCES}
    ${PLATFORM_SOURCES}
  )
else()
  add_executable(coollivecaptions
    ${SOURCES}
    ${PLATFORM_SOURCES}
  )
endif()

set(APP_ICON resources/icon.ico)
set(APP_RC resources/app.rc)
set(APP_VERSION_RC ${CMAKE_BINARY_DIR}/app_version.rc)
set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 1)
set(APP_VERSION_PATCH 0)
set(APP_VERSION_BUILD 0)
set(APP_VERSION_STRING "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}.${APP_VERSION_BUILD}")
set(APP_COMPANY "Cool Live Captions")
set(APP_COPYRIGHT "Copyright (c) 2026 Luca Jones")

configure_file(${CMAKE_SOURCE_DIR}/resources/app_version.rc.in ${APP_VERSION_RC} @ONLY)

if(WIN32)
  target_sources(coollivecaptions PRIVATE ${APP_VERSION_RC})
  set_source_files_properties(${APP_VERSION_RC} PROPERTIES LANGUAGE RC)
  if(EXISTS "${CMAKE_SOURCE_DIR}/${APP_ICON}")
    target_sources(coollivecaptions PRIVATE ${APP_RC})
    set_source_files_properties(${APP_RC} PROPERTIES LANGUAGE RC)
  endif()
endif()

target_include_directories(coollivecaptions PRIVATE src include ${glad_SOURCE_DIR}/include ${glfw_SOURCE_DIR}/include)
target_compile_definitions(coollivecaptions PRIVATE
  APP_VERSION_STRING="${APP_VERSION_STRING}"
  APP_VERSION_MAJOR=${APP_VERSION_MAJOR}
  APP_VERSION_MINOR=${APP_VERSION_MINOR}
  APP_VERSION_PATCH=${APP_VERSION_PATCH}
  APP_VERSION_BUILD=${APP_VERSION_BUILD}
  APP_VERSION_TAG="v${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}"
  GITHUB_REPO="BatteryDie/Cool-Live-Captions"
)

target_link_libraries(coollivecaptions PRIVATE imgui::imgui glfw glad)
if(PIPEWIRE_FOUND)
  target_link_libraries(coollivecaptions PRIVATE PkgConfig::PIPEWIRE)
  target_compile_definitions(coollivecaptions PRIVATE HAVE_PIPEWIRE)
else()
  message(WARNING "PipeWire development package not found; Linux audio capture will be disabled.")
endif()
target_compile_definitions(coollivecaptions PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

if(ONNXRUNTIME_ROOT)
  target_include_directories(coollivecaptions PRIVATE ${ONNXRUNTIME_ROOT}/include)
  target_link_directories(coollivecaptions PRIVATE ${ONNXRUNTIME_ROOT}/lib)
  target_link_libraries(coollivecaptions PRIVATE onnxruntime)
endif()

if(TARGET aprilasr)
  target_link_libraries(coollivecaptions PRIVATE aprilasr)
  target_include_directories(coollivecaptions PRIVATE ${april_asr_source_SOURCE_DIR}/include)
  add_custom_command(TARGET coollivecaptions POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:aprilasr> $<TARGET_FILE_DIR:coollivecaptions>/)
elseif(APRIL_ASR_ROOT)
  target_include_directories(coollivecaptions PRIVATE ${APRIL_ASR_ROOT} ${APRIL_ASR_ROOT}/include)
  target_link_directories(coollivecaptions PRIVATE ${APRIL_ASR_ROOT}/lib)
  target_link_libraries(coollivecaptions PRIVATE ${APRIL_ASR_LIB_NAME})
  if(WIN32)
    set(APRIL_ASR_RUNTIME ${APRIL_ASR_ROOT}/lib/${APRIL_ASR_LIB_NAME}.dll)
  elseif(APPLE)
    set(APRIL_ASR_RUNTIME ${APRIL_ASR_ROOT}/lib/lib${APRIL_ASR_LIB_NAME}.dylib)
  else()
    set(APRIL_ASR_RUNTIME ${APRIL_ASR_ROOT}/lib/lib${APRIL_ASR_LIB_NAME}.so)
  endif()
  if(EXISTS ${APRIL_ASR_RUNTIME})
    add_custom_command(TARGET coollivecaptions POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${APRIL_ASR_RUNTIME}
        $<TARGET_FILE_DIR:coollivecaptions>/
      VERBATIM)
  endif()
endif()

if(WIN32)
  target_compile_definitions(coollivecaptions PRIVATE APRIL_DLL_IMPORT)
  target_link_libraries(coollivecaptions PRIVATE opengl32 ole32 uuid mmdevapi avrt ksuser shell32)
  if(ONNXRUNTIME_ROOT)
    add_custom_command(TARGET coollivecaptions POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll
        ${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_shared.dll
        $<TARGET_FILE_DIR:coollivecaptions>/
      VERBATIM)
  endif()

  # Bundle MinGW runtime DLLs for redistribution.
  get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
  set(MINGW_RUNTIME_DLLS
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    libsrdc.dll
  )
  foreach(dll ${MINGW_RUNTIME_DLLS})
    if(EXISTS "${MINGW_BIN_DIR}/${dll}")
      add_custom_command(TARGET coollivecaptions POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${MINGW_BIN_DIR}/${dll}"
          $<TARGET_FILE_DIR:coollivecaptions>/
        VERBATIM)
    endif()
  endforeach()
elseif(UNIX AND NOT APPLE)
  if(ONNXRUNTIME_ROOT)
    add_custom_command(TARGET coollivecaptions POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so
        ${ONNXRUNTIME_ROOT}/lib/libonnxruntime_providers_shared.so
        $<TARGET_FILE_DIR:coollivecaptions>/
      VERBATIM)
  endif()
endif()

set_target_properties(coollivecaptions PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_custom_command(TARGET coollivecaptions POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:coollivecaptions>/profanity
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/profanity
    $<TARGET_FILE_DIR:coollivecaptions>/profanity
  VERBATIM)
