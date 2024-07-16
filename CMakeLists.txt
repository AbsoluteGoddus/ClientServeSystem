cmake_minimum_required(VERSION 3.28)
project(ClientServerSystem)

set(CMAKE_CXX_STANDARD 26)

include_directories(C:/SFML-2.6.1/include)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

include(FetchContent)
FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.x)
FetchContent_MakeAvailable(SFML)

add_executable(ClientServerSystem main.cpp)
target_link_libraries(ClientServerSystem PRIVATE sfml-graphics sfml-system sfml-network sfml-main sfml-window sfml-audio)
target_compile_features(ClientServerSystem PRIVATE cxx_std_23)