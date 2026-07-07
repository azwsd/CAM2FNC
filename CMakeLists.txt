cmake_minimum_required(VERSION 3.16)
project(CAM2FNC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find wxWidgets. Adjust these paths if your install is in a different location.
set(wxWidgets_ROOT_DIR   "C:/wxWidgets-3.3.2")
set(wxWidgets_LIB_DIR    "C:/wxWidgets-3.3.2/lib/vc_x64_dll")
set(wxWidgets_CONFIGURATION mswu)   # Release unicode DLL build

find_package(wxWidgets REQUIRED COMPONENTS core base adv)
include(${wxWidgets_USE_FILE})

set(SOURCES
    src/App.cpp
    src/MainFrame.cpp
    src/SettingsDialog.cpp
    src/CamParser.cpp
    src/FncGenerator.cpp
    src/ActivationManager.cpp
    src/ActivationDialog.cpp
)

add_executable(cam2fnc WIN32 ${SOURCES})
target_link_libraries(cam2fnc ${wxWidgets_LIBRARIES})

# Windows icon and manifest.
if(WIN32)
    target_sources(cam2fnc PRIVATE src/app.rc)
endif()

# Install the main app binary.

# Private license key generator (local only — keygen/ is gitignored).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/keygen/keygen.cpp")
    add_executable(keygen keygen/keygen.cpp)
    set_target_properties(keygen PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded"
    )
endif()