# loom: the command line, a thin layer over LoomCore.
add_executable(loom main.cpp)
target_link_libraries(loom PRIVATE LoomCore)
loom_set_compiler_options(loom)
