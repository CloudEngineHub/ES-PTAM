project(mapper_emvs_stereo)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_minimum_required(VERSION 2.8.3)

find_package(catkin_simple REQUIRED COMPONENTS
                tf
                tf_conversions
                eigen_conversions
                image_transport
                vicon)
catkin_simple(ALL_DEPS_REQUIRED)

set(CMAKE_BUILD_TYPE RelWithDebInfo) # Release, RelWithDebInfo
set(CMAKE_CXX_FLAGS "-O3 -fopenmp ${CMAKE_CXX_FLAGS}")

set(HEADERS
  include/mapper_emvs_stereo/mapper_emvs_stereo.hpp
  include/mapper_emvs_stereo/depth_vector.hpp
  include/mapper_emvs_stereo/trajectory.hpp
  include/mapper_emvs_stereo/geometry_utils.hpp
  include/mapper_emvs_stereo/median_filtering.hpp
  include/mapper_emvs_stereo/calib.hpp
  include/mapper_emvs_stereo/utils.hpp
  include/mapper_emvs_stereo/process1_live.hpp
  include/mapper_emvs_stereo/mapper.hpp
)

set(SOURCES
  src/mapper_emvs_stereo.cpp
  src/median_filtering.cpp
  src/calib.cpp
  src/utils.cpp
  src/process1_live.cpp
  src/mapper.cpp
)

option(DEFINE_USE_INVERSE_DEPTH "Use linear spacing in inverse depth (if OFF, will use linear spacing in depth)" ON)
if(DEFINE_USE_INVERSE_DEPTH)
    add_definitions(-DUSE_INVERSE_DEPTH)
endif(DEFINE_USE_INVERSE_DEPTH)

option(DEFINE_TRI "Enable trinocular fusion" OFF)
if(DEFINE_TRI)
    add_definitions(-DTRI)
endif(DEFINE_TRI)

cs_add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Executables
################################################################################

cs_add_executable(run_emvs src/mapperNode.cpp)
target_link_libraries(run_emvs ${PROJECT_NAME} yaml-cpp)

################################################################################
cs_install()
cs_export()

