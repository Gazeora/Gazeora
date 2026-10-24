# Gazeora's version, from the VERSION file at the root: the one place to change it for a
# release. It reaches the exe's version info, the About card and the MSI through
# PROJECT_VERSION and GAZEORA_VERSION_LABEL. Included before project(), which takes it.

set(GAZEORA_VERSION_FILE ${CMAKE_CURRENT_LIST_DIR}/../VERSION)
file(STRINGS ${GAZEORA_VERSION_FILE} GAZEORA_VERSION_TEXT LIMIT_COUNT 1)
string(STRIP "${GAZEORA_VERSION_TEXT}" GAZEORA_VERSION_TEXT)
# MAJOR.MINOR.PATCH for a release, MAJOR.MINOR.PATCH-rc.N for a release candidate
if(NOT GAZEORA_VERSION_TEXT MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)(-rc\\.[1-9][0-9]*)?$")
    message(FATAL_ERROR
        "VERSION must be like 1.2.0, or 1.2.0-rc.1 for a release candidate; "
        "it says '${GAZEORA_VERSION_TEXT}'")
endif()
# Windows Installer compares only the three numbers, so project() and the MSI get just those
set(GAZEORA_VERSION ${CMAKE_MATCH_1})
# How the version is shown: v1.2.0, or v1.2.0-rc.1
set(GAZEORA_VERSION_LABEL "v${GAZEORA_VERSION_TEXT}")
# Editing VERSION reconfigures on the next build.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GAZEORA_VERSION_FILE})
