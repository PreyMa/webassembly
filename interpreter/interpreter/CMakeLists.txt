﻿# CMakeList.txt : CMake project for interpreter, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_library (interpreter STATIC "interpreter.cpp" "interpreter.h" "decoding.h" "buffer.h" "util.h" "buffer.cpp" "decoding.cpp" "enum.h" "instruction.cpp" "error.h" "error.cpp" "nullable.h" "enum.cpp" "module.h" "forward.h" "module.cpp" "bytecode.h"  "arraylist.h" "host_function.h" "introspection.h" "introspection.cpp" "sealed.h" "virtual_span.h" "host_module.h" "indices.h" "value.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET interpreter PROPERTY CXX_STANDARD 20)
else()
  set_property(TARGET interpreter PROPERTY CXX_STANDARD 17)
endif()

# Generate asm files
# set_target_properties(interpreter PROPERTIES COMPILE_FLAGS "/FAs") 

# TODO: Add tests and install targets if needed.
