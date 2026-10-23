# MediaPipe's palm detection and hand landmark models (Apache 2.0), as converted to ONNX by
# the OpenCV model zoo. Downloaded once; HAND_MODEL_FILES lists them.

set(HAND_MODEL_BASE_URL https://huggingface.co/opencv)
set(HAND_MODELS
    "palm_detection_mediapipe/palm_detection_mediapipe_2023feb.onnx|78ff51c38496b7fc8b8ebdb6cc8c1abb02fa6c38427c6848254cdaba57fcce7c"
    "handpose_estimation_mediapipe/handpose_estimation_mediapipe_2023feb.onnx|db0898ae717b76b075d9bf563af315b29562e11f8df5027a1ef07b02bef6d81c")

set(HAND_MODEL_FILES)
foreach(model IN LISTS HAND_MODELS)
    string(REPLACE "|" ";" parts "${model}")
    list(GET parts 0 repo_path)
    list(GET parts 1 sha256)
    get_filename_component(repo ${repo_path} DIRECTORY)
    get_filename_component(name ${repo_path} NAME)
    set(file ${CMAKE_BINARY_DIR}/_deps/hand_models/${name})
    # Skipped when the file is already there with this hash.
    file(DOWNLOAD ${HAND_MODEL_BASE_URL}/${repo}/resolve/main/${name} ${file}
        EXPECTED_HASH SHA256=${sha256} SHOW_PROGRESS)
    list(APPEND HAND_MODEL_FILES ${file})
endforeach()
