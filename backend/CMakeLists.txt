include_guard(GLOBAL)

function(add_road_sign_module module_name)
    cmake_parse_arguments(MODULE "" "" "SOURCES;PUBLIC_DEPENDENCIES" ${ARGN})
    if(NOT MODULE_SOURCES)
        message(FATAL_ERROR "Module ${module_name} must define its sources")
    endif()

    set(module_root "${CMAKE_CURRENT_SOURCE_DIR}/modules/${module_name}")
    if(NOT IS_DIRECTORY "${module_root}/public")
        message(FATAL_ERROR "Module ${module_name} must have a public interface directory")
    endif()

    add_library("${module_name}" STATIC ${MODULE_SOURCES})
    target_include_directories("${module_name}" PUBLIC "${module_root}/public")
    target_include_directories("${module_name}" PRIVATE "${module_root}")
    foreach(dependency IN LISTS MODULE_PUBLIC_DEPENDENCIES)
        if(NOT TARGET "${dependency}")
            message(FATAL_ERROR "Module ${module_name} depends on unknown module ${dependency}")
        endif()
    endforeach()
    target_link_libraries("${module_name}" PUBLIC ${MODULE_PUBLIC_DEPENDENCIES})
endfunction()

set(ROAD_SIGN_MODULE_DEPENDENCIES
    "" CACHE STRING "Semicolon-separated public target dependencies for module-boundary checks")
if(ROAD_SIGN_MODULE_DEPENDENCIES)
    target_link_libraries(road-sign-backend INTERFACE ${ROAD_SIGN_MODULE_DEPENDENCIES})
endif()
