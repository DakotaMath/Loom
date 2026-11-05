#pragma once

// The demo project (Tests/Projects/Demo, Specs/m2-statements-review.md, "The demo project") as a
// git repository in a temporary folder, for every M2 test that needs a project: Catch2 and Qt
// Test alike, so it reports failures as text rather than through either framework.

#include "LoomCore/Git.h"

#include <QString>
#include <QTemporaryDir>

#include <memory>

namespace loomtest {

/// A copy of the demo project, committed once, in a folder deleted when the object goes.
class DemoProject {
public:
    /// Makes the copy.
    /// Steps: copies `Tests/Projects/Demo` and the project kit's `.gitignore` and `.gitattributes`
    /// (Templates/project) into a new temporary folder; gives git a test identity of its own (a
    /// global config file in the temporary folder, and no system config); runs `git init -b
    /// main`, installs Loom's hooks (with the `loom` this build made) and commits everything, so
    /// the commit passes the commit guard; then, if `overlay` is given, copies
    /// `Tests/Projects/Demo-edits/<overlay>/` over the working tree, uncommitted.
    /// @param overlay The overlay's name (`review`, `latex-error`), or empty for none.
    /// Side effects: creates the folder and repository; runs git and the commit guard.
    explicit DemoProject(const QString& overlay = {});

    /// Whether every step worked; if not, error() says which failed.
    bool ok() const { return m_error.isEmpty(); }
    /// What went wrong, or an empty string.
    const QString& error() const { return m_error; }
    /// The project folder, absolute, with `/`.
    const QString& root() const { return m_root; }
    /// The git to use in it: the test identity, no system config.
    const loom::GitTool& git() const { return m_git; }

    /// Copies an overlay over the working tree.
    /// @returns An empty string, or what went wrong. Side effects: overwrites files of the copy.
    QString applyOverlay(const QString& overlay);

private:
    std::unique_ptr<QTemporaryDir> m_temp;
    QString m_root;
    loom::GitTool m_git;
    QString m_error;
};

/// Copies a folder's files into another, creating folders as needed and replacing files there.
/// @returns An empty string, or the first file that couldn't be copied. Side effects: writes files.
QString copyFolder(const QString& from, const QString& to);

} // namespace loomtest
