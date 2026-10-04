find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SOURCE_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/third_party/source")
if(NOT EXISTS "${SOURCE_UPSTREAM}/tier0/wscript")
  message(FATAL_ERROR "Initialize Source dependency: git submodule update --init --depth 1")
endif()
set(SOURCE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/source-port")
execute_process(COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare_source.py" "${SOURCE_UPSTREAM}" "${SOURCE_ROOT}"
  RESULT_VARIABLE SOURCE_PREPARE_RESULT)
if(NOT SOURCE_PREPARE_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not prepare pinned Source modules")
endif()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/source-files.json" SOURCE_MANIFEST)
add_library(source_settings INTERFACE)
target_include_directories(source_settings SYSTEM INTERFACE
  "${SOURCE_ROOT}" "${SOURCE_ROOT}/public" "${SOURCE_ROOT}/public/tier0"
  "${SOURCE_ROOT}/public/tier1" "${SOURCE_ROOT}/public/mathlib"
  "${SOURCE_ROOT}/common" "${SOURCE_ROOT}/tier0" "${SOURCE_ROOT}/tier1")
target_compile_definitions(source_settings INTERFACE
  POSIX=1 _POSIX=1 PLATFORM_POSIX=1 GNUC PLATFORM_64BITS=1
  NO_HOOK_MALLOC NO_MEMOVERRIDE_NEW_DELETE _STATIC_LINKED
  TIER0_DLL_EXPORT=1 TIER1_STATIC_LIB=1 VSTDLIB_DLL_EXPORT=1 MATHLIB_LIB=1
  WAF_CFLAGS="CMake-iOS-port" WAF_LDFLAGS="static")
target_compile_options(source_settings INTERFACE -fno-strict-aliasing)
if(APPLE)
  target_compile_definitions(source_settings INTERFACE OSX=1 _OSX=1 _DLL_EXT=.dylib)
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(source_settings INTERFACE SOURCE_IOS=1)
  endif()
else()
  target_compile_definitions(source_settings INTERFACE LINUX=1 _LINUX=1 PLATFORM_GLIBC=1 _DLL_EXT=.so)
endif()
foreach(module IN ITEMS tier0 tier1 mathlib vstdlib)
  string(JSON count LENGTH "${SOURCE_MANIFEST}" "${module}")
  math(EXPR last "${count} - 1")
  set(sources)
  foreach(index RANGE 0 ${last})
    string(JSON relative GET "${SOURCE_MANIFEST}" "${module}" ${index})
    list(APPEND sources "${SOURCE_ROOT}/${relative}")
  endforeach()
  add_library(source_${module} STATIC ${sources})
  target_link_libraries(source_${module} PUBLIC source_settings)
  # Retain upstream code; suppress its legacy warnings without weakening host checks.
  target_compile_options(source_${module} PRIVATE -w)
endforeach()
add_library(source_modules INTERFACE)
if(APPLE)
  target_link_libraries(source_modules INTERFACE source_vstdlib source_tier1 source_mathlib source_tier0 iconv)
else()
  target_link_libraries(source_modules INTERFACE
    "$<LINK_GROUP:RESCAN,source_vstdlib,source_tier1,source_mathlib,source_tier0>" dl pthread)
endif()
