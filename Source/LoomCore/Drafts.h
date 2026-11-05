#pragma once

// Recovery files: the editor's unsaved text, kept under `.loom/drafts/` so a crash loses nothing
// (Specs/m2-statements-review.md, "Never losing text", and "Decisions this spec makes": "LoomCore
// writes them under the project's `.loom/drafts/` (gitignored)").

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <vector>

namespace loom {

/// One recovery file: `.loom/drafts/<path>` holds the text, `.loom/drafts/<path>.toml` what it is.
struct Draft {
    /// The project file the text is for, project-relative with `/` (`Statements/4KX7.tex`); for
    /// the New statement and New proof forms, `new/<form>.txt`.
    QString path;
    /// The editor's text, byte for byte.
    QByteArray text;
    /// fileBase() of the file when the editor loaded it; empty for a new file or a form.
    QString base;
    /// When it was written, with its offset from UTC, to the second.
    QDateTime written;
};

/// Where a draft of `path` lives, project-relative: `.loom/drafts/<path>`. No side effects.
QString draftPath(const QString& path);

/// Writes a recovery file (Specs/m2-statements-review.md, "Never losing text").
/// Rules: the text is written first and the `.toml` second, each atomically, so a crash between
/// them leaves a text without its `.toml`, which listDrafts() ignores; `path` must be a project
/// path (isProjectPath()) outside `.loom/`. The write lock isn't taken: only the app writes
/// drafts, one window per project (`.loom/app.lock`), and `loom` never reads them.
/// @param root  The project folder.
/// @param draft What to write; `written` is written as given.
/// @returns An empty string, or why it couldn't be written. Side effects: writes the two files,
///          creating their folders.
QString writeDraft(const QString& root, const Draft& draft);

/// Removes a recovery file: the `.toml` first, so a text left behind is ignored, then the text,
/// then any folders under `.loom/drafts/` this leaves empty.
/// @returns An empty string, or why a file couldn't be removed. A draft that isn't there is no
///          error. Side effects: deletes files and empty folders.
QString removeDraft(const QString& root, const QString& path);

/// Every recovery file of the project, in path order.
/// Rule: a draft is a `.toml` under `.loom/drafts/` that parses, whose `path` is where it sits
/// (`.loom/drafts/<path>.toml`), with its text beside it. Anything else there is ignored.
/// @returns The drafts. Reads files; writes nothing.
std::vector<Draft> listDrafts(const QString& root);

} // namespace loom
