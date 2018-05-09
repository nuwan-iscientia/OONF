# set path to source for apps
SET(APP_DATA_C_IN ${CMAKE_CURRENT_SOURCE_DIR}/src/main/app_data.c.in)
SET(MAIN_C ${CMAKE_CURRENT_SOURCE_DIR}/src/main/main.c)

# add include directoy
include_directories(include)
include_directories(${PROJECT_BINARY_DIR})
