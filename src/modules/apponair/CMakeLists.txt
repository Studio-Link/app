project(apponair C)

set(SRCS apponair.c account.c utils.c)

if(STATIC)
  add_library(${PROJECT_NAME} OBJECT ${SRCS})
else()
  add_library(${PROJECT_NAME} MODULE ${SRCS})
endif()

# apponair ships its own webapp.h, it must win over the shared one
target_include_directories(${PROJECT_NAME} BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC sl_common m)
