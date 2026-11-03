#include "LoomCore/Problem.h"

#include <algorithm>
#include <tuple>

namespace loom {

void sortProblems(std::vector<Problem>& problems)
{
    std::stable_sort(problems.begin(), problems.end(), [](const Problem& a, const Problem& b) {
        return std::tuple(a.severity, a.file, a.line) < std::tuple(b.severity, b.file, b.line);
    });
}

} // namespace loom
