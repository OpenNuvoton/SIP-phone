cmake_minimum_required(VERSION 3.5)

project(phone_gui LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#Add by CHChen59
set(OE_QMAKE_PATH_EXTERNAL_HOST_BINS /usr/bin)

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt5 COMPONENTS Network REQUIRED)

if(ANDROID)
  add_library(phone_gui SHARED
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
  )
else()
  add_executable(phone_gui
    main.cpp
    dcfb.h
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    dialog_dial.cpp
    dialog_dial.h
    dialog_dial.ui
    resources.qrc
    ipc_baresip.cpp
    ipc_baresip.h
    dialog_incoming.cpp
    dialog_incoming.h
    dialog_incoming.ui
    window_oncall.cpp
    window_oncall.h
    window_oncall.ui
    )
endif()

target_link_libraries(phone_gui PRIVATE Qt5::Widgets Qt5::Network)
