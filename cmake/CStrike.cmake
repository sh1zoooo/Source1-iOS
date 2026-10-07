# Separate compilation milestone. Do not expose an uninitialized game DLL to Host.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cstrike-files.json" CSTRIKE_MANIFEST)
string(JSON count LENGTH "${CSTRIKE_MANIFEST}" sources)
math(EXPR last "${count} - 1")
set(cstrike_sources)
foreach(index RANGE 0 ${last})
  string(JSON relative GET "${CSTRIKE_MANIFEST}" sources ${index})
  list(APPEND cstrike_sources "${SOURCE_ROOT}/${relative}")
endforeach()
if(SOURCE_LINK_CSTRIKE)
  # Compile common SDK implementations once in the existing engine libraries.
  # Studio hooks use the engine's actual model cache instead of a second DLL copy.
  foreach(module IN ITEMS engine tier0 tier1 tier2 tier3 mathlib vstdlib)
    string(JSON count LENGTH "${SOURCE_MANIFEST}" ${module})
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
      string(JSON relative GET "${SOURCE_MANIFEST}" ${module} ${index})
      list(REMOVE_ITEM cstrike_sources "${SOURCE_ROOT}/${relative}")
    endforeach()
  endforeach()
  list(REMOVE_ITEM cstrike_sources "${SOURCE_ROOT}/game/shared/studio_shared.cpp")
endif()
add_library(source_cstrike_server STATIC ${cstrike_sources})
set_target_properties(source_cstrike_server PROPERTIES CXX_STANDARD 14)
target_link_libraries(source_cstrike_server PRIVATE source_settings)
target_include_directories(source_cstrike_server PRIVATE "${SOURCE_ROOT}/game/shared")
foreach(field IN ITEMS includes defines)
  string(JSON count LENGTH "${CSTRIKE_MANIFEST}" ${field})
  math(EXPR last "${count} - 1")
  foreach(index RANGE 0 ${last})
    string(JSON value GET "${CSTRIKE_MANIFEST}" ${field} ${index})
    if(field STREQUAL "includes")
      target_include_directories(source_cstrike_server PRIVATE "${SOURCE_ROOT}/${value}")
    else()
      target_compile_definitions(source_cstrike_server PRIVATE "${value}")
    endif()
  endforeach()
endforeach()
target_compile_options(source_cstrike_server PRIVATE -w)
if(SOURCE_LINK_CSTRIKE)
  # Desktop DLL-local variables must retain their individual types and lifetimes.
  foreach(symbol IN ITEMS developer mat_hdr_tonemapscale modelinfo physcollision
      physprops skill sv_alternateticks sv_cheats sv_maxreplay sv_noclipduringpause
      COM_GetModDirectory)
    target_compile_definitions(source_cstrike_server PRIVATE "${symbol}=PortCS_${symbol}")
  endforeach()
  target_compile_definitions(source_cstrike_server PRIVATE
    SOURCE_GAME_LINK=1 CPhysicsSpring=PortCS_CPhysicsSpring)
  target_compile_definitions(source_engine PRIVATE SOURCE_GAME_LINK=1)
endif()

# Original support modules required by GameDLL::DLLInit and the game code.
# Compile-only mode retains archives; linked mode resolves these into the app.
set(cstrike_particles
  particles/builtin_constraints.cpp particles/builtin_initializers.cpp
  particles/builtin_particle_emitters.cpp particles/builtin_particle_forces.cpp
  particles/addbuiltin_ops.cpp particles/builtin_particle_ops.cpp
  particles/builtin_particle_render_ops.cpp particles/particle_sort.cpp
  particles/particles.cpp particles/psheet.cpp)
set(cstrike_dmxloader
  dmxloader/dmxattribute.cpp dmxloader/dmxelement.cpp dmxloader/dmxloader.cpp
  dmxloader/dmxloadertext.cpp dmxloader/dmxserializationdictionary.cpp)
set(cstrike_choreoobjects
  game/shared/choreoactor.cpp game/shared/choreochannel.cpp
  game/shared/choreoevent.cpp game/shared/choreoscene.cpp game/shared/sceneimage.cpp)
set(cstrike_soundemittersystem
  game/shared/interval.cpp soundemittersystem/soundemittersystembase.cpp
  public/SoundParametersInternal.cpp)
if(SOURCE_LINK_CSTRIKE)
  list(REMOVE_ITEM cstrike_soundemittersystem game/shared/interval.cpp)
endif()
set(cstrike_scenefilecache scenefilecache/SceneFileCache.cpp)
foreach(module IN ITEMS particles dmxloader choreoobjects soundemittersystem scenefilecache)
  set(sources)
  foreach(relative IN LISTS cstrike_${module})
    list(APPEND sources "${SOURCE_ROOT}/${relative}")
  endforeach()
  add_library(source_${module} STATIC ${sources})
  set_target_properties(source_${module} PROPERTIES CXX_STANDARD 14)
  target_link_libraries(source_${module} PRIVATE source_settings)
  target_include_directories(source_${module} PRIVATE
    "${SOURCE_ROOT}/${module}" "${SOURCE_ROOT}/game/shared" "${SOURCE_ROOT}/utils/common")
  target_compile_options(source_${module} PRIVATE -w)
endforeach()
target_compile_definitions(source_dmxloader PRIVATE DMXLOADER_LIB=1)
target_compile_definitions(source_soundemittersystem PRIVATE SOUNDEMITTERSYSTEM_EXPORTS=1 SOUNDEMITTERSYSTEM_DLL=1)
if(SOURCE_LINK_CSTRIKE)
  target_compile_definitions(source_scenefilecache PRIVATE IsBufferBinaryVCD=PortScene_IsBufferBinaryVCD)
endif()
add_custom_target(cstrike_compile_check DEPENDS source_cstrike_server
  source_particles source_dmxloader source_choreoobjects source_soundemittersystem source_scenefilecache)
if(SOURCE_LINK_CSTRIKE)
  if(APPLE)
    foreach(module IN ITEMS cstrike_server soundemittersystem scenefilecache)
      target_link_options(source_modules INTERFACE "LINKER:-force_load,$<TARGET_FILE:source_${module}>")
    endforeach()
    target_link_libraries(source_modules INTERFACE source_cstrike_server
      source_soundemittersystem source_scenefilecache source_particles source_dmxloader source_choreoobjects)
  else()
    # The game references tier helpers not previously pulled by the engine.
    # Resolve the entire monolithic graph together, including those new users.
    get_target_property(linkage source_modules INTERFACE_LINK_LIBRARIES)
    string(REPLACE ",source_tier0>"
      ",source_tier0,$<LINK_LIBRARY:WHOLE_ARCHIVE,source_cstrike_server,source_soundemittersystem,source_scenefilecache>,source_particles,source_dmxloader,source_choreoobjects>"
      linkage "${linkage}")
    set_target_properties(source_modules PROPERTIES INTERFACE_LINK_LIBRARIES "${linkage}")
  endif()
endif()
