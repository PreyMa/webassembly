# CMakeList.txt : CMake project for interpreter, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_library (interpreter STATIC "interpreter.cpp" "interpreter.h" "module.h" "buffer.h" "util.h" "buffer.cpp" "module.cpp" "enum.h" "instruction.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET interpreter PROPERTY CXX_STANDARD 20)
else()
  set_property(TARGET interpreter PROPERTY CXX_STANDARD 17)
endif()

# TODO: Add tests and install targets if needed.
