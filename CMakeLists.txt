cmake_minimum_required(VERSION 3.16...3.23)

# initialize pico-sdk from submodule
# note: this must happen before project()
if(DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Not ideal to use this global variable, but necessary to make sure
# that tooling and projects use the same version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example
# when compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add project_options v0.20.0
# https://github.com/cpp-best-practices/project_options
include(FetchContent)
FetchContent_Declare(_project_options URL https://github.com/aminya/project_options/archive/refs/tags/v0.20.0.zip)
FetchContent_MakeAvailable(_project_options)
include(${_project_options_SOURCE_DIR}/Index.cmake)

# Set the project name and language
project(
  picomultipointtemp
  VERSION 0.0.1
  DESCRIPTION ""
  LANGUAGES CXX C ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS ON)
pico_sdk_init()

set(GIT_SHA "Unknown" CACHE STRING "SHA this build was generated from")
string(SUBSTRING "${GIT_SHA}" 0 8 GIT_SHORT_SHA)

# defaulted_project_options sets recommended defaults and provides user and developer
# modes and full GUI support for choosing options at configure time

# for more flexibility, look into project_options() macro

# Any default can be overridden
# set(<feature_name> <value>) - set default for both user and developer modes
# set(<feature_name>_DEVELOPER <value>) - set default for developer mode
# set(<feature_name>_USER <value>) - set default for user mode
set(ENABLE_CLANG_TIDY OFF) # PICO SDK does not compile with clang
set(ENABLE_CPPCHECK OFF)
set(ENABLE_SANITIZER_ADDRESS OFF)
set(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR OFF)
set(ENABLE_COVERAGE OFF)
set(WARNINGS_AS_ERRORS OFF)

# Initialize project_options variable related to this project
# This overwrites `project_options` and sets `project_warnings`
# uncomment the options to enable them:
project_options(
  # Note: PCH is disabled by default in developer mode because these headers become
  # globally included and they can mask other errors
  PCH_HEADERS
  <vector>
  <string> # This is a list of headers to pre-compile, here are some common ones
  GCC_WARNINGS
  -fdata-sections
  -Wall
  -Wextra
  -Wpedantic
  -Wformat-security
  -Wno-format-zero-length
  -Werror=format-security
  -Wdouble-promotion
  -Wpointer-arith
  -Wnull-dereference
  -Wredundant-decls
  -Wshadow
  -Wswitch-default
  -Wcast-align
)

add_library(picomultipointtemp::project_options INTERFACE IMPORTED)
add_library(picomultipointtemp::project_warnings INTERFACE IMPORTED)

# Adding the src:
add_subdirectory(src)

if(CMAKE_SKIP_INSTALL_RULES)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

# Add other targets that you want installed here, be default we just package the one executable
# we know we want to ship
package_project(TARGETS picomultipointtemp project_options project_warnings)

# Experience shows that explicit package naming can help make it easier to sort
# out potential ABI related issues before they start, while helping you
# track a build to a specific GIT SHA
set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
)

include(CPack)
