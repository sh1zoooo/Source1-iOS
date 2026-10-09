# Original graphical modules are compiled separately until their platform
# services and runtime interfaces are connected. Do not advertise them as active.
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/clientmod_manifest.py"
  RESULT_VARIABLE CLIENTMOD_MANIFEST_RESULT)
if(NOT CLIENTMOD_MANIFEST_RESULT EQUAL 0)
  message(FATAL_ERROR "ClientMod source selection changed")
endif()
set(CLIENTMOD_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/third_party/clientmod")
set(CLIENTMOD_ROOT "${CMAKE_CURRENT_BINARY_DIR}/clientmod-port")
execute_process(COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare_clientmod.py"
  "${CLIENTMOD_UPSTREAM}" "${CLIENTMOD_ROOT}" "${SOURCE_UPSTREAM}"
  RESULT_VARIABLE CLIENTMOD_PREPARE_RESULT)
if(NOT CLIENTMOD_PREPARE_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not prepare pinned ClientMod modules")
endif()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clientmod-files.json" CLIENTMOD_MANIFEST)
foreach(module IN ITEMS vgui_controls matsys_controls vgui2 GameUI client vgui_surfacelib vguimatsurface stdshader_dx9)
  set(sources)
  string(JSON count LENGTH "${CLIENTMOD_MANIFEST}" modules ${module} source)
  math(EXPR last "${count}-1")
  foreach(index RANGE 0 ${last})
    string(JSON path GET "${CLIENTMOD_MANIFEST}" modules ${module} source ${index})
    if(NOT path STREQUAL "public/tier0/memoverride.cpp")
      list(APPEND sources "${CLIENTMOD_ROOT}/${path}")
    endif()
  endforeach()
  if(module STREQUAL "GameUI")
    list(APPEND sources "${CLIENTMOD_ROOT}/gameui/ModMenu/ClientModMainMenu.cpp"
      "${CLIENTMOD_ROOT}/gameui/ModMenu/ClientModMenuWindow.cpp")
  endif()
  add_library(clientmod_${module} STATIC ${sources})
  set_target_properties(clientmod_${module} PROPERTIES CXX_STANDARD 14)
  if(SOURCE_BUILD_CLIENTMOD_RUNTIME)
    set_target_properties(clientmod_${module} PROPERTIES POSITION_INDEPENDENT_CODE ON
      CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    target_compile_options(clientmod_${module} PRIVATE -U_STATIC_LINKED)
  endif()
  target_link_libraries(clientmod_${module} PRIVATE source_settings)
  target_compile_options(clientmod_${module} PRIVATE -w)
  string(JSON count LENGTH "${CLIENTMOD_MANIFEST}" modules ${module} includes)
  math(EXPR last "${count}-1")
  foreach(index RANGE 0 ${last})
    string(JSON path GET "${CLIENTMOD_MANIFEST}" modules ${module} includes ${index})
    # Platform and public interfaces use the already patched SDK headers.
    if(NOT path MATCHES "^public($|/)")
      target_include_directories(clientmod_${module} PRIVATE "${CLIENTMOD_ROOT}/${path}")
    endif()
  endforeach()
  target_include_directories(clientmod_${module} PRIVATE
    "${CLIENTMOD_ROOT}/compat"
    "${SOURCE_UPSTREAM}/thirdparty/SDL-src/include" "${CLIENTMOD_ROOT}/game/shared"
    "${CLIENTMOD_ROOT}/common")
  string(JSON count LENGTH "${CLIENTMOD_MANIFEST}" modules ${module} defines)
  if(count GREATER 0)
    math(EXPR last "${count}-1")
    foreach(index RANGE 0 ${last})
      string(JSON define GET "${CLIENTMOD_MANIFEST}" modules ${module} defines ${index})
      target_compile_definitions(clientmod_${module} PRIVATE "${define}")
    endforeach()
  endif()
  target_compile_definitions(clientmod_${module} PRIVATE USE_SDL=1 NO_STEAM=1 DISABLE_STEAM=1 DONT_PROTECT_FILEIO_FUNCTIONS=1)
endforeach()
target_compile_definitions(clientmod_GameUI PRIVATE GAMEUI_EXPORTS=1 VERSION_SAFE_STEAM_API_INTERFACES=1)
target_compile_definitions(clientmod_vgui2 PRIVATE VGUI_EXPORTS=1)
target_compile_definitions(clientmod_vguimatsurface PRIVATE VGUIMATSURFACE_DLL_EXPORT=1 GAMEUI_EXPORTS=1)
target_compile_definitions(clientmod_stdshader_dx9 PRIVATE STDSHADER_DX9_DLL_EXPORT=1 FAST_MATERIALVAR_ACCESS=1)
foreach(module IN ITEMS vgui_surfacelib vguimatsurface)
  target_include_directories(clientmod_${module} PRIVATE "${SOURCE_UPSTREAM}/thirdparty/freetype/include")
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  enable_language(OBJCXX)
  target_sources(clientmod_vgui2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/ios/ClientModPlatform.mm")
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/ios/ClientModPlatform.mm"
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(clientmod_vgui2 PRIVATE "-framework UIKit" "-framework Foundation"
    "-framework UniformTypeIdentifiers")
endif()
add_custom_target(clientmod_compile_check DEPENDS clientmod_client clientmod_GameUI
  clientmod_vgui2 clientmod_vgui_controls clientmod_matsys_controls
  clientmod_vgui_surfacelib clientmod_vguimatsurface clientmod_stdshader_dx9)
