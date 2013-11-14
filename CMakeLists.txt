cmake_minimum_required(VERSION 2.6)

PROJECT(scidb_rdb)

#if you don't want the full compiler output, remove the following line
SET(CMAKE_VERBOSE_MAKEFILE ON)

#add definitions, compiler switches, etc.
IF (CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR CMAKE_BUILD_TYPE STREQUAL "debug" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  MESSAGE ("This is a Debug Build")
  ADD_DEFINITIONS(-Wall) # for debug. -O2 confuses debugger
ELSE (CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR CMAKE_BUILD_TYPE STREQUAL "debug" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  MESSAGE ("This is NOT a Debug Build")
  ADD_DEFINITIONS(-Wall -O2)
ENDIF (CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR CMAKE_BUILD_TYPE STREQUAL "debug" OR CMAKE_BUILD_TYPE STREQUAL "Debug")

find_package(Boost REQUIRED thread)

# mysql client header/lib folder
INCLUDE_DIRECTORIES(/usr/include/mysql/ /home/jennie/install/include)

LINK_DIRECTORIES(/usr/lib/mysql/ /home/jennie/install/lib)

#list all source files here
ADD_EXECUTABLE(scidb_rdb main.cpp configs.cpp loader.cpp mysqlwrapper.cpp queries.cpp dummydata.cpp cook.cpp cookgroup.cpp)

ADD_EXECUTABLE(group_test groupImpl.cpp groupBatchImpl.cpp stubGroup.cpp groupingStub.cpp group.cpp configs.cpp mysqlwrapper.cpp)

ADD_EXECUTABLE(benchGen benchGen.cc)

# log4cxx and MySQL C Connector
# if you don't have them installed yet, do:
#  yum install mysql-devel
#  yum install log4cxx
# or equivalent.
TARGET_LINK_LIBRARIES(scidb_rdb log4cxx mysqlclient ${Boost_LIBRARIES}) 
TARGET_LINK_LIBRARIES(group_test log4cxx mysqlclient)

 
