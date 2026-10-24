# cmake -P SignFile.cmake: signs FILE with signtool, at build time (see CodeSigning.cmake).
# Takes SIGNTOOL, FILE, TIMESTAMP_URL, and CERT_FILE or CERT_THUMBPRINT.

if(CERT_FILE)
    set(cert_args /f "${CERT_FILE}")
    if(DEFINED ENV{GAZEORA_SIGN_CERT_PASSWORD})
        list(APPEND cert_args /p "$ENV{GAZEORA_SIGN_CERT_PASSWORD}")
    endif()
else()
    set(cert_args /sha1 "${CERT_THUMBPRINT}")
endif()

execute_process(
    COMMAND "${SIGNTOOL}" sign ${cert_args} /fd SHA256 /tr "${TIMESTAMP_URL}" /td SHA256 "${FILE}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Couldn't sign ${FILE}")
endif()
