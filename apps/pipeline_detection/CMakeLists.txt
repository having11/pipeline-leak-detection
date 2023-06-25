set(PIPELINE_DETECTION_LINK_LIBRARIES
    libs_base-m7_freertos
    libs_base-m7_http_server
    libs_jpeg_m7
)

add_executable_m7(pipeline_detection
    main.cc
    DATA
    ${CMAKE_CURRENT_SOURCE_DIR}/web/coral_micro_camera.html>/coral_micro_camera.html
    ${PROJECT_SOURCE_DIR}/models/yamnet_spectra_in_edgetpu.tflite>/yamnet_spectra_in_edgetpu.tflite
    M4_EXECUTABLE
    pipeline_detection_m4
)

target_link_libraries(pipeline_detection
    ${PIPELINE_DETECTION_LINK_LIBRARIES}
    libs_nxp_rt1176-sdk_wiced
    libs_base-m7_wifi
)

add_executable_m4(pipeline_detection_m4
    main_m4.cc
)

target_link_libraries(pipeline_detection_m4
    libs_base-m4_freertos
)