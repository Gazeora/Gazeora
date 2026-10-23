# Ways to control the PC besides its own mouse and keyboard: hand gestures seen by a webcam.
include(OnnxRuntime)
include(HandModels)

add_library(gazeora_input STATIC
    # tracking: MediaPipe's palm detector and hand landmark model
    src/tracking/image_sampler.cpp
    src/tracking/landmark_estimator.cpp
    src/tracking/mediapipe_hand_tracker.cpp
    src/tracking/mediapipe_hand_tracker_factory.cpp
    src/tracking/palm_anchors.cpp
    src/tracking/palm_detector.cpp
    src/tracking/roi_calculator.cpp
    src/tracking/roi_stabilizer.cpp
    # gestures: what the hand's movement means for the pointer
    src/gestures/air_box.cpp
    src/gestures/air_touch_pointer.cpp
    src/gestures/finger_posture.cpp
    src/gestures/finger_reach.cpp
    src/gestures/hand_roles.cpp
    src/gestures/finger_straightness.cpp
    src/gestures/fingertip.cpp
    src/gestures/gesture_states.cpp
    src/gestures/hand_gesture_detector.cpp
    src/gestures/hand_pose_reader.cpp
    src/gestures/landmark_smoother.cpp
    src/gestures/left_hand_signs.cpp
    src/gestures/open_hand_trigger.cpp
    src/gestures/pinch_reader.cpp
    src/gestures/pointer_precision.cpp
    src/gestures/wheel_drag.cpp
    # services: the camera-to-pointer loop
    src/services/frame_rate_meter.cpp
    src/services/hand_gesture_controller.cpp
    src/services/hand_gesture_session.cpp
    # platform: Media Foundation, ONNX Runtime, SendInput
    src/platform/camera_devices.cpp
    src/platform/media_foundation_camera.cpp
    src/platform/media_foundation_camera_catalog.cpp
    src/platform/onnx_model.cpp
    src/platform/onnx_runtime_api.cpp
    src/platform/onnx_runtime_engine.cpp
    src/platform/runtime_files.cpp
    src/platform/send_input_pointer.cpp)
target_include_directories(gazeora_input
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_input
    PUBLIC gazeora_common
    PRIVATE onnxruntime_headers mf mfplat mfreadwrite mfuuid ole32)

# onnxruntime.dll beside the executables and the models under models\, where RuntimeFiles
# looks for them.
add_custom_target(gazeora_input_runtime
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GAZEORA_RUNTIME_DIR}/models
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ONNXRUNTIME_DLL}
        ${GAZEORA_RUNTIME_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HAND_MODEL_FILES}
        ${GAZEORA_RUNTIME_DIR}/models
    VERBATIM)
add_dependencies(gazeora_input gazeora_input_runtime)

# And the same beside the installed executables (cmake/Install.cmake).
include(GNUInstallDirs)
install(FILES ${ONNXRUNTIME_DLL} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
install(FILES ${HAND_MODEL_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/models COMPONENT Runtime)
