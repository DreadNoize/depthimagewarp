###############################################################################
# set sources
###############################################################################
FILE(GLOB EXAMPLE_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)

GET_FILENAME_COMPONENT(_EXE_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
SET(_EXE_NAME example_${_EXE_NAME}.out)
PROJECT(${_EXE_NAME})

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

INCLUDE_DIRECTORIES( ${INCLUDE_PATHS} 
                     ${CMAKE_CURRENT_SOURCE_DIR}/include 
                     ${GLEW_INCLUDE_DIR}
                     ${SCHISM_INCLUDE_DIRS}
                     ${GLFW_INCLUDE_DIRS}
)

SET(LIBRARY_DIRS ${LIB_PATHS} 
)

LINK_DIRECTORIES (${LIBRARY_DIRS})

ADD_EXECUTABLE( ${_EXE_NAME}
    ${EXAMPLE_SRC}
)

SET_TARGET_PROPERTIES( ${_EXE_NAME} PROPERTIES COMPILE_FLAGS ${BUILD_FLAGS})

###############################################################################
# dependencies
###############################################################################
#ADD_DEPENDENCIES(${_EXE_NAME})

TARGET_LINK_LIBRARIES(${_EXE_NAME} 
                      debug ${FREEIMAGE_LIBRARY_DEBUG} optimized ${FREEIMAGE_LIBRARY}
                      debug ${FREEIMAGE_PLUS_LIBRARY_DEBUG} optimized ${FREEIMAGE_PLUS_LIBRARY}
                      debug ${Boost_SYSTEM_LIBRARY_DEBUG} optimized ${Boost_SYSTEM_LIBRARY}
                      debug ${Boost_LOG_LIBRARY_DEBUG} optimized ${Boost_LOG_LIBRARY}
                      debug ${Boost_THREAD_LIBRARY_DEBUG} optimized ${Boost_THREAD_LIBRARY}
                      debug ${SCHISM_CORE_LIBRARY_DEBUG} optimized ${SCHISM_CORE_LIBRARY}
                      debug ${SCHISM_GL_CORE_LIBRARY_DEBUG} optimized ${SCHISM_GL_CORE_LIBRARY}
                      debug ${SCHISM_GL_UTIL_LIBRARY_DEBUG} optimized ${SCHISM_GL_UTIL_LIBRARY}
                      debug ${GLFW_LIBRARIES} optimized ${GLFW_LIBRARIES}
                      )

IF (MSVC)
  TARGET_LINK_LIBRARIES(${_EXE_NAME} OpenGL32.lib)
ENDIF (MSVC)