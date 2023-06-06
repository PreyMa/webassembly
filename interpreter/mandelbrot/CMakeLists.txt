
add_executable (mandelbrot "main.cpp" "stb_image_write.h" "stb_image.h")
target_link_libraries(mandelbrot interpreter)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET mandelbrot PROPERTY CXX_STANDARD 20)
else()
  set_property(TARGET mandelbrot PROPERTY CXX_STANDARD 17)
endif()
