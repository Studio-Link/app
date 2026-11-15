# Apply PATCH to the current source dir once (FetchContent PATCH_COMMAND).
# Skips a patch that is already applied, so re-configuring keeps working.
execute_process(COMMAND git apply --reverse --check "${PATCH}"
  RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
if(already EQUAL 0)
  return()
endif()
execute_process(COMMAND git apply "${PATCH}" RESULT_VARIABLE failed)
if(failed)
  message(FATAL_ERROR "could not apply ${PATCH}")
endif()
