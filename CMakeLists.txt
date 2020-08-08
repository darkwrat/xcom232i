cmake_minimum_required(VERSION 3.11.4)
project(xcic)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(MsgPuck REQUIRED)
find_package(Tarantool REQUIRED)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_library(xcic SHARED
	${SOURCE_DIR}/xcic.c
	${THIRD_PARTY_DIR}/scomlib/scom_data_link.c
	${THIRD_PARTY_DIR}/scomlib/scom_property.c
)

target_link_libraries(xcic ${MSGPUCK_LIBRARIES} ${TARANTOOL_LIBRARIES})
target_include_directories(xcic PRIVATE ${THIRD_PARTY_DIR}/scomlib ${MSGPUCK_INCLUDE_DIRS} ${TARANTOOL_INCLUDE_DIRS})
target_compile_options(xcic PRIVATE -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes)
set_target_properties(xcic PROPERTIES PREFIX "" OUTPUT_NAME xcic)
set_property(TARGET xcic PROPERTY C_STANDARD 11)
set_property(TARGET xcic PROPERTY POSITION_INDEPENDENT_CODE ON)
