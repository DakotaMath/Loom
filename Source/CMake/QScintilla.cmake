# QScintilla, the editor widget of the Statements tab (Specs/m2-statements-review.md, "Dependencies
# and toolchain"; why: Wiki/decisions/004-qscintilla.md). QScintilla ships only qmake files, so
# this builds its source archive as a static library, `QScintilla`, the way its
# src/qscintilla.pro does, minus what Loom doesn't use:
# - qsciprinter.cpp, so Loom doesn't need Qt Print Support (Loom never prints from the editor);
# - MacPasteboardMime.cpp, which is for macOS only.
# Its headers are SYSTEM, so its warnings stay out of Loom's /W4 and -Wall builds, and its own
# sources compile with warnings off, as its qmake file says (`warn_off`).
include(FetchContent)

FetchContent_Declare(qscintilla
    URL https://www.riverbankcomputing.com/static/Downloads/QScintilla/2.14.1/QScintilla_src-2.14.1.tar.gz
    URL_HASH SHA256=dfe13c6acc9d85dfcba76ccc8061e71a223957a6c02f3c343b30a9d43a4cdd4d)
# The archive has no CMakeLists.txt, so this only downloads and unpacks it.
FetchContent_MakeAvailable(qscintilla)

set(qsci_dir "${qscintilla_SOURCE_DIR}")
# The archive is pinned by its hash, so globbing its folders always gives the same files.
file(GLOB qsci_sources
    "${qsci_dir}/src/*.cpp"
    "${qsci_dir}/scintilla/lexers/*.cpp"
    "${qsci_dir}/scintilla/lexlib/*.cpp"
    "${qsci_dir}/scintilla/src/*.cpp")
list(REMOVE_ITEM qsci_sources
    "${qsci_dir}/src/qsciprinter.cpp"
    "${qsci_dir}/src/MacPasteboardMime.cpp")
# The headers with Q_OBJECT, listed so AUTOMOC runs moc on them: they aren't beside the sources
# that include them.
file(GLOB qsci_headers "${qsci_dir}/src/Qsci/*.h" "${qsci_dir}/src/*.h")
list(REMOVE_ITEM qsci_headers "${qsci_dir}/src/Qsci/qsciprinter.h")

add_library(QScintilla STATIC ${qsci_sources} ${qsci_headers})
set_target_properties(QScintilla PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
# As qscintilla.pro: SCINTILLA_QT and SCI_LEXER select Scintilla's Qt platform and its lexers.
# Without QSCINTILLA_MAKE_DLL or QSCINTILLA_DLL, qsciglobal.h makes a static library.
target_compile_definitions(QScintilla PRIVATE SCINTILLA_QT SCI_LEXER INCLUDE_DEPRECATED_FEATURES)
target_include_directories(QScintilla SYSTEM PUBLIC "${qsci_dir}/src")
target_include_directories(QScintilla PRIVATE
    "${qsci_dir}/scintilla/include"
    "${qsci_dir}/scintilla/lexlib"
    "${qsci_dir}/scintilla/src")
if(MSVC)
    target_compile_options(QScintilla PRIVATE /W0 /utf-8)
else()
    target_compile_options(QScintilla PRIVATE -w)
endif()
target_link_libraries(QScintilla PUBLIC Qt6::Widgets)
