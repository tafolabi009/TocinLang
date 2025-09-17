cmake_minimum_required(VERSION 3.16)
project(Tocin LANGUAGES C CXX)

# Set C++ standard to C++17 for better compatibility
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable verbose output for debugging
set(CMAKE_VERBOSE_MAKEFILE ON)

# Add compiler flags and definitions
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_FORMAT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)

# -------------------------
# User-Selectable Feature Flags
# -------------------------
option(WITH_PYTHON "Enable Python FFI" ON)
option(WITH_V8 "Enable JavaScript (V8) FFI" ON)
option(WITH_ZSTD "Enable zstd compression" ON)
option(WITH_XML "Enable XML support (LibXml2)" ON)
option(WITH_MACROS "Enable macro system" ON)
option(WITH_ASYNC "Enable async/await support" ON)
option(WITH_DEBUGGER "Enable debugger support" ON)
option(WITH_WASM "Enable WebAssembly target" ON)
option(WITH_PACKAGE_MANAGER "Enable package manager" ON)

# -------------------------
# ZLIB Integration
# -------------------------
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

# -------------------------
# LLVM Integration
# -------------------------
<<<<<<< Current (Your changes)
# First try to find LLVM through standard paths
set(`LLVM_DIR "D:/Downloads/msys64/mingw64/lib/cmake/llvm")
=======
# First try to find LLVM through standard paths; on Windows allow a fallback hint
if(WIN32 AND NOT DEFINED LLVM_DIR)
    set(LLVM_DIR "D:/Downloads/msys64/mingw64/lib/cmake/llvm")
endif()
>>>>>>> Incoming (Background Agent changes)
find_package(LLVM REQUIRED CONFIG)

if(LLVM_FOUND)
    message(STATUS "Found LLVM version ${LLVM_PACKAGE_VERSION}")
    message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
    message(STATUS "LLVM library dirs: ${LLVM_LIBRARY_DIRS}")

    # LLVM configuration options
    set(LLVM_LINK_LLVM_DYLIB ON) # Use the single LLVM shared library
    set(LLVM_ENABLE_RTTI ON)
    set(LLVM_ENABLE_ZLIB ON)

    include_directories(${LLVM_INCLUDE_DIRS})
    add_definitions(${LLVM_DEFINITIONS})
    link_directories(${LLVM_LIBRARY_DIRS})

    # Optional header test for Windows environments only
    if(WIN32)
        # Create header tests to check if we can find LLVM headers
        file(WRITE "${CMAKE_BINARY_DIR}/llvm_host_test.cpp" "#include <llvm/Support/Host.h>\nint main() { return 0; }")

        try_compile(LLVM_HOST_HEADER_TEST
            ${CMAKE_BINARY_DIR}
            ${CMAKE_BINARY_DIR}/llvm_host_test.cpp
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LLVM_INCLUDE_DIRS}")

        if(NOT LLVM_HOST_HEADER_TEST)
            message(WARNING "LLVM Host.h header not found in standard include paths, trying additional locations")

            # Try finding the header manually in common locations
            file(GLOB_RECURSE LLVM_HOST_HEADER_PATH
                "D:/Downloads/msys64/mingw64/include/**/Host.h"
                "D:/Downloads/msys64/mingw64/lib/llvm/include/**/Host.h")

            if(LLVM_HOST_HEADER_PATH)
                message(STATUS "Found LLVM Host.h at ${LLVM_HOST_HEADER_PATH}")
                get_filename_component(LLVM_HOST_HEADER_DIR "${LLVM_HOST_HEADER_PATH}" DIRECTORY)
                get_filename_component(LLVM_SUPPORT_DIR "${LLVM_HOST_HEADER_DIR}" DIRECTORY)
                get_filename_component(LLVM_INCLUDE_DIR "${LLVM_SUPPORT_DIR}" DIRECTORY)

                # Add this directory to includes
                include_directories(${LLVM_INCLUDE_DIR})
            else()
                message(STATUS "Could not find LLVM Host.h, trying alternative LLVM includes")

                # Add alternative LLVM include directories
                include_directories(
                    "D:/Downloads/msys64/mingw64/include/llvm"
                    "D:/Downloads/msys64/mingw64/include/llvm-c"
                    "D:/Downloads/msys64/usr/include/llvm"
                    "/mingw64/include"
                    "/mingw64/include/llvm"
                    "/mingw64/include/llvm-c"
                )
            endif()
        endif()
    endif()

    # Resolve LLVM libraries: prefer monolithic libLLVM when available
    if(LLVM_LINK_LLVM_DYLIB)
        set(LLVM_LIBS LLVM)
    else()
        llvm_map_components_to_libnames(LLVM_LIBS
            Core
            IRReader
            Support
            Passes
            OrcJIT
        )
    endif()
else()
    message(FATAL_ERROR "LLVM not found. Please ensure LLVM is installed.")
endif()

# -------------------------
# Python Integration
# -------------------------
if(WITH_PYTHON)
    find_package(Python COMPONENTS Interpreter Development)
    if(Python_FOUND)
        # Try to find Python headers in common locations
        find_path(PYTHON_INCLUDE_DIR Python.h
            PATHS
            ${Python_INCLUDE_DIRS}
            ${Python_INCLUDE_DIR}
            ${Python_ROOT_DIR}/include
            ${Python_ROOT_DIR}/include/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}
            ${Python_ROOT_DIR}/include/python${Python_VERSION_MAJOR}
            /usr/include/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}
            /usr/include/python${Python_VERSION_MAJOR}
            /usr/local/include/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}
            /usr/local/include/python${Python_VERSION_MAJOR}
            C:/msys64/mingw64/include/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}
            C:/msys64/mingw64/include/python${Python_VERSION_MAJOR}
            C:/Python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}/include
            C:/Python${Python_VERSION_MAJOR}/include
            ${Python_EXECUTABLE}/../include
            ${Python_EXECUTABLE}/../include/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}
            ${Python_EXECUTABLE}/../include/python${Python_VERSION_MAJOR}
        )
        
        if(PYTHON_INCLUDE_DIR)
            include_directories(${PYTHON_INCLUDE_DIR})
            add_definitions(-DWITH_PYTHON)
            message(STATUS "Python found at: ${Python_EXECUTABLE}")
            message(STATUS "Python headers found at: ${PYTHON_INCLUDE_DIR}")
        else()
            message(WARNING "Python headers not found. Python-related functionality will be disabled.")
            set(Python_FOUND FALSE)
        endif()
    else()
        message(WARNING "Python not found. Python-related functionality will be disabled.")
    endif()
else()
    message(STATUS "WITH_PYTHON=OFF: Python FFI disabled by user.")
endif()

# -------------------------
# V8 Integration
# -------------------------
if(WITH_V8)
    # Search for V8 in multiple locations
    find_path(V8_INCLUDE_DIR v8.h
        PATHS
        ${V8_INCLUDE_DIR}
        C:/msys64/mingw64/include
        C:/msys64/mingw64/include/v8
        C:/msys64/usr/include
        C:/msys64/usr/include/v8
        /mingw64/include
        /mingw64/include/v8
        /usr/include
        /usr/include/v8
        /usr/local/include
        /usr/local/include/v8
        $ENV{V8_ROOT}/include
        $ENV{V8_ROOT}/include/v8
    )
    
    find_library(V8_LIBRARY 
        NAMES v8 libv8 v8.dll.a libv8.dll.a
        PATHS
        ${V8_LIBRARY_DIRS}
        C:/msys64/mingw64/lib
        C:/msys64/usr/lib
        /mingw64/lib
        /usr/lib
        /usr/local/lib
        $ENV{V8_ROOT}/lib
    )
    
    if(V8_INCLUDE_DIR AND V8_LIBRARY)
        set(V8_FOUND TRUE)
        message(STATUS "Found V8 headers in: ${V8_INCLUDE_DIR}")
        message(STATUS "Found V8 library: ${V8_LIBRARY}")
        include_directories(${V8_INCLUDE_DIR})
        add_definitions(-DWITH_V8)
    else()
        set(V8_FOUND FALSE)
        message(WARNING "V8 not found. V8-related functionality will be disabled.")
        message(STATUS "To install V8 in MSYS2, run: pacman -S mingw-w64-x86_64-v8")
        message(STATUS "Or set WITH_V8=OFF to disable V8 support")
    endif()
else()
    set(V8_FOUND FALSE)
    message(STATUS "WITH_V8=OFF: JavaScript FFI disabled by user.")
endif()

# -------------------------
# V8 Platform Dependencies
# -------------------------
if(V8_FOUND)
    set(V8_PLATFORM_LIBRARIES
        ${V8_LIBRARY}
    )

    find_library(V8_PLATFORM_LIBRARY
        NAMES v8_libplatform v8_libplatform.dll.a libv8_libplatform libv8_libplatform.dll.a
        HINTS "C:/msys64/mingw64/lib")

    if(V8_PLATFORM_LIBRARY)
        list(APPEND V8_PLATFORM_LIBRARIES ${V8_PLATFORM_LIBRARY})
    endif()

    find_library(V8_BASE_LIBRARY
        NAMES v8_libbase v8_libbase.dll.a libv8_libbase libv8_libbase.dll.a
        HINTS "C:/msys64/mingw64/lib")

    if(V8_BASE_LIBRARY)
        list(APPEND V8_PLATFORM_LIBRARIES ${V8_BASE_LIBRARY})
    endif()
endif()

# -------------------------
# FFI (libffi) Integration
# -------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFI REQUIRED libffi)
include_directories(${FFI_INCLUDE_DIRS})

# -------------------------
# zstd Integration
# -------------------------
if(WITH_ZSTD)
    find_package(zstd)
    if(zstd_FOUND)
        include_directories(${zstd_INCLUDE_DIRS})
        add_definitions(-DWITH_ZSTD)
    else()
        message(WARNING "zstd not found. zstd compression functionality will be disabled.")
    endif()
else()
    message(STATUS "WITH_ZSTD=OFF: zstd disabled by user.")
endif()

# -------------------------
# LibXml2 Integration
# -------------------------
if(WITH_XML)
    find_package(LibXml2)
    if(LibXml2_FOUND)
        include_directories(${LibXml2_INCLUDE_DIRS})
        add_definitions(-DWITH_XML)
    else()
        message(WARNING "LibXml2 not found. XML functionality will be disabled.")
    endif()
else()
    message(STATUS "WITH_XML=OFF: XML support disabled by user.")
endif()

# -------------------------
# Thread Support
# -------------------------
find_package(Threads REQUIRED)

if(THREADS_HAVE_PTHREAD_ARG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# -------------------------
# Source Files
# -------------------------
# Find all source files
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Exclude test files that have their own main function
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/test.cpp")

# Find all header files
file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

# Add new feature files specifically
set(FEATURE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/type/ownership.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/type/result_option.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/concurrency.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ast/defer_stmt.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ast/property.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/linq.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/type/null_safety.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/type/extension_functions.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/type/traits.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/type/move_semantics.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/type/feature_integration.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/macro_system.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/async_system.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/debugger/debugger.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/targets/wasm_target.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/package/package_manager.h"
)

# Add the feature headers to the existing headers list
list(APPEND HEADERS ${FEATURE_HEADERS})

# Create executable
add_executable(tocin ${SOURCES}
        src/error/error_handler.cpp
        src/main.cpp)

# Include Project Headers
target_include_directories(tocin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)
if(WIN32)
    target_include_directories(tocin PRIVATE
        D:/Downloads/msys64/mingw64/include
        D:/Downloads/msys64/mingw64/include/c++/15.1.0
        D:/Downloads/msys64/mingw64/include/c++/15.1.0/x86_64-w64-mingw32
        D:/Downloads/msys64/mingw64/include/c++/15.1.0/backward
        D:/Downloads/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/15.1.0/include
        D:/Downloads/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/15.1.0/include-fixed
        D:/Downloads/msys64/mingw64/x86_64-w64-mingw32/include
    )
endif()

# Add optional includes
if(Python_FOUND)
    target_include_directories(tocin PRIVATE ${Python_INCLUDE_DIRS})
endif()

if(V8_FOUND)
    target_include_directories(tocin PRIVATE ${V8_INCLUDE_DIR})
endif()

if(LibXml2_FOUND)
    target_include_directories(tocin PRIVATE ${LibXml2_INCLUDE_DIRS})
endif()

if(zstd_FOUND)
    target_include_directories(tocin PRIVATE ${zstd_INCLUDE_DIRS})
endif()

# -------------------------
# Linking Libraries
# -------------------------
set(LINK_LIBS
    ${ZLIB_LIBRARIES}
    ${FFI_LIBRARIES}
)

# Link against LLVM (monolithic libLLVM preferred)
if(LLVM_LINK_LLVM_DYLIB)
    list(APPEND LINK_LIBS LLVM)
else()
    list(APPEND LINK_LIBS ${LLVM_LIBS})
endif()

# Threads
find_package(Threads REQUIRED)
if(Threads_FOUND)
    list(APPEND LINK_LIBS Threads::Threads)
endif()

# Platform-specific system libraries
if(WIN32)
    list(APPEND LINK_LIBS
        mingw32
        ws2_32
        advapi32
        shell32
        user32
        kernel32
        ole32
        oleaut32
        uuid
        version
        z
    )
endif()

# Add optional libraries
if(Python_FOUND)
    list(APPEND LINK_LIBS ${Python_LIBRARIES})
endif()

if(V8_FOUND)
    list(APPEND LINK_LIBS ${V8_PLATFORM_LIBRARIES})
endif()

if(LibXml2_FOUND)
    list(APPEND LINK_LIBS ${LibXml2_LIBRARIES})
endif()

if(zstd_FOUND)
    list(APPEND LINK_LIBS ${zstd_LIBRARIES})
endif()

target_link_libraries(tocin PRIVATE ${LINK_LIBS})

# -------------------------
# Custom IR Generator Build
# -------------------------
add_custom_target(ir_generator_direct
    COMMAND ${CMAKE_CXX_COMPILER}
    -c ${CMAKE_SOURCE_DIR}/src/codegen/ir_generator.cpp
    -o ${CMAKE_BINARY_DIR}/ir_generator.o
    -I${CMAKE_SOURCE_DIR}/src
    -std=c++17
    -I${LLVM_INCLUDE_DIRS}
    -I${CMAKE_SOURCE_DIR}/src/ast
    -I${CMAKE_SOURCE_DIR}/src/lexer
    -I${CMAKE_SOURCE_DIR}/src/type
    -I${CMAKE_SOURCE_DIR}/src/error
    -I${CMAKE_SOURCE_DIR}/src/runtime
    -I${CMAKE_SOURCE_DIR}/src/compiler
    ${LLVM_DEFINITIONS}
    COMMENT "Building IR generator directly with g++"
)

# Add this to the main target dependencies
add_dependencies(tocin ir_generator_direct)

# -------------------------
# Feature-specific definitions
# -------------------------
if(WITH_MACROS)
    add_definitions(-DWITH_MACROS)
    message(STATUS "Macro system enabled")
endif()

if(WITH_ASYNC)
    add_definitions(-DWITH_ASYNC)
    message(STATUS "Async/await support enabled")
endif()

if(WITH_DEBUGGER)
    add_definitions(-DWITH_DEBUGGER)
    message(STATUS "Debugger support enabled")
endif()

if(WITH_WASM)
    add_definitions(-DWITH_WASM)
    message(STATUS "WebAssembly target enabled")
endif()

if(WITH_PACKAGE_MANAGER)
    add_definitions(-DWITH_PACKAGE_MANAGER)
    message(STATUS "Package manager enabled")
endif()

# -------------------------
# CPack Installer Setup
# -------------------------
# Installer Generation
if(WIN32)
    # NSIS Installer
    include(InstallRequiredSystemLibraries)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/Tocin_Logo.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/Tocin_Logo.ico")
    set(CPACK_NSIS_DISPLAY_NAME "Tocin Compiler")
    set(CPACK_NSIS_PACKAGE_NAME "Tocin Compiler")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_MENU_LINKS
        "https://tocin.vercel.app" "Website"
        "https://github.com/tafolabi009/tocin_compiler" "GitHub"
    )
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Tocin Compiler")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_VERSION_MAJOR "1")
    set(CPACK_PACKAGE_VERSION_MINOR "0")
    set(CPACK_PACKAGE_VERSION_PATCH "0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tocin Programming Language Compiler")
    set(CPACK_PACKAGE_VENDOR "Tocin Team")
    set(CPACK_PACKAGE_CONTACT "contact@tocin.dev")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        CreateShortCut \\\"$DESKTOP\\\\Tocin Compiler.lnk\\\" \\\"$INSTDIR\\\\tocin.exe\\\"
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        Delete \\\"$DESKTOP\\\\Tocin Compiler.lnk\\\"
    ")
    include(CPack)
elseif(APPLE)
    # macOS DMG Installer
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Tocin Compiler")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/setup_dmg.scpt")
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/Tocin_Logo.icns")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_VERSION_MAJOR "1")
    set(CPACK_PACKAGE_VERSION_MINOR "0")
    set(CPACK_PACKAGE_VERSION_PATCH "0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tocin Programming Language Compiler")
    set(CPACK_PACKAGE_VENDOR "Tocin Team")
    set(CPACK_PACKAGE_CONTACT "afolabi@genovotech.com")
    include(CPack)
endif()

# -------------------------
# Install required DLLs
# -------------------------
install(TARGETS tocin DESTINATION bin COMPONENT Applications)

if(WIN32)
    # Base system DLLs
    install(FILES
        "C:/msys64/mingw64/bin/libwinpthread-1.dll"
        "C:/msys64/mingw64/bin/libstdc++-6.dll"
        "C:/msys64/mingw64/bin/libgcc_s_seh-1.dll"
        DESTINATION bin COMPONENT Applications
    )

    # Python DLLs (if Python is found)
    if(Python_FOUND)
        if(EXISTS ${Python_EXECUTABLE})
            get_filename_component(PYTHON_DIR ${Python_EXECUTABLE} DIRECTORY)
            file(GLOB PYTHON_DLLS "${PYTHON_DIR}/python*.dll")
            install(FILES ${PYTHON_DLLS} DESTINATION bin COMPONENT Applications)
        endif()
    endif()

    # V8 DLLs (if V8 is found)
    if(V8_FOUND)
        install(FILES
            "C:/msys64/mingw64/bin/libv8.dll"
            DESTINATION bin COMPONENT Applications
        )

        # Try to find additional V8 DLLs
        if(EXISTS "C:/msys64/mingw64/bin/libv8_libbase.dll")
            install(FILES "C:/msys64/mingw64/bin/libv8_libbase.dll" DESTINATION bin COMPONENT Applications)
        endif()

        if(EXISTS "C:/msys64/mingw64/bin/libv8_libplatform.dll")
            install(FILES "C:/msys64/mingw64/bin/libv8_libplatform.dll" DESTINATION bin COMPONENT Applications)
        endif()
    endif()

    # ICU DLLs (for V8)
    if(EXISTS "C:/msys64/mingw64/bin/libicudt*.dll")
        file(GLOB ICU_DLLS "C:/msys64/mingw64/bin/libicudt*.dll")
        install(FILES ${ICU_DLLS} DESTINATION bin COMPONENT Applications)
    endif()

    if(EXISTS "C:/msys64/mingw64/bin/libicuin*.dll")
        file(GLOB ICU_DLLS "C:/msys64/mingw64/bin/libicuin*.dll")
        install(FILES ${ICU_DLLS} DESTINATION bin COMPONENT Applications)
    endif()

    if(EXISTS "C:/msys64/mingw64/bin/libicuuc*.dll")
        file(GLOB ICU_DLLS "C:/msys64/mingw64/bin/libicuuc*.dll")
        install(FILES ${ICU_DLLS} DESTINATION bin COMPONENT Applications)
    endif()

    # zlib DLLs
    install(FILES "C:/msys64/mingw64/bin/libzlib.dll" DESTINATION bin COMPONENT Applications)

    # zstd DLLs (if zstd is found)
    if(zstd_FOUND)
        if(EXISTS "C:/msys64/mingw64/bin/libzstd.dll")
            install(FILES "C:/msys64/mingw64/bin/libzstd.dll" DESTINATION bin COMPONENT Applications)
        endif()
    endif()

    # LibXml2 DLLs (if LibXml2 is found)
    if(LibXml2_FOUND)
        if(EXISTS "C:/msys64/mingw64/bin/libxml2-2.dll")
            install(FILES "C:/msys64/mingw64/bin/libxml2-2.dll" DESTINATION bin COMPONENT Applications)
        endif()
    endif()

    # FFI DLLs
    if(EXISTS "C:/msys64/mingw64/bin/libffi-*.dll")
        file(GLOB FFI_DLLS "C:/msys64/mingw64/bin/libffi-*.dll")
        install(FILES ${FFI_DLLS} DESTINATION bin COMPONENT Applications)
    endif()
endif()

# Additional documentation and examples
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION doc COMPONENT Documentation
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples" DESTINATION . COMPONENT Examples)
endif()

# -------------------------
# CPack Final Setup
# -------------------------
set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Tocin Compiler Application")
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Example Programs")
set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION "Main executable and required libraries")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "Documentation files")
set(CPACK_COMPONENT_EXAMPLES_DESCRIPTION "Example Tocin programs")

set(CPACK_COMPONENT_APPLICATIONS_REQUIRED TRUE)
set(CPACK_COMPONENT_EXAMPLES_DEPENDS applications)
set(CPACK_COMPONENT_DOCUMENTATION_DEPENDS applications)

include(CPack)

# -------------------------
# Advanced CMake Options
# -------------------------
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install path prefix.")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Release/Debug)")
set(CMAKE_SKIP_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# -------------------------
# Install headers, stdlib, and docs
# -------------------------
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/ DESTINATION include/tocin FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stdlib/ DESTINATION share/tocin/stdlib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/ DESTINATION share/tocin/docs PATTERN "*.md")

# -------------------------
# Uninstall target
# -------------------------
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake)
endif()
