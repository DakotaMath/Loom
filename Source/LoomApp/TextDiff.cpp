#include "LoomApp/TextDiff.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPalette>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <algorithm>

namespace loom {

std::vector<DiffLine> diffLines(const QString& before, const QString& after)
{
    const QStringList a = before.split(u'\n');
    const QStringList b = after.split(u'\n');
    const qsizetype n = a.size();
    const qsizetype m = b.size();
    // lcs[i][j]: the longest common subsequence of a[i..] and b[j..].
    std::vector<std::vector<int>> lcs(static_cast<std::size_t>(n + 1), std::vector<int>(static_cast<std::size_t>(m + 1), 0));
    for (qsizetype i = n - 1; i >= 0; --i) {
        for (qsizetype j = m - 1; j >= 0; --j) {
            const auto ui = static_cast<std::size_t>(i);
            const auto uj = static_cast<std::size_t>(j);
            lcs[ui][uj] = a[i] == b[j] ? lcs[ui + 1][uj + 1] + 1 : std::max(lcs[ui + 1][uj], lcs[ui][uj + 1]);
        }
    }
    std::vector<DiffLine> lines;
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < n || j < m) {
        const auto ui = static_cast<std::size_t>(i);
        const auto uj = static_cast<std::size_t>(j);
        if (i < n && j < m && a[i] == b[j]) {
            lines.push_back({DiffLine::Kind::Same, a[i]});
            ++i;
            ++j;
        } else if (i < n && (j >= m || lcs[ui + 1][uj] >= lcs[ui][uj + 1])) {
            lines.push_back({DiffLine::Kind::Removed, a[i]});
            ++i;
        } else {
            lines.push_back({DiffLine::Kind::Added, b[j]});
            ++j;
        }
    }
    return lines;
}

void showCompare(QWidget* parent, const QString& title, const QString& before, const QString& after)
{
    auto* dialog = new QDialog(parent);
    dialog->setObjectName(QStringLiteral("compareDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(title);
    dialog->resize(760, 520);
    auto* note = new QLabel(QStringLiteral("Lines marked − are only in the file on disk; lines marked + are only in your text."), dialog);
    note->setWordWrap(true);
    auto* view = new QTextBrowser(dialog);
    view->setObjectName(QStringLiteral("diff"));
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    const bool dark = dialog->palette().color(QPalette::Window).lightness() < 128;
    QString html = QStringLiteral("<pre style='margin:0'>");
    for (const DiffLine& line : diffLines(before, after)) {
        const QString text = line.text.toHtmlEscaped();
        switch (line.kind) {
        case DiffLine::Kind::Same:
            html += QStringLiteral("  %1\n").arg(text);
            break;
        case DiffLine::Kind::Removed:
            html += QStringLiteral("<span style='background:%1'>− %2</span>\n").arg(dark ? QStringLiteral("#5a2020") : QStringLiteral("#fbe0e0"), text);
            break;
        case DiffLine::Kind::Added:
            html += QStringLiteral("<span style='background:%1'>+ %2</span>\n").arg(dark ? QStringLiteral("#1f4a26") : QStringLiteral("#dcf5dc"), text);
            break;
        }
    }
    html += QStringLiteral("</pre>");
    view->setHtml(html);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(note);
    layout->addWidget(view, 1);
    layout->addWidget(buttons);
    dialog->show();
}

} // namespace loom
