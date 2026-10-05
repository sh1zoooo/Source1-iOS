cmake_minimum_required(VERSION 3.24)
project(Source1IOS LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)

include(cmake/SourceModules.cmake)
add_library(port_runtime STATIC src/runtime/Runtime.cpp src/source/SourceBridge.cpp src/source/SourceFiles.cpp src/source/SourceAppSystems.cpp src/source/SourceEngine.cpp src/source/SourceAssets.cpp src/source/SourceHost.cpp src/source/SourceMap.cpp src/source/SourceStudio.cpp)
target_include_directories(port_runtime PUBLIC src/runtime)
target_include_directories(port_runtime PUBLIC src/source)
target_include_directories(port_runtime PRIVATE "${SOURCE_ROOT}/engine")
target_link_libraries(port_runtime PRIVATE source_modules)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  enable_language(OBJCXX)
  add_executable(Source1IOS MACOSX_BUNDLE src/ios/main.mm)
  set(SOURCE_NOTICES third_party/SOURCE1_LICENSE.txt third_party/SOURCE1_THIRD_PARTY_NOTICES.txt)
  set_source_files_properties(${SOURCE_NOTICES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  target_sources(Source1IOS PRIVATE ${SOURCE_NOTICES})
  target_link_libraries(Source1IOS PRIVATE port_runtime
    "-framework UIKit" "-framework Foundation" "-framework CoreGraphics"
    "-framework QuartzCore" "-framework Metal" "-framework MetalKit")
  target_compile_options(Source1IOS PRIVATE -fobjc-arc)
  set_target_properties(Source1IOS PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/ios/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "io.github.sh1zoooo.source1ios"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE "NO")
else()
  include(CTest)
  if(BUILD_TESTING)
    add_executable(runtime_tests tests/runtime_tests.cpp)
    target_link_libraries(runtime_tests PRIVATE port_runtime)
    add_test(NAME runtime_contract COMMAND runtime_tests)
  endif()
endif()
