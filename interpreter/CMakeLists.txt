# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("interpreter")

# Include sub-projects.
add_subdirectory ("interpreter")
add_subdirectory ("embedder")
add_subdirectory ("mandelbrot")
