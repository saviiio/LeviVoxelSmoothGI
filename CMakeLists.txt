cmake_minimum_required(VERSION 3.22)
project(LeviVoxelSmoothGI VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(MOD_VERSION "1.0.0-beta.8.6" CACHE STRING "Version")
set(MOD_ID "levi_voxel_smooth_gi" CACHE STRING "LeviLauncher package directory")

set(CORE src/Sha256.cpp src/BinaryCache.cpp src/ShaderPatcher.cpp)
if(ANDROID)
  include(FetchContent)
  if(NOT PRELOADER_ANDROID_ROOT)
    FetchContent_Declare(preloader_android GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git GIT_TAG 0.2.2 GIT_SHALLOW TRUE)
    FetchContent_GetProperties(preloader_android)
    if(NOT preloader_android_POPULATED)
      FetchContent_Populate(preloader_android)
    endif()
    set(PRELOADER_ANDROID_ROOT "${preloader_android_SOURCE_DIR}")
  endif()
  if(NOT EXISTS "${PRELOADER_ANDROID_ROOT}/include/pl/Mod.hpp")
    message(FATAL_ERROR "PRELOADER_ANDROID_ROOT does not contain include/pl/Mod.hpp")
  endif()

  # preloader-android owns its dependency graph (including fmt 11.2.0).
  # Do not create fmt here: preloader's CMakeLists uses FetchContent_MakeAvailable(fmt).
  # A cached/local fmt checkout can be supplied with FETCHCONTENT_SOURCE_DIR_FMT,
  # which makes FetchContent reuse it without downloading or defining fmt twice.

  # HookHandle is not header-only. Build and link the official preloader target so
  # pl::memory::hook/HookHandle resolve through libpreloader.so at runtime.
  # This matches the current LeviLauncher Android native-mod template.
  if(NOT TARGET preloader)
    add_subdirectory("${PRELOADER_ANDROID_ROOT}" "${CMAKE_BINARY_DIR}/_preloader_android" EXCLUDE_FROM_ALL)
  endif()
  if(NOT TARGET preloader)
    message(FATAL_ERROR "preloader target was not created by preloader-android")
  endif()

  add_library(levi_voxel_smooth_gi SHARED ${CORE} src/VoxelRuntime.cpp src/ItemPrewarmer.cpp src/GlHooks.cpp src/Mod.cpp)
  # preloader-android 0.2.2 uses C++20 concepts/requires in pl/Mod.hpp.
  # Keep the portable host core at C++17, but compile the Android mod target as C++20.
  target_compile_features(levi_voxel_smooth_gi PRIVATE cxx_std_20)
  # pl/Config.hpp is a public preloader header, but preloader 0.2.2 links its
  # header-only Config dependencies privately. Since this mod includes Config.hpp
  # directly, link those interface targets explicitly so their include paths are
  # propagated to this target (Boost.PFR, magic_enum and nlohmann/json).
  foreach(_cfg_dep Boost::pfr magic_enum::magic_enum nlohmann_json::nlohmann_json)
    if(NOT TARGET ${_cfg_dep})
      message(FATAL_ERROR "preloader Config dependency target is missing: ${_cfg_dep}")
    endif()
  endforeach()
  target_include_directories(levi_voxel_smooth_gi PRIVATE include generated "${PRELOADER_ANDROID_ROOT}/include")
  target_compile_options(levi_voxel_smooth_gi PRIVATE -Wall -Wextra -Wpedantic -Werror -fvisibility=hidden -fvisibility-inlines-hidden)
  find_library(LOG_LIB log REQUIRED)
  find_library(GLES_LIB GLESv3 REQUIRED)
  find_library(EGL_LIB EGL REQUIRED)
  find_library(DL_LIB dl REQUIRED)
  find_package(Threads REQUIRED)
  target_link_libraries(levi_voxel_smooth_gi PRIVATE
    preloader
    Boost::pfr
    magic_enum::magic_enum
    nlohmann_json::nlohmann_json
    ${LOG_LIB} ${GLES_LIB} ${EGL_LIB} ${DL_LIB} Threads::Threads)
  target_link_options(levi_voxel_smooth_gi PRIVATE -Wl,-z,max-page-size=16384 -Wl,--no-undefined)
  set_target_properties(levi_voxel_smooth_gi PROPERTIES OUTPUT_NAME levi_voxel_smooth_gi)
  configure_file(manifest.json.in "${CMAKE_BINARY_DIR}/manifest.json" @ONLY)
  set(PKG "${CMAKE_BINARY_DIR}/package-root")
  set(MODDIR "${PKG}/${MOD_ID}")
  set(OUT "${CMAKE_BINARY_DIR}/LeviVoxelSmoothGI-${MOD_VERSION}-arm64.levipack")
  add_custom_target(levi_package
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PKG}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MODDIR}/config"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MODDIR}/resources"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:levi_voxel_smooth_gi>" "${MODDIR}/liblevi_voxel_smooth_gi.so"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/manifest.json" "${MODDIR}/manifest.json"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/config/config.json" "${MODDIR}/config/config.json"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/config/config.schema.json" "${MODDIR}/config/config.schema.json"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/prewarm/current_item_programs.lvpr" "${MODDIR}/resources/current_item_programs.lvpr"
    COMMAND ${CMAKE_COMMAND} -E chdir "${PKG}" ${CMAKE_COMMAND} -E tar cf "${OUT}" --format=zip "${MOD_ID}"
    DEPENDS levi_voxel_smooth_gi VERBATIM)
else()
  add_library(lvsgi_core STATIC ${CORE})
  target_include_directories(lvsgi_core PUBLIC include generated)
  target_compile_options(lvsgi_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_executable(lvsgi_tests tests/test_core.cpp)
  target_link_libraries(lvsgi_tests PRIVATE lvsgi_core)
  enable_testing()
  add_test(NAME core COMMAND lvsgi_tests)
endif()
