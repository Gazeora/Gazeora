# Authenticode signing. Windows shows "Publisher: Unknown" in the UAC prompt (Gazeora asks for
# an administrator to install the USB driver) and in SmartScreen for any unsigned exe; signed,
# it shows the name on the certificate instead. Signing is off until a certificate is given:
#  - GAZEORA_SIGN_CERT_FILE: a .pfx, its password in the GAZEORA_SIGN_CERT_PASSWORD environment
#    variable (read by SignFile.cmake as each file is signed, so it is never written into the
#    build files), or
#  - GAZEORA_SIGN_CERT_THUMBPRINT: the SHA-1 thumbprint of a certificate in the user's store.
# Each exe and DLL passed to gazeora_sign() is signed right after it links.

set(GAZEORA_SIGN_CERT_FILE "" CACHE FILEPATH "Code-signing certificate (.pfx); empty: no signing")
set(GAZEORA_SIGN_CERT_THUMBPRINT "" CACHE STRING
    "Thumbprint of a code-signing certificate in the user's store; empty: no signing")
set(GAZEORA_SIGN_TIMESTAMP_URL "http://timestamp.digicert.com" CACHE STRING
    "RFC 3161 timestamp server, so signatures outlive the certificate")

if(GAZEORA_SIGN_CERT_FILE OR GAZEORA_SIGN_CERT_THUMBPRINT)
    # signtool comes with the Windows SDK, under Windows Kits\10\bin\<version>\x64\.
    file(GLOB GAZEORA_SIGNTOOL_HINTS LIST_DIRECTORIES true
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64")
    list(SORT GAZEORA_SIGNTOOL_HINTS COMPARE NATURAL ORDER DESCENDING)  # newest SDK first
    find_program(GAZEORA_SIGNTOOL signtool HINTS ${GAZEORA_SIGNTOOL_HINTS} REQUIRED)
    message(STATUS "Signing with ${GAZEORA_SIGNTOOL}")
endif()

function(gazeora_sign)
    if(NOT GAZEORA_SIGNTOOL)
        return()
    endif()
    foreach(target IN LISTS ARGN)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DSIGNTOOL=${GAZEORA_SIGNTOOL}"
                    "-DCERT_FILE=${GAZEORA_SIGN_CERT_FILE}"
                    "-DCERT_THUMBPRINT=${GAZEORA_SIGN_CERT_THUMBPRINT}"
                    "-DTIMESTAMP_URL=${GAZEORA_SIGN_TIMESTAMP_URL}"
                    "-DFILE=$<TARGET_FILE:${target}>"
                    -P "${PROJECT_SOURCE_DIR}/cmake/SignFile.cmake"
            VERBATIM)
    endforeach()
endfunction()
