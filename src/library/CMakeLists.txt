# The user's games, stored as JSON in %APPDATA%\Gazeora\library.json.
include(NlohmannJson)

add_library(gazeora_library STATIC src/game_library.cpp)
target_include_directories(gazeora_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_library
    PUBLIC gazeora_host_lib version shell32 ole32 uuid
    PRIVATE nlohmann_json::nlohmann_json)
