project(slogging C)

set(SRCS slogging.c)

if(STATIC)
  add_library(${PROJECT_NAME} OBJECT ${SRCS})
else()
  add_library(${PROJECT_NAME} MODULE ${SRCS})
endif()


# embedded CA certificates (slcacert)
target_link_libraries(${PROJECT_NAME} PUBLIC sl_common)
