cmake_minimum_required(VERSION 3.16)

project(Mayya VERSION 1.0 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)           # Use C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # Enforce standard
set(CMAKE_CXX_EXTENSIONS OFF)        # Disable compiler-specific extensions


add_executable(mayya
    src/main.cpp
)

