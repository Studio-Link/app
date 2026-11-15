project(webapp C)

set(SRCS webapp.c account.c contact.c option.c
  ws_baresip.c ws_contacts.c ws_meter.c ws_calls.c ws_options.c ws_audio.c
  websocket.c utils.c sessions.c)

if(STATIC)
  add_library(${PROJECT_NAME} OBJECT ${SRCS})
else()
  add_library(${PROJECT_NAME} MODULE ${SRCS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/assets
)
target_link_libraries(${PROJECT_NAME} PUBLIC sl_common SL::FLAC m)
