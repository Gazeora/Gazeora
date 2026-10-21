# The user-facing programs:
#  - gazeora_library: the game library shared by both front ends
#  - GazeoraCLI.exe:  command line (library management, launch, attach + capture)
#  - Gazeora.exe:     launcher UI
add_subdirectory(library)
add_subdirectory(cli)
add_subdirectory(app)
