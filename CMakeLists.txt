cmake_minimum_required(VERSION 3.28)
project(MyScreensaver C)

set(CMAKE_C_STANDARD 23)

# To include libraries like the CRT statically
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(MinimalScr WIN32
        main.c
        Utils/General.c
        Utils/General.h
)
