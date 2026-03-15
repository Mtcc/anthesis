cmake_minimum_required(VERSION 3.22)
project(Anthesis VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE as submodule at ./JUCE
add_subdirectory(JUCE)

# Shared UI/DSP library (added before plugins so each can link it)
add_subdirectory(shared)

# Plugins
add_subdirectory(Mycelium)
add_subdirectory(Resin)
add_subdirectory(Aureole)
add_subdirectory(Pollen)
add_subdirectory(Corolla)
add_subdirectory(Alluvium)
