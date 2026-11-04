#pragma once

#include <QString>

#include <vector>

namespace loom {

/// How serious a problem is (Specs/m1-core.md, "Validation"): errors block a commit, warnings don't.
enum class Severity { Error, Warning };

/// One problem Loom found in a project file, as `loom validate` reports it:
/// `<file>:<line>: <severity>: <message>`.
struct Problem {
    Severity severity = Severity::Error;
    /// The stable code from the spec's tables, e.g. `unknown-field`.
    QString code;
    /// The project-relative path of the file, with `/`, e.g. `loom.toml`.
    QString file;
    /// The line of the offending value, else of its table header, else 1.
    int line = 1;
    /// The plain-English message, filled in from the spec's table.
    QString message;
};

/// `<file>:<line>: <severity>: <message>`, as `loom validate` prints a problem. No side effects.
QString problemLine(const Problem& problem);

/// Sorts problems the way `loom validate` prints them: errors first, then warnings, each group
/// in file order, then by line.
/// @param problems The problems to sort, in place. Side effects: reorders `problems`; the order
///                 of problems on the same line is kept.
void sortProblems(std::vector<Problem>& problems);

} // namespace loom
