# LoomAppLib: the UI's code, kept in a library so LoomAppTests can link it.
# LoomApp: the program, a main() around LoomAppLib.
qt_add_library(LoomAppLib STATIC
    MainWindow.cpp
    MainWindow.h)
target_link_libraries(LoomAppLib PUBLIC LoomCore Qt6::Widgets)
loom_set_compiler_options(LoomAppLib)

qt_add_executable(LoomApp WIN32 main.cpp)
target_link_libraries(LoomApp PRIVATE LoomAppLib)
loom_set_compiler_options(LoomApp)
