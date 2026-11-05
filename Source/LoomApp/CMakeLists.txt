# LoomAppLib: the UI's code, kept in a library so LoomAppTests can link it.
# LoomApp: the program, a main() around LoomAppLib.
qt_add_library(LoomAppLib STATIC
    AppSettings.cpp
    AppSettings.h
    BottomPanel.cpp
    BottomPanel.h
    EditBuffer.cpp
    EditBuffer.h
    FilePage.cpp
    FilePage.h
    FindBar.cpp
    FindBar.h
    HeaderForms.cpp
    HeaderForms.h
    Lane.cpp
    Lane.h
    LatexEditor.cpp
    LatexEditor.h
    LineIcons.cpp
    LineIcons.h
    MainWindow.cpp
    MainWindow.h
    NewItemDialogs.cpp
    NewItemDialogs.h
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
    StatementList.cpp
    StatementList.h
    StatementsPage.cpp
    StatementsPage.h
    StatusMarks.cpp
    StatusMarks.h
    Tabs.cpp
    Tabs.h
    TextDiff.cpp
    TextDiff.h)
target_link_libraries(LoomAppLib PUBLIC LoomCore Qt6::Widgets QScintilla)
loom_set_compiler_options(LoomAppLib)

qt_add_executable(LoomApp WIN32 main.cpp)
target_link_libraries(LoomApp PRIVATE LoomAppLib)
loom_set_compiler_options(LoomApp)
