cmake_minimum_required(VERSION 3.16)
project(KillTracker VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-D_WINDOWS)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)
endif()

# AseApi is located next to this plugin folder
set(ARKAPI_ROOT "${CMAKE_SOURCE_DIR}/../AseApi")

# Include directories
include_directories(
    ${ARKAPI_ROOT}/version/Core/Public
    ${ARKAPI_ROOT}/version/Core/Public/API
    ${ARKAPI_ROOT}/include
    ${CMAKE_SOURCE_DIR}
)

# Link directories
link_directories(
    ${ARKAPI_ROOT}/out_lib
)

# Source files (header files are included automatically via #include)
set(SOURCES
    KillDeathTracker_SharedData.cpp
    KillDeathTracker_Core.cpp
    KillDeathTracker_KillDeath.cpp
    KillDeathTracker_PlayerSession.cpp
    KillDeathTracker_DamageTracking.cpp
    KillDeathTracker_ResourceTracking.cpp
    KillDeathTracker_BossTracking.cpp
    KillDeathTracker_SpawnTracking.cpp
    KillDeathTracker_APIIntegration.cpp
    KillDeathTracker_ClusterID.cpp
    KillDeathTracker_Main.cpp
)

# Create the DLL
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Link against ArkApi
target_link_libraries(${PROJECT_NAME} 
    ArkApi
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy config files to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "${CMAKE_BINARY_DIR}/bin/config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/PluginInfo.json"
        "${CMAKE_BINARY_DIR}/bin/PluginInfo.json"
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ArkApi/Plugins/KillDeathTracker
    LIBRARY DESTINATION ArkApi/Plugins/KillDeathTracker
)

install(FILES 
    config.json 
    PluginInfo.json
    DESTINATION ArkApi/Plugins/KillDeathTracker
)
