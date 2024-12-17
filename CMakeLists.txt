cmake_minimum_required(VERSION 3.28)
project(Edw590SCR)

set(CMAKE_CXX_STANDARD 26)

# Dummy file just for CLion to understand the project - use VS 2005 to compile it.

add_executable(Edw590SCR WIN32
        main.c
        GeneralUtils.c
        GeneralUtils.h
        unzip.cpp
        unzip.h
)
