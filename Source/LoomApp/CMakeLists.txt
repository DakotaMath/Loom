# LoomAppLib: the UI's code, kept in a library so LoomAppTests can link it.
# LoomApp: the program, a main() around LoomAppLib.
qt_add_library(LoomAppLib STATIC
    AppSettings.cpp
    AppSettings.h
    BottomPanel.cpp
    BottomPanel.h
    Lane.cpp
    Lane.h
    LineIcons.cpp
    LineIcons.h
    MainWindow.cpp
    MainWindow.h
    NewProjectDialog.cpp
    NewProjectDialog.h
    ProjectSession.cpp
    ProjectSession.h
    ProjectSnapshot.cpp
    ProjectSnapshot.h
    SettingsPage.cpp
    SettingsPage.h
    SetupCheckView.cpp
    SetupCheckView.h
    SideBar.cpp
    SideBar.h
    StartScreen.cpp
    StartScreen.h
    Tabs.cpp
    Tabs.h)
target_link_libraries(LoomAppLib PUBLIC LoomCore Qt6::Widgets)
loom_set_compiler_options(LoomAppLib)

qt_add_executable(LoomApp WIN32 main.cpp)
target_link_libraries(LoomApp PRIVATE LoomAppLib)
loom_set_compiler_options(LoomApp)
