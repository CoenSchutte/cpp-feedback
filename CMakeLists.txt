cmake_minimum_required(VERSION 3.15)
project(odmr_gui_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Enable warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

include(FetchContent)

# External dependencies
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        latest
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(implot)

FetchContent_Declare(
    implot3d
    GIT_REPOSITORY https://github.com/brenocq/implot3d.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(implot3d)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# App icon setup
set(APP_ICON resources/icon.png)
set(APP_ICNS resources/odmr.icns)
set(APP_ICO resources/icon.ico)

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE odmr.icns)
    set_source_files_properties(${APP_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

if(WIN32)
    set(WINDOWS_ICON resources/icon.rc)
endif()

# Source files
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

# App executable
add_executable(odmr_gui
    ${PROJECT_SOURCES}
    ${WINDOWS_ICON}
    ${APP_ICON}
    ${APP_ICNS}
    ${APP_ICO}

    # ImGui core
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

    # ImGui backend
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp

    # ImPlot
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp

    # ImPlot3D
    ${implot3d_SOURCE_DIR}/implot3d.cpp
    ${implot3d_SOURCE_DIR}/implot3d_items.cpp
)

target_compile_definitions(odmr_gui PRIVATE IMGUI_ENABLE_DOCKING)

# Include paths
target_include_directories(odmr_gui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
    ${implot3d_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/fonts
)

# Link libraries
find_package(OpenGL REQUIRED)
target_link_libraries(odmr_gui PRIVATE
    glfw
    OpenGL::GL
    nlohmann_json::nlohmann_json
)



# Find and link VISA library
if(WIN32)
    find_library(VISA_LIBRARY visa32 HINTS "$ENV{VXIPNPPATH}/WinNT/lib" "$ENV{VISA_ROOT}/lib")
elseif(APPLE)
    find_library(VISA_LIBRARY visa HINTS /Library/Frameworks /usr/local/lib /opt/homebrew/lib)
elseif(UNIX)
    find_library(VISA_LIBRARY visa HINTS /usr/lib /usr/local/lib)
endif()

if(VISA_LIBRARY)
    message(STATUS "Found VISA library: ${VISA_LIBRARY}")
    target_link_libraries(odmr_gui PRIVATE ${VISA_LIBRARY})
else()
    message(WARNING "VISA library not found! VISA features will be unavailable.")
endif()


# macOS app bundle setup
if(APPLE)
    set_target_properties(odmr_gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist"
    )
endif()

# Install rules
install(TARGETS odmr_gui
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

# Linux install icon and desktop entry
if(UNIX AND NOT APPLE)
    install(FILES resources/icon.png DESTINATION share/icons/hicolor/256x256/apps RENAME odmr_gui.png)
    install(FILES odmr_gui.desktop DESTINATION share/applications)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)
enable_testing()

# Gather test sources
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)

# Only include core source files — not GUI or main
set(CORE_SOURCES
    src/measurement.cpp
    src/settings.cpp
    src/utils.cpp
)

# Define test target
add_executable(tests
    ${TEST_SOURCES}
    ${CORE_SOURCES}
)

# Include directories for testing
target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Link required libraries
target_link_libraries(tests PRIVATE
    gtest_main
    nlohmann_json::nlohmann_json
    ${VISA_LIBRARY}
)

# Optional: enable coverage
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Enabling LLVM coverage instrumentation")
    target_compile_options(tests PRIVATE -fprofile-instr-generate -fcoverage-mapping -O0 -g)
    target_link_options(tests PRIVATE -fprofile-instr-generate)
endif()

# Add test run command
add_test(NAME AllTests COMMAND tests)
