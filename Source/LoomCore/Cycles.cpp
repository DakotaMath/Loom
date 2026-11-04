#include "LoomCore/Cycles.h"

#include <QHash>
#include <QQueue>
#include <QSet>

#include <algorithm>
#include <functional>

namespace loom {

std::vector<QStringList> findCycles(const QMap<QString, QStringList>& edges)
{
    // Tarjan's strongly connected components. Recursion depth is the length of the longest path,
    // at most the number of statements: fine at M1's thousands.
    QHash<QString, int> index;
    QHash<QString, int> low;
    QSet<QString> onStack;
    QStringList stack;
    int next = 0;
    std::vector<QStringList> components;

    std::function<void(const QString&)> visit = [&](const QString& node) {
        index.insert(node, next);
        low.insert(node, next);
        ++next;
        stack << node;
        onStack.insert(node);
        for (const QString& successor : edges.value(node)) {
            if (successor == node)
                continue;
            if (!index.contains(successor)) {
                visit(successor);
                low[node] = std::min(low[node], low[successor]);
            } else if (onStack.contains(successor)) {
                low[node] = std::min(low[node], index[successor]);
            }
        }
        if (low[node] == index[node]) {
            QStringList component;
            QString member;
            do {
                member = stack.takeLast();
                onStack.remove(member);
                component << member;
            } while (member != node);
            if (component.size() > 1)
                components.push_back(component);
        }
    };
    for (auto it = edges.keyBegin(); it != edges.keyEnd(); ++it) {
        if (!index.contains(*it))
            visit(*it);
    }

    std::vector<QStringList> cycles;
    for (QStringList& component : components) {
        std::sort(component.begin(), component.end());
        const QSet<QString> members(component.begin(), component.end());
        const QString& start = component.front();

        // Breadth first from the start, within the component, smaller IDs first, until a node
        // leads back to the start.
        QHash<QString, QString> cameFrom;
        QQueue<QString> queue;
        queue.enqueue(start);
        QString last;
        while (!queue.isEmpty() && last.isEmpty()) {
            const QString node = queue.dequeue();
            QStringList successors = edges.value(node);
            std::sort(successors.begin(), successors.end());
            for (const QString& successor : successors) {
                if (successor == start && node != start) {
                    last = node;
                    break;
                }
                if (members.contains(successor) && successor != start && !cameFrom.contains(successor)) {
                    cameFrom.insert(successor, node);
                    queue.enqueue(successor);
                }
            }
        }
        QStringList cycle{start};
        for (QString node = last; node != start; node = cameFrom.value(node))
            cycle.insert(1, node);
        cycle << start;
        cycles.push_back(cycle);
    }
    std::sort(cycles.begin(), cycles.end(), [](const QStringList& a, const QStringList& b) { return a.front() < b.front(); });
    return cycles;
}

} // namespace loom
