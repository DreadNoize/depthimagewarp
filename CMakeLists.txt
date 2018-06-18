################################################################
# DEPTHIMAGEWARP
################################################################
SET(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
PROJECT(DEPTHIMAGEWARP CXX)

# cmake minimum version 2.8.0
cmake_minimum_required(VERSION 2.8.0)

if (UNIX)
	find_package(PkgConfig)
endif(UNIX)

# Location where cmake first looks for modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        SET(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Default install
                 prefix" FORCE)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT) 

################################################################
# DEPTHIMAGEWARP Configuration
################################################################

################################################################
# Configure and find dependencies
################################################################
SET(GLOBAL_EXT_DIR ${CMAKE_SOURCE_DIR}/externals)

if (UNIX)
	pkg_check_modules(GL REQUIRED gl) 
endif (UNIX)

include(macros)

##boost#########################################################
set(BOOST_INCLUDEDIR ${GLOBAL_EXT_DIR}/boost/include)
set(BOOST_LIBRARYDIR ${GLOBAL_EXT_DIR}/boost/lib)
set(Boost_USE_MULTITHREADED ON)

IF (MSVC)
  set (Boost_USE_STATIC_LIBS ON)
ENDIF(MSVC)

find_package(Boost 1.58.0 REQUIRED thread regex system program_options timer chrono filesystem date_time log log_setup)

ADD_DEFINITIONS("-DBOOST_ALL_NO_LIB")

##freeimage#####################################################
include(FindFreeImage)

IF (UNIX)
  SET(FREEIMAGE_LIBRARY_DEBUG ${FREEIMAGE_LIBRARY})
  SET(FREEIMAGE_PLUS_LIBRARY_DEBUG ${FREEIMAGE_PLUS_LIBRARY})
ENDIF (UNIX)

## schism ######################################################
include(FindSchism)

## glfw ###################################################
find_package(GLFW3 REQUIRED)


################################################################
# Write Configuration
################################################################
set(LIB_PATHS
    ${BOOST_LIBRARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

set(INCLUDE_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

################################################################
# preprocessor configuration
################################################################
IF (UNIX)
	set(CMAKE_CXX_FLAGS_RELEASE "-s -O4 -fPIC -std=c++1y -DBOOST_LOG_DYN_LINK -lpthread")
	set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -fPIC -std=c++1y -DBOOST_LOG_DYN_LINK -lpthread")
ELSEIF(MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "-D NDEBUG -D _SECURE_SCL=0 -D _SCL_SECURE_NO_WARNINGS -D _CRT_SECURE_NO_DEPRECATE /MD /MP")
	set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG /MDd /Zi /MP")
  add_definitions(-D _USE_MATH_DEFINES -D NOMINMAX)
ENDIF(UNIX)

################################################################
# Create build flags
################################################################
SET (BUILD_FLAGS " ")

################################################################
# Create libraries
################################################################
SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)

ADD_SUBDIRECTORY(examples)

################################################################
# Summary
################################################################

message(STATUS "" )
message(STATUS "Summary:" )
message(STATUS " build type: ${CMAKE_BUILD_TYPE}" )
message(STATUS "" )
