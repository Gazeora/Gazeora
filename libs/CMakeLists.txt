# The libraries, in dependency order. Each keeps its public headers under
# include/gazeora/<module>/ and its sources under src/, with the same subfolders.
add_subdirectory(common)   # gazeora_common: shared-memory protocol between hook and host
add_subdirectory(hook)     # gazeora_hook_core + gazeora_hook.dll, injected into the game
add_subdirectory(host)     # gazeora_host_lib: launch, capture, encode, stream
add_subdirectory(library)  # gazeora_library: the game library shared by both front ends
add_subdirectory(input)    # gazeora_input: game controllers
