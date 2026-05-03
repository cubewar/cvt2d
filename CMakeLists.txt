cmake_minimum_required(VERSION 3.15)
project(VTuberFireHead)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ----- Fetch Raylib -----
include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

# ----- Find OpenCV -----
# OpenCV must be installed on the system.
# On Windows, set OpenCV_DIR to the build directory containing OpenCVConfig.cmake
# e.g., C:/opencv/build
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio dnn objdetect)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# ----- Sources -----
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/ControlPanel.cpp")

# ----- Executable -----
add_executable(${PROJECT_NAME} ${SOURCES})

# ----- Includes -----
target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    ${OpenCV_INCLUDE_DIRS}
)

# ----- Linking -----
target_link_libraries(${PROJECT_NAME} PRIVATE 
    raylib
    ${OpenCV_LIBS}
)

# ----- Control Panel Executable -----
add_executable(VTuberControlPanel src/ControlPanel.cpp src/SharedConfig.cpp)
target_link_libraries(VTuberControlPanel PRIVATE raylib)

# ----- Copy OpenCV DLLs to output on Windows -----
if(WIN32)
    # Copy OpenCV DLLs next to the executable after build
    foreach(LIB ${OpenCV_LIBS})
        get_target_property(LIB_LOCATION ${LIB} IMPORTED_LOCATION_RELEASE)
        if(LIB_LOCATION)
            get_filename_component(LIB_DIR ${LIB_LOCATION} DIRECTORY)
            get_filename_component(LIB_NAME ${LIB_LOCATION} NAME_WE)
            set(DLL_PATH "${LIB_DIR}/../bin/${LIB_NAME}.dll")
            if(EXISTS ${DLL_PATH})
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DLL_PATH}
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
                )
            endif()
        endif()
    endforeach()
endif()

# ----- Copy assets to build directory -----
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# ----- Download face detection model if not present -----
set(MODEL_DIR "${CMAKE_SOURCE_DIR}/assets/models")
set(PROTOTXT_FILE "${MODEL_DIR}/deploy.prototxt")
set(CAFFEMODEL_FILE "${MODEL_DIR}/res10_300x300_ssd_iter_140000.caffemodel")

if(NOT EXISTS ${PROTOTXT_FILE})
    message(STATUS "Downloading face detection prototxt...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/opencv/opencv/master/samples/dnn/face_detector/deploy.prototxt"
        ${PROTOTXT_FILE}
        SHOW_PROGRESS
    )
endif()

if(NOT EXISTS ${CAFFEMODEL_FILE})
    message(STATUS "Downloading face detection caffemodel (10MB)...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/opencv/opencv_3rdparty/dnn_samples_face_detector_20170830/res10_300x300_ssd_iter_140000.caffemodel"
        ${CAFFEMODEL_FILE}
        SHOW_PROGRESS
    )
endif()
