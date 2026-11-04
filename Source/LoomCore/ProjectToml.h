#pragma once

// `loom.toml`, the file at the top of every project (Specs/m1-core.md, "`loom.toml`").

#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// The `loom.toml` format this Loom reads and writes (Specs/m1-core.md, D7).
inline constexpr qint64 projectFormat = 1;

/// One entry of `[[tags]]` in `loom.toml`.
struct ProjectTag {
    QString id;    ///< Lowercase words joined by `-`, e.g. `energy-estimates`.
    QString kind;  ///< `equation`, `technique` or `other`.
    QString title; ///< Non-empty.
};

/// What `loom.toml` holds, field by field in the spec's order.
struct ProjectConfig {
    qint64 format = projectFormat;
    QString name;
    QString idea;             ///< Optional: empty means absent.
    QDate created;
    QStringList preamble;     ///< `tex.preamble`: project-relative paths, read in order.
    std::vector<ProjectTag> tags;
};

/// The result of reading `loom.toml`.
struct ProjectTomlReading {
    /// The fields that were read correctly; a field with a problem keeps its default.
    ProjectConfig config;
    /// Every problem found, sorted with sortProblems(). The file is `loom.toml`.
    std::vector<Problem> problems;
    /// True if `format` is newer than this Loom's: then `problems` holds only `format-too-new`,
    /// nothing else was read, and every command must stop without writing.
    bool formatTooNew = false;
};

/// Reads and checks `loom.toml` (Specs/m1-core.md, "`loom.toml`" and "Validation", the problems
/// `toml-syntax`, `unknown-field`, `missing-field`, `wrong-type`, `bad-value`, `bad-path`,
/// `format-too-new`, `preamble-missing`, `tag-syntax` and `tag-duplicate`).
/// Rules: `format` is read first, and a format larger than projectFormat stops the reading;
/// unknown fields are errors; `tex.preamble` entries must be project paths (`bad-path`) of `.tex`
/// files (`bad-value`) that exist inside the project (`preamble-missing`); tag IDs match
/// `[a-z0-9]+(-[a-z0-9]+)*` and are unique.
/// @param bytes The content of `loom.toml`.
/// @param root  The project folder, for checking the preamble files exist.
/// @returns The fields and the problems. Reads the preamble files' metadata; writes nothing.
ProjectTomlReading readProjectToml(const QByteArray& bytes, const QString& root);

/// Writes `loom.toml` in the layout of the spec's example: `format`, `name`, `idea` (left out if
/// empty, else a multi-line string), `created`, then `[tex]`, then one `[[tags]]` table per tag.
/// @param config The fields to write.
/// @returns The file's bytes, UTF-8 with LF line endings. No side effects.
QByteArray formatProjectToml(const ProjectConfig& config);

/// How opening a project went.
enum class OpenStatus {
    Opened,       ///< `loom.toml` was read; its problems (if any) are in OpenedProject::problems.
    NotAProject,  ///< There's no `loom.toml` in the folder.
    Unreadable,   ///< `loom.toml` exists but can't be read, or resolves outside the project.
    FormatTooNew, ///< A newer Loom made the project; nothing may be written.
};

/// A project opened with openProject().
struct OpenedProject {
    OpenStatus status = OpenStatus::NotAProject;
    /// The project folder, absolute.
    QString root;
    /// What `loom.toml` holds (meaningful only when status is Opened).
    ProjectConfig config;
    /// The problems in `loom.toml`, sorted with sortProblems().
    std::vector<Problem> problems;
    /// For any status other than Opened: why, in plain English.
    QString message;
};

/// Opens the project in a folder by reading its `loom.toml`.
/// @param folder The project folder: the one holding `loom.toml`.
/// @returns The project, its status and its problems. Reads files; writes nothing.
OpenedProject openProject(const QString& folder);

} // namespace loom
