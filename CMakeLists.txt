cmake_minimum_required(VERSION 3.19)
project(EMCctrl LANGUAGES CXX)

add_compile_definitions(NOMINMAX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(QT_DEBUG_CONSOLE FALSE)

set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${SRC_DIR}
    ${RES_DIR}
)

# =========================
# C++ standard
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# Qt
# =========================
find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport Concurrent Network)
qt_standard_project_setup()

# =========================
# Project structure
# =========================
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# =========================
# Source files (explicit)
# =========================
set(SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/mainwindow_draw.cpp
    ${SRC_DIR}/GpibDevice.cpp
    ${SRC_DIR}/qledindicator.cpp
    ${SRC_DIR}/MediaButtonsWidget.cpp
    #${SRC_DIR}/qcustomplot.cpp
    ${SRC_DIR}/ftdibitbang.cpp
    ${SRC_DIR}/rf_limits.cpp
    ${SRC_DIR}/PowerAmplifier.cpp
    ${SRC_DIR}/EmcMeasurementManager.cpp
    ${SRC_DIR}/mainwindow.ui
)

set(HEADERS
    ${INC_DIR}/mainwindow.h
    ${INC_DIR}/GpibDevice.h
    ${INC_DIR}/qledindicator.h
    ${INC_DIR}/MediaButtonsWidget.h
    #${INC_DIR}/qcustomplot.h
    ${INC_DIR}/ftdibitbang.h
    ${INC_DIR}/rf_limits.h
    ${INC_DIR}/PowerAmplifier.h
    #${INC_DIR}/common.h
    ${INC_DIR}/ftd2xx.h
    ${INC_DIR}/EmcMeasurementManager.h
)

# =========================
# Library
# =========================

add_library(qcustomplot STATIC
    ${LIB_DIR}/qcustomplot/qcustomplot.cpp
    ${LIB_DIR}/qcustomplot/qcustomplot.h
)

target_include_directories(qcustomplot PUBLIC
    ${LIB_DIR}/qcustomplot
)

target_link_libraries(qcustomplot PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::PrintSupport
)

# =========================
# Executable
# =========================
qt_add_executable(EMCctrl WIN32
    ${SOURCES}
    ${HEADERS}
    ${RES_DIR}/resources.qrc
    src/mainwindow_spectrum.cpp
    src/mainwindow_pa.cpp
    src/mainwindow_cs114.cpp
    src/mainwindow_att.cpp
    src/mainwindow_power_supply.cpp
    src/mainwindow_arb_generator.cpp
    ${RES_DIR}/app.rc
    include/StatusBarManager.h
    src/networkautomation.cpp
    include/networkautomation.h
    src/RemoteLogger.cpp
    include/RemoteLogger.h
    include/diag_ipu_159_01.h

)

# =========================
# Include directories
# =========================
target_include_directories(EMCctrl PRIVATE
    ${INC_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# =========================
# Platform-specific setup
# =========================
if(WIN32)

    message(STATUS "Building for Windows")

    # Skryje konzoli na Windows (přepne aplikaci do GUI režimu)
    set_target_properties(EMCctrl PROPERTIES WIN32_EXECUTABLE TRUE)

    # FTDI (D2XX)
    set(FTDI_LIB ${LIB_DIR}/windows/ftd2xx.lib)

    if(NOT EXISTS ${FTDI_LIB})
        message(WARNING "ftd2xx.lib not found!")
    endif()

    target_link_libraries(EMCctrl PRIVATE ${FTDI_LIB})

    # NI GPIB (Windows only)
    target_sources(EMCctrl PRIVATE
        "c:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/lib64/msvc/ni4882.obj"
    )

    target_include_directories(EMCctrl PRIVATE
        "c:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/include/"
    )

elseif(UNIX AND NOT APPLE)

    message(STATUS "Building for Linux")

    # FTDI (D2XX .so)
    set(FTDI_LIB ${LIB_DIR}/linux/libftd2xx.so)

    if(NOT EXISTS ${FTDI_LIB})
        message(FATAL_ERROR "libftd2xx.so not found!")
    endif()

    target_link_libraries(EMCctrl PRIVATE ${FTDI_LIB})

    # Ensure runtime finds .so in same folder
    set_target_properties(EMCctrl PROPERTIES
        BUILD_RPATH "$ORIGIN"
    )

    # Linux GPIB
    find_library(GPIB_LIB gpib)

    if(GPIB_LIB)
        target_link_libraries(EMCctrl PRIVATE ${GPIB_LIB})
    else()
        message(WARNING "libgpib not found – GPIB disabled")
    endif()

endif()

# =========================
# Common Qt libraries
# =========================
target_link_libraries(EMCctrl PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Concurrent
    qcustomplot
)

# =========================
# Install rules
# =========================
install(TARGETS EMCctrl
    RUNTIME DESTINATION bin
)

# Copy FTDI runtime
if(WIN32)
    install(FILES ${LIB_DIR}/windows/ftd2xx.dll DESTINATION bin)
elseif(UNIX)
    install(FILES ${FTDI_LIB} DESTINATION bin)
endif()

# =========================
# Qt deploy (Windows only)
# =========================
if(WIN32)

    qt_generate_deploy_app_script(
        TARGET EMCctrl
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )

    install(SCRIPT ${deploy_script})

endif()
