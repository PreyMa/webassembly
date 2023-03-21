
add_executable (embedder "main.cpp")
target_link_libraries(embedder interpreter)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET embedder PROPERTY CXX_STANDARD 20)
else()
  set_property(TARGET embedder PROPERTY CXX_STANDARD 17)
endif()
