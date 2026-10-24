# Gazeora's version, from the VERSION file at the root: the one place to change it for a
# release. It reaches the exe's version info, the About card and the MSI through
# PROJECT_VERSION. Included before project(), which takes it.

set(GAZEORA_VERSION_FILE ${CMAKE_CURRENT_LIST_DIR}/../VERSION)
file(STRINGS ${GAZEORA_VERSION_FILE} GAZEORA_VERSION LIMIT_COUNT 1)
string(STRIP "${GAZEORA_VERSION}" GAZEORA_VERSION)
# MAJOR.MINOR.PATCH only: Windows Installer compares those three and nothing else.
if(NOT GAZEORA_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR
        "VERSION must be MAJOR.MINOR.PATCH, like 1.2.0; it says '${GAZEORA_VERSION}'")
endif()
# Editing VERSION reconfigures on the next build.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GAZEORA_VERSION_FILE})
