cmake_minimum_required(VERSION 3.21)
project(vulkan_ffmpeg LANGUAGES C)

# Find packages using vcpkg-provided names
find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(FFMPEG REQUIRED) # FFmpeg often needs manual component listing if find_package(FFmpeg) fails
# Set up your single source file
add_executable(${PROJECT_NAME} src/main.c)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/SDL3/include")
target_link_libraries(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/SDL3/lib/SDL3.lib"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
    ${FFMPEG_LIBRARIES} # vcpkg's FFmpeg typically provides these variables
)

# Automatically copy necessary DLLs (like SDL3.dll) to the folder with your .exe
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/SDL3/lib/SDL3.dll" 
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)