# set path to source for apps
SET(APP_DATA_C_IN ${CMAKE_CURRENT_LIST_DIR}/src/main/app_data.c.in)
SET(MAIN_C ${CMAKE_CURRENT_LIST_DIR}/src/main/main.c)
SET(VERSION_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR})

# add include directoy
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
include_directories(${PROJECT_BINARY_DIR})
