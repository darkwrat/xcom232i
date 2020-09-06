cmake_minimum_required(VERSION 3.11.4)
project(xcic)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(FUSE_INCLUDE_DIR /usr/include/fuse3)

find_package(MsgPuck REQUIRED)
find_package(Tarantool REQUIRED)
find_package(Small REQUIRED)
find_package(Fuse3 REQUIRED)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_library(xcic SHARED
	${SOURCE_DIR}/xcic.c
	${THIRD_PARTY_DIR}/scomlib/scom_data_link.c
	${THIRD_PARTY_DIR}/scomlib/scom_property.c
)

target_link_libraries(xcic ${MSGPUCK_LIBRARIES} ${TARANTOOL_LIBRARIES} ${SMALL_LIBRARIES})
target_include_directories(xcic PRIVATE ${THIRD_PARTY_DIR}/scomlib ${MSGPUCK_INCLUDE_DIRS} ${TARANTOOL_INCLUDE_DIRS} ${SMALL_INCLUDE_DIRS})
target_compile_options(xcic PRIVATE -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes)
set_target_properties(xcic PROPERTIES PREFIX "" OUTPUT_NAME xcic)
set_property(TARGET xcic PROPERTY C_STANDARD 11)
set_property(TARGET xcic PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(fsc SHARED
	${SOURCE_DIR}/fsc.c
)

target_link_libraries(fsc ${MSGPUCK_LIBRARIES} ${TARANTOOL_LIBRARIES} ${SMALL_LIBRARIES} ${FUSE3_LIBRARIES})
target_include_directories(fsc PRIVATE ${MSGPUCK_INCLUDE_DIRS} ${TARANTOOL_INCLUDE_DIRS} ${SMALL_INCLUDE_DIRS} ${FUSE3_INCLUDE_DIRS})
target_compile_options(fsc PRIVATE -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes -D_FILE_OFFSET_BITS=64)
set_target_properties(fsc PROPERTIES PREFIX "" OUTPUT_NAME fsc)
set_property(TARGET fsc PROPERTY C_STANDARD 11)
set_property(TARGET fsc PROPERTY POSITION_INDEPENDENT_CODE ON)
