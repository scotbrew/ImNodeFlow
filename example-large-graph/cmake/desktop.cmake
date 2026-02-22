option(USE_SYSTEM_SDL2 "Use system SDL2 instead of automatic download" OFF)

find_package(OpenGL REQUIRED)

if(USE_SYSTEM_SDL2)
  find_package(SDL2 REQUIRED)
else()
  # Fetch SDL2
  include(FetchContent)
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.0
    GIT_SHALLOW    TRUE
  )
  # Force static to keep it simple
  #set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  #set(SDL_STATIC ON  CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL2)

  set(SDL2_FOUND TRUE)
  set(SDL2_DIR ${SDL2_BINARY_DIR}) 

  # Create the alias so the rest of the script can find 'SDL2::SDL2'
  if(NOT TARGET SDL2::SDL2)
      if(TARGET SDL2-static)
          add_library(SDL2::SDL2 ALIAS SDL2-static)
      elseif(TARGET SDL2)
          add_library(SDL2::SDL2 ALIAS SDL2)
      endif()
  endif()
endif()

if (UNIX)
  if (NOT APPLE)
      find_package(Threads REQUIRED)
      find_package(X11 REQUIRED)
      target_link_libraries(${PROJECT_NAME} PRIVATE
              ${CMAKE_THREAD_LIBS_INIT} ${X11_LIBRARIES} ${CMAKE_DL_LIBS})
  endif()
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::GL SDL2::SDL2)