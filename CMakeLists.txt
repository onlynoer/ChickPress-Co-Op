cmake_minimum_required(VERSION 3.18)
project(as9 CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(../raylib-cpp raylib)

add_library(asio INTERFACE)

target_include_directories(asio INTERFACE ../asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)

include(../assets/includeable.cmake)

add_executable(as9 as9.cpp skybox.cpp logicHandler.cpp component.cpp network.cpp)
target_link_libraries(as9 raylib raylib_cpp raylib::buffered asio)

if(MSVC)
    target_compile_definitions(as9 PRIVATE _WIN32_WINNT=0x0601)
endif()

# FastSignals injects old MSVC auto-link names (v141-*) via pragma comment(lib).
# We link the CMake-built libfastsignals target directly, so suppress those stale defaults.
if(MSVC)
    target_link_options(as9 PRIVATE
        /NODEFAULTLIB:libfastsignalsd-v141-x32.lib
        /NODEFAULTLIB:libfastsignals-v141-x32.lib
        /NODEFAULTLIB:libfastsignalsd-v141-x64.lib
        /NODEFAULTLIB:libfastsignals-v141-x64.lib
        /NODEFAULTLIB:libfastsignalsd-llvm-x32.lib
        /NODEFAULTLIB:libfastsignals-llvm-x32.lib
        /NODEFAULTLIB:libfastsignalsd-llvm-x64.lib
        /NODEFAULTLIB:libfastsignals-llvm-x64.lib
    )
endif()

make_includeable(../assets/shaders/skybox.vs generated/skybox.vs)
make_includeable(../assets/shaders/skybox.fs generated/skybox.fs)
make_includeable(../assets/shaders/cubemap.vs generated/cubemap.vs)
make_includeable(../assets/shaders/cubemap.fs generated/cubemap.fs)

configure_file(./button.glb models/button.glb COPYONLY)
configure_file(./chicken.glb models/chicken.glb COPYONLY)

configure_file(../assets/textures/skybox.png textures/skybox.png COPYONLY)
configure_file(../assets/textures/grass.jpg textures/grass.jpg COPYONLY)

configure_file(./button_press.mp3 audio/button_press.mp3 COPYONLY)
configure_file(./explosion.mp3 audio/explosion.mp3 COPYONLY)