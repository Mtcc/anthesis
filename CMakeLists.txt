cmake_minimum_required(VERSION 3.22)
project(Anthesis VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE as submodule at ./JUCE
add_subdirectory(JUCE)

# Plugins (uncomment as each is built)
add_subdirectory(Resin)
# add_subdirectory(Ferrite)   ← metallic transformer saturation, bonus plugin
# add_subdirectory(Mycelium)
# add_subdirectory(Aureole)
# add_subdirectory(Pollen)
# add_subdirectory(Corolla)
# add_subdirectory(Alluvium)
