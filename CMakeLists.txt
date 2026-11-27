cmake_minimum_required(VERSION 3.20)

project(allfetch
    VERSION 1.0.4
    DESCRIPTION "CLI fetch utility with some easter eggs and other fun functionalities"
    LANGUAGES CXX
)

add_executable(allfetch
    src/main.cpp
    src/system/system_info.cpp
    src/core/argparser.cpp
    src/core/command_dispatch.cpp
    src/render/json_render.cpp
    src/render/pretty_render.cpp
    src/render/logo_selector.cpp
)

find_package(nlohmann_json 3.9 CONFIG REQUIRED)
target_compile_definitions(allfetch PRIVATE ALLFETCH_VERSION="${PROJECT_VERSION}")

set(logo_root "${CMAKE_CURRENT_SOURCE_DIR}/assets/logos/fastfetch")
file(GLOB logo_files CONFIGURE_DEPENDS "${logo_root}/[a-z_]/*.txt")
list(APPEND logo_files "${logo_root}/logo_index.json" "${logo_root}/LICENSE" "${logo_root}/SOURCE.txt")
set(logo_stamp "${CMAKE_CURRENT_BINARY_DIR}/logos-$<CONFIG>.stamp")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/logo-files.cmake"
    CONTENT "set(logo_files [==[${logo_files}]==])\n")
add_custom_command(OUTPUT "${logo_stamp}"
    COMMAND "${CMAKE_COMMAND}"
        "-Dmanifest=${CMAKE_CURRENT_BINARY_DIR}/logo-files.cmake"
        "-Dsource=${logo_root}" "-Ddestination=$<TARGET_FILE_DIR:allfetch>/logos"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/stage_logos.cmake"
    COMMAND "${CMAKE_COMMAND}" -E touch "${logo_stamp}"
    DEPENDS ${logo_files} "${CMAKE_CURRENT_BINARY_DIR}/logo-files.cmake" tools/stage_logos.cmake
    VERBATIM
)
add_custom_target(allfetch_logos DEPENDS "${logo_stamp}")
add_dependencies(allfetch allfetch_logos)

target_link_libraries(allfetch PRIVATE
    nlohmann_json::nlohmann_json
)

target_include_directories(allfetch PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(allfetch PRIVATE
        -static
    )
endif()

if (WIN32)
    target_link_libraries(allfetch PRIVATE
        advapi32
        dxgi
        dxguid
        dxcore
        iphlpapi
        shcore
        setupapi
        userenv
        netapi32
        shlwapi
        uxtheme
        version
    )

    target_compile_definitions(allfetch PRIVATE
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")   
    option(ALLFETCH_ENABLE_VULKAN "Enable Vulkan GPU detection" ON)

    if(ALLFETCH_ENABLE_VULKAN)
        find_package(Vulkan QUIET)
    endif()

    if(ALLFETCH_ENABLE_VULKAN AND Vulkan_FOUND)
        target_link_libraries(allfetch PRIVATE Vulkan::Vulkan)
        target_compile_definitions(allfetch PRIVATE
            ALLFETCH_HAS_VULKAN=1
        )
    endif()

    find_package(PkgConfig QUIET)

    if(PkgConfig_FOUND)
        pkg_check_modules(XRANDR QUIET IMPORTED_TARGET x11 xrandr)

        if(XRANDR_FOUND)
            target_link_libraries(allfetch PRIVATE PkgConfig::XRANDR)
            target_compile_definitions(allfetch PRIVATE
                ALLFETCH_HAS_XRANDR=1
            )
        endif()
        pkg_check_modules(GIO QUIET IMPORTED_TARGET gio-2.0)

        if(GIO_FOUND)
            target_link_libraries(allfetch PRIVATE PkgConfig::GIO)

            target_compile_definitions(allfetch PRIVATE
                ALLFETCH_HAS_GIO=1
            )
        endif()
    endif()
endif()

set_target_properties(allfetch PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

install(TARGETS allfetch RUNTIME DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)
install(FILES licenses/nlohmann-json.txt DESTINATION licenses)
foreach(file IN LISTS logo_files)
    file(RELATIVE_PATH relative "${logo_root}" "${file}")
    get_filename_component(folder "${relative}" DIRECTORY)
    install(FILES "${file}" DESTINATION "logos/${folder}")
endforeach()

option(ALLFETCH_PACKAGE_RELEASE "Configure a portable-layout release archive" OFF)
if(ALLFETCH_PACKAGE_RELEASE)
    include(tools/release_package.cmake)
endif()
