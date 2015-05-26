cmake_minimum_required(VERSION 2.8.12)

project(davetray)

# Silence CMake 3.0 warnings
# if(POLICY CMP0043)
#     cmake_policy(SET CMP0043 OLD)
# endif()

find_package(ECM 0.0.11 REQUIRED NO_MODULE)
include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(GenerateExportHeader)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
# set(CMAKE_INCLUDE_CURRENT_DIR ON)
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
    Widgets
    X11Extras
)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

include(ECMInstallIcons)
include(ECMOptionalAddSubdirectory)

# All the required XCB components
#I probably don't need /all/ of these. Delete later
find_package(XCB
    REQUIRED COMPONENTS
        XCB
        XFIXES
        DAMAGE
        COMPOSITE
        SHAPE
        SYNC
        RENDER
        RANDR
        KEYSYMS
        IMAGE
        SHM
        UTIL
        GLX
)


add_definitions(-Wall -std=c++11)

set(HELPER_SOURCES
    main.cpp
    fdoselectionmanager.cpp
)

add_executable(davetray ${HELPER_SOURCES})

set(kwin_XCB_LIBS
    XCB::XCB
    XCB::XFIXES
    XCB::DAMAGE
    XCB::COMPOSITE
    XCB::SHAPE
    XCB::SYNC
    XCB::RENDER
    XCB::RANDR
    XCB::KEYSYMS
    XCB::SHM
    XCB::GLX
    XCB::UTIL
)
set_package_properties(XCB PROPERTIES TYPE REQUIRED)

target_link_libraries(davetray Qt5::Core Qt5::Widgets Qt5::X11Extras ${kwin_XCB_LIBS})
