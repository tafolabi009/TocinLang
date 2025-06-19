cmake_minimum_required(VERSION 3.10)
project(tocin VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Find required packages
find_package(LLVM REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add source files
set(SOURCES
    src/Interpreter.cpp
    src/Builtins.cpp
    src/Runtime.cpp
    src/Environment.cpp
    src/Repl.cpp
)

# Add header files
set(HEADERS
    include/Interpreter.h
    include/Builtins.h
    include/Runtime.h
    include/Environment.h
)

# Create library target
add_library(tocin_lib STATIC ${SOURCES} ${HEADERS})

# Link libraries to the library target
target_link_libraries(tocin_lib
    PRIVATE
    ${LLVM_LIBRARIES}
    ${Boost_LIBRARIES}
    Threads::Threads
)

# Create REPL executable
add_executable(tocin-repl src/Repl.cpp)
target_link_libraries(tocin-repl PRIVATE tocin_lib)

# Installation rules
include(GNUInstallDirs)
install(TARGETS tocin-repl tocin_lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tocin
)

# Package configuration
set(CPACK_PACKAGE_NAME "tocin")
set(CPACK_PACKAGE_VENDOR "Genovo Technologies")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tocin Programming Language Interpreter - Proprietary software created by Afolabi Oluwatosin for Genovo Technologies")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "tocin")
set(CPACK_PACKAGE_COPYRIGHT "Copyright (c) 2024 Genovo Technologies. All rights reserved.")

# Windows NSIS configuration
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "Tocin")
    set(CPACK_NSIS_DISPLAY_NAME "Tocin ${PROJECT_VERSION}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_MENU_LINKS
        "https://genovotechnologies.com" "Website"
        "https://genovotechnologies.com/support" "Support"
    )
endif()

# Linux DEB configuration
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Genovo Technologies <support@genovotechnologies.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libllvm-dev, libboost-all-dev")
endif()

# macOS DMG configuration
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Tocin")
    set(CPACK_DMG_FORMAT "UDZO")
endif()

# Enable packaging
include(CPack)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif() 