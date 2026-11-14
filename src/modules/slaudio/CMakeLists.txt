project(slaudio C)

set(SRCS slaudio.c record.c)

if(STATIC)
  add_library(${PROJECT_NAME} OBJECT ${SRCS})
else()
  add_library(${PROJECT_NAME} MODULE ${SRCS})
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC
  sl_common SL::portaudio SL::samplerate SL::FLAC m)
