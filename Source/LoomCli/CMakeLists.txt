# loom: the command line, a thin layer over LoomCore.
add_executable(loom
    CheckCommands.cpp
    CheckCommands.h
    Cli.cpp
    Cli.h
    StatementCommands.cpp
    StatementCommands.h
    main.cpp)
target_link_libraries(loom PRIVATE LoomCore)
loom_set_compiler_options(loom)
