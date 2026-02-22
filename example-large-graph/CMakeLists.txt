cmake_minimum_required(VERSION 3.23)

project(example-large-graph VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_SYSTEM_IMGUI "Use system Imgui instead of automatic download" OFF)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} 
  PRIVATE
    example-large-graph.cpp
  PRIVATE FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
    example-large-graph.hpp
  )

# Add ImNodeFlow
set(IMNODEFLOW_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
target_sources(${PROJECT_NAME} 
  PRIVATE
    ${IMNODEFLOW_DIR}/src/ImNodeFlow.cpp   
    ${IMNODEFLOW_DIR}/include/ImNodeFlow.h
    ${IMNODEFLOW_DIR}/src/context_wrapper.h
    ${IMNODEFLOW_DIR}/src/imgui_bezier_math.h
    ${IMNODEFLOW_DIR}/src/imgui_bezier_math.inl
    ${IMNODEFLOW_DIR}/src/imgui_extra_math.h
    ${IMNODEFLOW_DIR}/src/imgui_extra_math.inl
    ${IMNODEFLOW_DIR}/src/imNodeFlow.inl
  )

if(USE_SYSTEM_IMGUI)
  # Make sure you have the Findimgui.cmake scripts
  # available to CMake using correct path
  find_package(imgui)
  # Make sure the target imgui::imgui is setup in your scripts
  target_link_libraries(ImNodeFlow imgui::imgui)
else()
  # Location to download Imgui sources
  include(FetchContent)
  FetchContent_Declare(
      imgui
      GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
      GIT_TAG "v1.91.6"  # Update with future minimum compatibility
      GIT_SHALLOW TRUE  # Limit history to download
  )
  FetchContent_MakeAvailable(imgui)
  target_sources(${PROJECT_NAME} PRIVATE 
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
  target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
endif()

# Common ImNodeFlow definitions
target_include_directories(${PROJECT_NAME} PRIVATE ${IMNODEFLOW_DIR}/include)
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

if(CMAKE_SYSTEM_NAME MATCHES Emscripten)
  include(cmake/emscripten.cmake)
else()
  include(cmake/desktop.cmake)
endif()

# Copy all necessary DLLs to the executable target directory after building
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${PROJECT_NAME}> $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()
