# Separate compilation milestone. Do not expose an uninitialized game DLL to Host.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cstrike-files.json" CSTRIKE_MANIFEST)
string(JSON count LENGTH "${CSTRIKE_MANIFEST}" sources)
math(EXPR last "${count} - 1")
set(cstrike_sources)
foreach(index RANGE 0 ${last})
  string(JSON relative GET "${CSTRIKE_MANIFEST}" sources ${index})
  list(APPEND cstrike_sources "${SOURCE_ROOT}/${relative}")
endforeach()
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

# Original support modules required by GameDLL::DLLInit and the game code.
# They are compiled independently; resolving them into the app comes next.
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
target_compile_definitions(source_soundemittersystem PRIVATE SOUNDEMITTERSYSTEM_EXPORTS=1)
add_custom_target(cstrike_compile_check DEPENDS source_cstrike_server
  source_particles source_dmxloader source_choreoobjects source_soundemittersystem source_scenefilecache)
