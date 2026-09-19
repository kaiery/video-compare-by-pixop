cmake_minimum_required(VERSION 3.24)
project(VideoCompareGUI VERSION 0.1.1 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Build this standalone launcher with MSVC for Windows x64.")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "Use tools/build.ps1 or an out-of-source build directory.")
endif()

add_library(launcher-core STATIC
  src/core/catalog.cpp
  src/core/model.cpp
  src/core/command_line.cpp
  src/core/build_plan.cpp
  src/core/storage.cpp
)
target_include_directories(launcher-core PUBLIC src)
target_compile_features(launcher-core PUBLIC cxx_std_17)
target_compile_options(launcher-core PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(launcher-core PRIVATE NOMINMAX)
target_link_libraries(launcher-core PUBLIC ole32)
set_target_properties(launcher-core PROPERTIES CXX_EXTENSIONS OFF
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(launcher-settings STATIC src/ui/settings.cpp)
target_compile_features(launcher-settings PUBLIC cxx_std_17)
target_compile_options(launcher-settings PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(launcher-settings PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_include_directories(launcher-settings PRIVATE resources)
target_link_libraries(launcher-settings PUBLIC launcher-core comctl32 user32 gdi32 ole32 shell32 uuid)
set_target_properties(launcher-settings PROPERTIES CXX_EXTENSIONS OFF
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(launcher-process STATIC src/process/runtime.cpp)
target_compile_features(launcher-process PUBLIC cxx_std_17)
target_compile_options(launcher-process PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(launcher-process PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(launcher-process PUBLIC launcher-core user32 ole32)
target_link_libraries(launcher-settings PUBLIC launcher-process)
set_target_properties(launcher-process PROPERTIES CXX_EXTENSIONS OFF MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_executable(video-compare-runner src/process/runner.cpp)
target_compile_options(video-compare-runner PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(video-compare-runner PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(video-compare-runner PRIVATE launcher-process)
set_target_properties(video-compare-runner PROPERTIES CXX_EXTENSIONS OFF
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
install(TARGETS video-compare-runner RUNTIME DESTINATION .)

add_executable(video-compare-gui WIN32
  src/main.cpp
  resources/app.rc
  resources/resource.h
  resources/app.manifest
)
target_compile_features(video-compare-gui PRIVATE cxx_std_17)
target_compile_definitions(video-compare-gui PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
target_compile_options(video-compare-gui PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_include_directories(video-compare-gui PRIVATE resources)
target_link_libraries(video-compare-gui PRIVATE launcher-settings launcher-process)
add_dependencies(video-compare-gui video-compare-runner)
set_target_properties(video-compare-gui PROPERTIES
  CXX_EXTENSIONS OFF
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
# CMake merges and embeds the manifest for both incremental and release links.
install(TARGETS video-compare-gui RUNTIME DESTINATION .)

include(CTest)
if(BUILD_TESTING)
  add_executable(launcher-engine-acceptance tests/engine_acceptance.cpp resources/app.rc resources/app.manifest)
  target_compile_options(launcher-engine-acceptance PRIVATE /W4 /permissive- /utf-8 /EHsc)
  target_compile_definitions(launcher-engine-acceptance PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  target_include_directories(launcher-engine-acceptance PRIVATE resources)
  target_link_libraries(launcher-engine-acceptance PRIVATE launcher-settings launcher-process)
  add_dependencies(launcher-engine-acceptance video-compare-runner)
  set_target_properties(launcher-engine-acceptance PROPERTIES CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
  add_executable(launcher-storage-tests tests/storage_tests.cpp)
  target_link_libraries(launcher-storage-tests PRIVATE launcher-core)
  target_compile_options(launcher-storage-tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
  set_target_properties(launcher-storage-tests PROPERTIES CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
  add_test(NAME config.storage COMMAND launcher-storage-tests "${CMAKE_SOURCE_DIR}/work/p06/native/$<CONFIG>")
  add_executable(launcher-engine-fixture tests/engine_fixture.cpp)
  add_library(launcher-fixture-dependency SHARED tests/fixture_dependency.cpp)
  add_executable(launcher-missing-dll-fixture tests/missing_dll_fixture.cpp)
  target_link_libraries(launcher-missing-dll-fixture PRIVATE launcher-fixture-dependency)
  add_executable(launcher-process-tests tests/process_tests.cpp)
  target_link_libraries(launcher-process-tests PRIVATE launcher-process)
  add_dependencies(launcher-process-tests video-compare-runner launcher-engine-fixture launcher-missing-dll-fixture)
  foreach(target launcher-engine-fixture launcher-fixture-dependency launcher-missing-dll-fixture launcher-process-tests)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
  endforeach()
  add_test(NAME process.lifecycle COMMAND launcher-process-tests "${CMAKE_SOURCE_DIR}/work/p05/native/$<CONFIG>")
  set_tests_properties(process.lifecycle PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
  add_executable(launcher-core-tests tests/core_tests.cpp)
  target_compile_options(launcher-core-tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
  target_compile_definitions(launcher-core-tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(launcher-core-tests PRIVATE launcher-core shell32)
  set_target_properties(launcher-core-tests PROPERTIES CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
  foreach(group catalog overrides semantics quoting)
    add_test(NAME "core.${group}" COMMAND launcher-core-tests "${group}")
    set_tests_properties("core.${group}" PROPERTIES TIMEOUT 30)
  endforeach()
  add_executable(launcher-ui-tests tests/ui_events.cpp resources/app.rc resources/app.manifest)
  target_compile_features(launcher-ui-tests PRIVATE cxx_std_17)
  target_compile_options(launcher-ui-tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
  target_compile_definitions(launcher-ui-tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  target_include_directories(launcher-ui-tests PRIVATE resources)
  target_link_libraries(launcher-ui-tests PRIVATE launcher-settings launcher-process)
  set_target_properties(launcher-ui-tests PROPERTIES CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
  add_test(NAME ui.native-events COMMAND launcher-ui-tests "${CMAKE_SOURCE_DIR}/work/p03/native")
  set_tests_properties(ui.native-events PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
  add_executable(launcher-settings-tests tests/settings_events.cpp resources/app.rc resources/app.manifest)
  target_compile_features(launcher-settings-tests PRIVATE cxx_std_17)
  target_compile_options(launcher-settings-tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
  target_compile_definitions(launcher-settings-tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  target_include_directories(launcher-settings-tests PRIVATE resources)
  target_link_libraries(launcher-settings-tests PRIVATE launcher-settings)
  set_target_properties(launcher-settings-tests PROPERTIES CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
  foreach(group catalog-a catalog-b overrides semantics)
    add_test(NAME "ui.settings-${group}" COMMAND launcher-settings-tests "${group}")
    set_tests_properties("ui.settings-${group}" PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
  endforeach()
endif()
