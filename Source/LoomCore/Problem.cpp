#include "LoomCore/Problem.h"

#include <algorithm>
#include <tuple>

namespace loom {

QString problemLine(const Problem& problem)
{
    return QStringLiteral("%1:%2: %3: %4")
        .arg(problem.file).arg(problem.line)
        .arg(problem.severity == Severity::Error ? QStringLiteral("error") : QStringLiteral("warning"), problem.message);
}

void sortProblems(std::vector<Problem>& problems)
{
    std::stable_sort(problems.begin(), problems.end(), [](const Problem& a, const Problem& b) {
        return std::tuple(a.severity, a.file, a.line) < std::tuple(b.severity, b.file, b.line);
    });
}

} // namespace loom
