# Generates a Fortran module with plume tag constants from TagDefinitions.def.

if(NOT DEFINED INPUT)
  message(FATAL_ERROR "INPUT is required")
endif()

if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "OUTPUT is required")
endif()

file(READ "${INPUT}" TAG_DEFINITIONS_CONTENT)
string(REPLACE "\r\n" "\n" TAG_DEFINITIONS_CONTENT "${TAG_DEFINITIONS_CONTENT}")
string(REPLACE "\r" "\n" TAG_DEFINITIONS_CONTENT "${TAG_DEFINITIONS_CONTENT}")

string(REGEX MATCHALL "PLUME_TAG\\([ \t]*[A-Z0-9_]+[ \t]*,[ \t]*\"[^\"]+\"[ \t]*\\)" TAG_DEFINITION_LINES "${TAG_DEFINITIONS_CONTENT}")

if(NOT TAG_DEFINITION_LINES)
  message(FATAL_ERROR "No PLUME_TAG(...) definitions found in ${INPUT}")
endif()

set(FORTRAN_CONSTANTS "")
set(TAG_COUNT 0)

foreach(TAG_LINE IN LISTS TAG_DEFINITION_LINES)
  string(REGEX REPLACE "PLUME_TAG\\([ \t]*([A-Z0-9_]+)[ \t]*,[ \t]*\"([^\"]+)\"[ \t]*\\)" "\\1;\\2" TAG_PARTS "${TAG_LINE}")
  list(GET TAG_PARTS 0 TAG_ID)
  list(GET TAG_PARTS 1 TAG_NAME)

  string(APPEND FORTRAN_CONSTANTS "integer(c_int), parameter, public :: PLUME_TAG_${TAG_ID} = ${TAG_COUNT}\n")
  string(APPEND FORTRAN_CONSTANTS "character(len=*), parameter, public :: PLUME_TAG_NAME_${TAG_ID} = \"${TAG_NAME}\"\n")

  math(EXPR TAG_COUNT "${TAG_COUNT} + 1")
endforeach()

set(MODULE_TEXT "! Auto-generated from TagDefinitions.def. Do not edit by hand.\n")
string(APPEND MODULE_TEXT "module plume_tags_module\n")
string(APPEND MODULE_TEXT "\n")
string(APPEND MODULE_TEXT "use iso_c_binding, only : c_int\n")
string(APPEND MODULE_TEXT "\n")
string(APPEND MODULE_TEXT "implicit none\n")
string(APPEND MODULE_TEXT "private\n")
string(APPEND MODULE_TEXT "\n")
string(APPEND MODULE_TEXT "${FORTRAN_CONSTANTS}")
string(APPEND MODULE_TEXT "integer(c_int), parameter, public :: PLUME_TAG_COUNT = ${TAG_COUNT}\n")
string(APPEND MODULE_TEXT "\n")
string(APPEND MODULE_TEXT "end module plume_tags_module\n")

get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(WRITE "${OUTPUT}" "${MODULE_TEXT}")
