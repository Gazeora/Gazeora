# The MSI installer (WiX 5, from the dotnet tool manifest at the repository root). Build the
# gazeora_msi target: it stages `cmake --install` (the Runtime and Tools components) and packs
# that into Gazeora-<version>-<arch>.msi in the build folder. Never part of the default build.

# Each release is a product of its own, so the two install side by side. Never change an
# upgrade code, or a new MSI stops replacing the installed one. The MSI and Uninstall.exe both
# use these.
if(GAZEORA_ARCH STREQUAL "x64")
    set(GAZEORA_PRODUCT_NAME "Gazeora")
    set(GAZEORA_UPGRADE_CODE "842FB609-35EB-4496-9430-A172F877C6A9")
else()
    set(GAZEORA_PRODUCT_NAME "Gazeora (32-bit)")
    set(GAZEORA_UPGRADE_CODE "E63B0C29-A11E-49E4-AA71-350DE64CEC3F")
endif()

add_subdirectory(custom_actions)
add_subdirectory(uninstaller)

find_program(GAZEORA_DOTNET dotnet)
if(NOT GAZEORA_DOTNET)
    message(STATUS "No .NET SDK found: the gazeora_msi target is off (WiX needs it)")
    return()
endif()

set(GAZEORA_WIX_VERSION 5.0.2)  # keep in step with dotnet-tools.json
set(GAZEORA_WIX_EXTENSIONS UI Util Firewall)
set(GAZEORA_MSI_STAGE ${CMAKE_CURRENT_BINARY_DIR}/stage)
set(GAZEORA_MSI_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(GAZEORA_MSI ${CMAKE_BINARY_DIR}/Gazeora-${PROJECT_VERSION}-${GAZEORA_ARCH}.msi)
set(GAZEORA_WIX "${GAZEORA_DOTNET}" tool run wix --)

set(extension_add)
set(extension_args)
foreach(extension IN LISTS GAZEORA_WIX_EXTENSIONS)
    set(package WixToolset.${extension}.wixext/${GAZEORA_WIX_VERSION})
    list(APPEND extension_add COMMAND ${GAZEORA_WIX} extension add -g ${package})
    list(APPEND extension_args -ext ${package})
endforeach()

set(stage_install ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --config $<CONFIG>)

set(sign_msi)
if(GAZEORA_SIGNTOOL)
    set(sign_msi COMMAND ${CMAKE_COMMAND}
        "-DSIGNTOOL=${GAZEORA_SIGNTOOL}"
        "-DCERT_FILE=${GAZEORA_SIGN_CERT_FILE}"
        "-DCERT_THUMBPRINT=${GAZEORA_SIGN_CERT_THUMBPRINT}"
        "-DTIMESTAMP_URL=${GAZEORA_SIGN_TIMESTAMP_URL}"
        "-DFILE=${GAZEORA_MSI}"
        -P ${PROJECT_SOURCE_DIR}/cmake/SignFile.cmake)
endif()

file(GLOB GAZEORA_WIX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/wix/*.wxs)

add_custom_target(gazeora_msi
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${GAZEORA_MSI_STAGE}
    COMMAND ${stage_install} --component Runtime --prefix ${GAZEORA_MSI_STAGE}/runtime
    COMMAND ${stage_install} --component Tools --prefix ${GAZEORA_MSI_STAGE}/tools
    COMMAND ${CMAKE_COMMAND}
        "-DINPUTS=${PROJECT_SOURCE_DIR}/NOTICE|${PROJECT_SOURCE_DIR}/LICENSE"
        -DOUTPUT=${GAZEORA_MSI_GENERATED}/License.rtf
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TextToRtf.cmake
    COMMAND "${GAZEORA_DOTNET}" tool restore
    ${extension_add}
    COMMAND ${GAZEORA_WIX} build -nologo -arch ${GAZEORA_ARCH} ${extension_args}
        -d ProductVersion=${PROJECT_VERSION}
        "-d" "ProductName=${GAZEORA_PRODUCT_NAME}"
        -d UpgradeCode=${GAZEORA_UPGRADE_CODE}
        "-d" "Publisher=${GAZEORA_PUBLISHER}"
        -bindpath Runtime=${GAZEORA_MSI_STAGE}/runtime
        -bindpath Tools=${GAZEORA_MSI_STAGE}/tools
        -bindpath Actions=$<TARGET_FILE_DIR:gazeora_setup_actions>
        -bindpath Branding=${CMAKE_CURRENT_SOURCE_DIR}/branding
        -bindpath Resources=${PROJECT_SOURCE_DIR}/apps/gazeora/resources
        -bindpath Generated=${GAZEORA_MSI_GENERATED}
        -o ${GAZEORA_MSI}
        ${GAZEORA_WIX_SOURCES}
    ${sign_msi}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}  # where dotnet finds the tool manifest
    COMMENT "Packing ${GAZEORA_MSI}"
    VERBATIM)
add_dependencies(gazeora_msi
    Gazeora GazeoraCLI gazeora_hook gazeora_testapp gazeora_stream_probe gazeora_setup_actions
    gazeora_uninstall)
