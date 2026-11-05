#include "LoomCore/BuildDocument.h"

#include "LoomCore/Bibliography.h"
#include "LoomCore/Cycles.h"
#include "LoomCore/Ids.h"
#include "LoomCore/LatexCommands.h"
#include "LoomCore/ProjectFiles.h"

#include <QFile>
#include <QHash>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace loom {

namespace {

/// Records a problem.
void add(std::vector<Problem>& problems, const char* code, const QString& file, int line, const QString& message,
         Severity severity = Severity::Warning)
{
    problems.push_back({severity, QString::fromLatin1(code), file, line, message});
}

/// The words `\loomheader` gives each mode.
QString modePhrase(BuildMode mode, bool verbose)
{
    QString phrase;
    switch (mode) {
    case BuildMode::Alone: phrase = QStringLiteral("alone"); break;
    case BuildMode::Premises: phrase = QStringLiteral("with its direct premises"); break;
    case BuildMode::Chain: phrase = QStringLiteral("with the full chain"); break;
    }
    return verbose ? phrase + QStringLiteral(", verbose") : phrase;
}

/// Whether an alias can be a LaTeX label: `\label` and `\ref` take it as written, so it must
/// have none of the characters TeX treats specially there. An alias that can't is simply not
/// labelled; validation already reports a `\ref` to an alias (`ref-alias`).
bool labelSafe(const QString& alias)
{
    for (const QChar c : alias) {
        if (c.unicode() < 0x20 || QStringView(u"%#\\&$^~{}").contains(c))
            return false;
    }
    return !alias.isEmpty();
}

/// The labels an entry's aliases give, label-safe ones only.
QStringList safeAliases(const QStringList& aliases)
{
    QStringList safe;
    for (const QString& alias : aliases) {
        if (labelSafe(alias))
            safe << alias;
    }
    return safe;
}

/// A proof's letter number, for letter order; 0 for an ID Loom can't split.
qint64 letterNumber(const QString& proofId)
{
    const std::optional<ProofIdParts> parts = splitProofId(proofId);
    return parts ? proofNumber(parts->letters) : 0;
}

/// One statement or stub in a group, with the proofs shown under it.
struct Entry {
    QString tag;
    bool stub = false;
    QStringList proofs;
};

/// A group of `main.tex`: its `\loomsection` heading and its entries.
struct Group {
    QString heading;
    std::vector<Entry> entries;
};

/// The keys of a project's papers, to read the keys a body `\cite`s as Validation's `cite-alias`
/// reads them (Specs/m1-core.md, A3: keys compare ignoring case; old keys are `aliases`).
struct PaperKeys {
    QSet<QString> keys;                    ///< Every current key (and folder name).
    QHash<QString, QString> keyOfOldKey;   ///< Old key to current key.
    QHash<QString, QString> keyOfFolded;   ///< Case-folded current key to current key.
    QHash<QString, QString> keyOfFoldedOld;///< Case-folded old key to current key.

    /// Indexes the papers. No side effects beyond the index.
    explicit PaperKeys(const ProjectContents& contents)
    {
        for (const PaperFile& file : contents.papers) {
            const QString current = currentKey(file);
            keys.insert(file.folder);
            keys.insert(current);
            if (!keyOfFolded.contains(current.toCaseFolded()))
                keyOfFolded.insert(current.toCaseFolded(), current);
        }
        for (const PaperFile& file : contents.papers) {
            if (!file.reading.parsed)
                continue;
            for (const QString& old : file.reading.document.aliases) {
                if (!keys.contains(old) && !keyOfOldKey.contains(old))
                    keyOfOldKey.insert(old, currentKey(file));
                if (!keyOfFoldedOld.contains(old.toCaseFolded()))
                    keyOfFoldedOld.insert(old.toCaseFolded(), currentKey(file));
            }
        }
    }

    /// A paper's current key: its `key`, else its folder's name.
    static QString currentKey(const PaperFile& file)
    {
        return file.reading.parsed && !file.reading.document.key.isEmpty() ? file.reading.document.key : file.folder;
    }

    /// The current key a cited key names, or the key itself when it names no paper (BibTeX
    /// then reports it, and the build `tex-cite-undefined`).
    QString current(const QString& key) const
    {
        if (keys.contains(key))
            return key;
        if (const QString k = keyOfOldKey.value(key); !k.isEmpty())
            return k;
        if (const QString k = keyOfFolded.value(key.toCaseFolded()); !k.isEmpty())
            return k;
        if (const QString k = keyOfFoldedOld.value(key.toCaseFolded()); !k.isEmpty())
            return k;
        return key;
    }
};

/// Rewrites the keys of `\cite` and `\nocite` in a copy (E3), and notes which keys it cites.
/// @param copy  The copy; edited in place, line by line, so its lines stay where they were.
/// @param cited Gains every current key cited; `*` for `\nocite{*}`.
void rewriteCitations(QByteArray& copy, const PaperKeys& keys, QSet<QString>& cited)
{
    static const QStringList names = {QStringLiteral("cite"), QStringLiteral("nocite")};
    const std::vector<LatexCommand> commands = latexCommands(copy, 1, names);
    if (commands.empty())
        return;
    QList<QByteArray> lines = copy.split('\n');
    // Edits go right to left within a line, so earlier positions stay valid.
    for (auto c = commands.rbegin(); c != commands.rend(); ++c) {
        QString line = QString::fromUtf8(lines[c->line - 1]);
        const QString argument = line.mid(c->argumentStart, c->argumentLength);
        QStringList parts = argument.split(u',');
        bool changed = false;
        for (QString& part : parts) {
            const QString key = part.trimmed();
            if (key.isEmpty())
                continue;
            if (key == u"*") {
                cited.insert(key);
                continue;
            }
            const QString current = keys.current(key);
            cited.insert(current);
            if (current != key) {
                part.replace(part.indexOf(key), key.size(), current);
                changed = true;
            }
        }
        if (changed) {
            line.replace(c->argumentStart, c->argumentLength, parts.join(u','));
            lines[c->line - 1] = line.toUtf8();
        }
    }
    copy = lines.join('\n');
}

/// Orders a group's tags (see composeBuild(): dependency order, ties by tag, each circle as a
/// whole in tag order) and finds its circles.
/// @param tags  The group's tags.
/// @param needs Each tag's dependencies, any of which may be outside the group.
/// @param circles Gains each circle, as findCycles() gives it.
/// @returns The tags in order. No side effects.
QStringList dependencyOrder(const QStringList& tags, const QMap<QString, QStringList>& needs, std::vector<QStringList>& circles)
{
    const QSet<QString> inGroup(tags.begin(), tags.end());
    QMap<QString, QStringList> edges;
    for (const QString& tag : tags) {
        QStringList within;
        for (const QString& need : needs.value(tag)) {
            if (inGroup.contains(need) && need != tag && !within.contains(need))
                within << need;
        }
        edges.insert(tag, within);
    }

    // Strongly connected components (Tarjan), so a circle can be placed as one.
    QHash<QString, int> index, low;
    QSet<QString> onStack;
    QStringList stack;
    QHash<QString, int> component;
    std::vector<QStringList> components;
    int counter = 0;
    std::function<void(const QString&)> visit = [&](const QString& v) {
        index.insert(v, counter);
        low.insert(v, counter);
        ++counter;
        stack << v;
        onStack.insert(v);
        for (const QString& w : edges.value(v)) {
            if (!index.contains(w)) {
                visit(w);
                low[v] = std::min(low[v], low[w]);
            } else if (onStack.contains(w)) {
                low[v] = std::min(low[v], index[w]);
            }
        }
        if (low[v] == index[v]) {
            QStringList members;
            QString w;
            do {
                w = stack.takeLast();
                onStack.remove(w);
                component.insert(w, static_cast<int>(components.size()));
                members << w;
            } while (w != v);
            std::sort(members.begin(), members.end());
            components.push_back(members);
        }
    };
    QStringList sorted = tags;
    std::sort(sorted.begin(), sorted.end());
    for (const QString& tag : sorted) {
        if (!index.contains(tag))
            visit(tag);
    }

    // Kahn's algorithm over the components, the one with the smallest tag first.
    const int n = static_cast<int>(components.size());
    std::vector<std::set<int>> after(n);
    std::vector<int> waitingFor(n, 0);
    for (const QString& tag : tags) {
        for (const QString& need : edges.value(tag)) {
            const int from = component.value(need);
            const int to = component.value(tag);
            if (from != to && after[from].insert(to).second)
                ++waitingFor[to];
        }
    }
    std::set<std::pair<QString, int>> ready;
    for (int i = 0; i < n; ++i) {
        if (waitingFor[i] == 0)
            ready.insert({components[i].first(), i});
    }
    QStringList order;
    while (!ready.empty()) {
        const int i = ready.begin()->second;
        ready.erase(ready.begin());
        order << components[i];
        for (const int next : after[i]) {
            if (--waitingFor[next] == 0)
                ready.insert({components[next].first(), next});
        }
    }

    for (const QStringList& circle : findCycles(edges))
        circles.push_back(circle);
    return order;
}

/// The heading text of a stub: its paper's key and the locator of the version it's cited from
/// (`SmithJones24, Theorem 1.2`); without one there, the locator it was extracted from, with
/// that version (`SmithJones24, arxiv-v1: Lemma 2.1`); else the key alone.
QString stubCitation(const ProjectContents& contents, const StubFile& stub)
{
    const PaperFile* paper = contents.paper(stub.paperFolder);
    const QString key = paper ? PaperKeys::currentKey(*paper) : stub.paperFolder;
    const Stub& s = stub.reading.document;
    if (paper && paper->reading.parsed) {
        if (const Locator* at = s.locator(paper->reading.document.cite); at && !at->at.isEmpty())
            return key + QStringLiteral(", ") + at->at;
    }
    if (const Locator* at = s.locator(s.extractedFrom); at && !at->at.isEmpty())
        return QStringLiteral("%1, %2: %3").arg(key, s.extractedFrom, at->at);
    return key;
}

/// Writes `main.tex` line by line, noting what each line belongs to.
struct MainWriter {
    QStringList lines;
    std::vector<GeneratedPart> owners;

    /// Adds one line (without its line break), belonging to `owner`.
    void line(const QString& text, const GeneratedPart& owner)
    {
        lines << text;
        owners.push_back(owner);
    }
};

} // namespace

QString buildModeName(BuildMode mode)
{
    switch (mode) {
    case BuildMode::Alone: return QStringLiteral("alone");
    case BuildMode::Premises: return QStringLiteral("premises");
    case BuildMode::Chain: return QStringLiteral("chain");
    }
    return {};
}

std::optional<BuildMode> buildModeFromName(const QString& name)
{
    for (const BuildMode mode : {BuildMode::Alone, BuildMode::Premises, BuildMode::Chain}) {
        if (buildModeName(mode) == name)
            return mode;
    }
    return std::nullopt;
}

bool BuildDocument::unreadable() const
{
    return std::any_of(problems.begin(), problems.end(), [](const Problem& p) { return p.code == u"build-unreadable"; });
}

QByteArray lineCopy(const QByteArray& body, int bodyLine)
{
    return QByteArray("%\n").repeated(std::max(0, bodyLine - 1)) + body;
}

QString escapeLatexText(const QString& text, bool keepMath)
{
    QString out;
    bool math = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (keepMath && c == u'$') {
            // A `$` opens math only if another closes it; a lone one is printed.
            if (math || text.indexOf(u'$', i + 1) >= 0) {
                math = !math;
                out += c;
                continue;
            }
        }
        if (math) {
            out += c;
            continue;
        }
        switch (c.unicode()) {
        case u'\\': out += QStringLiteral("\\textbackslash{}"); break;
        case u'{': out += QStringLiteral("\\{"); break;
        case u'}': out += QStringLiteral("\\}"); break;
        case u'%': out += QStringLiteral("\\%"); break;
        case u'#': out += QStringLiteral("\\#"); break;
        case u'&': out += QStringLiteral("\\&"); break;
        case u'_': out += QStringLiteral("\\_"); break;
        case u'$': out += QStringLiteral("\\$"); break;
        case u'^': out += QStringLiteral("\\textasciicircum{}"); break;
        case u'~': out += QStringLiteral("\\textasciitilde{}"); break;
        // In text, the default OT1 fonts print these as other characters (¡, ¿, —).
        case u'<': out += QStringLiteral("\\textless{}"); break;
        case u'>': out += QStringLiteral("\\textgreater{}"); break;
        case u'|': out += QStringLiteral("\\textbar{}"); break;
        default: out += c.unicode() < 0x20 ? QChar(u' ') : c;
        }
    }
    return out;
}

QByteArray loomStyle()
{
    QFile file(QStringLiteral(":/tex/loom.sty"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

namespace {

/// composeBuild() and composePreview(): a preview is the statement alone, without its proofs, the
/// header line or page numbers, and with the titles left out of `\loomoutside` (which doesn't print
/// them), so that its inputs change only when what it shows can.
BuildDocument compose(const BuildSource& source, const QString& typedId, BuildMode mode, bool verbose, bool preview)
{
    const ProjectContents& contents = source.contents;
    const ProjectGraph& graph = source.graph;
    BuildDocument doc;
    doc.mode = mode;
    doc.verbose = verbose;
    doc.preview = preview;

    // --- The target ------------------------------------------------------------------------------
    QString proofTarget;
    if (const std::optional<QString> id = resolveProof(contents, typedId)) {
        proofTarget = *id;
        doc.statement = contents.proof(*id)->folder;
        doc.target = *id;
        doc.targetFile = contents.proof(*id)->path;
    } else if (const std::optional<QString> tag = resolveStatement(contents, typedId)) {
        if (const StubFile* stub = contents.stub(*tag)) {
            const PaperFile* paper = contents.paper(stub->paperFolder);
            doc.refusal = QStringLiteral("build-stub");
            doc.refusalMessage = QStringLiteral("%1 is a result from %2; it's built only as part of a statement's chain.")
                                     .arg(*tag, paper ? PaperKeys::currentKey(*paper) : stub->paperFolder);
            return doc;
        }
        doc.statement = *tag;
        doc.target = *tag;
        doc.targetFile = contents.statement(*tag)->path;
    } else {
        doc.refusal = QStringLiteral("ref-unknown");
        doc.refusalMessage = QStringLiteral("`%1` isn't a statement or proof in this project.").arg(typedId);
        return doc;
    }

    // --- What the build reads ----------------------------------------------------------------------
    // A statement or stub named in a list, by tag or (wrongly, but readably) by alias; empty if
    // the project has nothing by that name.
    const auto lookup = [&](const QString& name) -> QString {
        if (contents.statement(name) || contents.stub(name))
            return name;
        return contents.tagWithAlias(name);
    };
    const auto unreadable = [&](const QString& path) {
        add(doc.problems, "build-unreadable", path, 1,
            QStringLiteral("%1 can't be built: Loom can't read its front matter (`loom validate` says why).").arg(path),
            Severity::Error);
    };
    // Each statement's tag must have a readable file before its body or fields are used.
    const auto readable = [&](const QString& tag) {
        if (const StatementFile* file = contents.statement(tag); file && !file->reading.parsed) {
            unreadable(file->path);
            return false;
        }
        if (const StubFile* file = contents.stub(tag); file && !file->reading.parsed) {
            unreadable(file->path);
            return false;
        }
        return true;
    };
    const auto proofOf = [&](const QString& id) -> const Proof* {
        const ProofFile* file = contents.proof(id);
        if (!file)
            return nullptr;
        if (!file->reading.parsed) {
            unreadable(file->path);
            return nullptr;
        }
        return &file->reading.document;
    };

    if (!contents.statement(doc.statement)) {
        doc.refusal = QStringLiteral("ref-unknown");
        doc.refusalMessage = QStringLiteral("`%1` proves %2, which has no statement file.").arg(doc.target, doc.statement);
        return doc;
    }
    if (!readable(doc.statement))
        return doc;
    const Statement& target = contents.statement(doc.statement)->reading.document;

    QStringList shown;
    if (preview) {
        // The statement only.
    } else if (!proofTarget.isEmpty()) {
        shown << proofTarget;
    } else {
        std::vector<const ProofFile*> ofTarget;
        for (const ProofFile& file : contents.proofs) {
            if (file.folder == doc.statement)
                ofTarget.push_back(&file);
        }
        std::stable_sort(ofTarget.begin(), ofTarget.end(), [](const ProofFile* a, const ProofFile* b) {
            const bool aFailed = a->reading.document.level == u"failed";
            const bool bFailed = b->reading.document.level == u"failed";
            if (aFailed != bFailed)
                return bFailed;
            return letterNumber(a->id) < letterNumber(b->id);
        });
        for (const ProofFile* file : ofTarget)
            shown << file->id;
    }
    for (const QString& id : shown)
        proofOf(id);

    // Which proofs each statement of the build shows: the groups below fill it.
    QHash<QString, QStringList> proofsOf;
    proofsOf.insert(doc.statement, shown);
    QSet<QString> placed = {doc.statement};
    std::vector<Group> groups;

    const auto premisesOf = [&](const QStringList& proofs) {
        QStringList premises;
        for (const QString& id : proofs) {
            if (const ProofFile* file = contents.proof(id); file && file->reading.parsed) {
                for (const QString& premise : file->reading.document.premises) {
                    const QString tag = lookup(premise);
                    if (!tag.isEmpty() && !premises.contains(tag))
                        premises << tag;
                }
            }
        }
        return premises;
    };
    const auto statedUsingOf = [&](const QString& tag) {
        QStringList used;
        QStringList listed;
        if (const StatementFile* file = contents.statement(tag); file && file->reading.parsed)
            listed = file->reading.document.statedUsing;
        else if (const StubFile* stub = contents.stub(tag); stub && stub->reading.parsed)
            listed = stub->reading.document.statedUsing;
        for (const QString& name : listed) {
            const QString t = lookup(name);
            if (!t.isEmpty() && !used.contains(t))
                used << t;
        }
        return used;
    };
    // A group of statements without proofs, of those not placed yet.
    const auto plainGroup = [&](const QString& heading, const QStringList& tags) {
        Group group{heading, {}};
        for (const QString& tag : tags) {
            if (placed.contains(tag) || !readable(tag))
                continue;
            placed.insert(tag);
            group.entries.push_back({tag, contents.stub(tag) != nullptr, {}});
        }
        if (!group.entries.empty())
            groups.push_back(group);
    };

    if (mode == BuildMode::Premises) {
        plainGroup(QStringLiteral("Stated using"), statedUsingOf(doc.statement));
        plainGroup(QStringLiteral("Premises"), premisesOf(shown));
    } else if (mode == BuildMode::Chain) {
        Group chain{QStringLiteral("The chain"), {}};
        QStringList queue = statedUsingOf(doc.statement) + premisesOf(shown);
        while (!queue.isEmpty()) {
            const QString tag = queue.takeFirst();
            if (placed.contains(tag) || !readable(tag))
                continue;
            placed.insert(tag);
            if (contents.stub(tag)) {
                // A stub ends the chain: its `paper_premises` are the paper's business. What it
                // is stated using is still needed to read it.
                chain.entries.push_back({tag, true, {}});
                queue += statedUsingOf(tag);
                continue;
            }
            QStringList proofs;
            if (const StatementNode* node = graph.statement(tag); node && !node->decidingProof.isEmpty())
                proofs << node->decidingProof;
            for (const QString& id : proofs)
                proofOf(id);
            proofsOf.insert(tag, proofs);
            chain.entries.push_back({tag, false, proofs});
            queue += statedUsingOf(tag);
            queue += premisesOf(proofs);
        }
        if (!chain.entries.empty())
            groups.push_back(chain);
    }
    groups.push_back({QStringLiteral("The statement"), {{doc.statement, false, shown}}});

    if (doc.unreadable())
        return doc;

    // --- Order within each group -----------------------------------------------------------------
    for (Group& group : groups) {
        QStringList tags;
        QMap<QString, QStringList> needs;
        QHash<QString, Entry> byTag;
        for (const Entry& entry : group.entries) {
            tags << entry.tag;
            needs.insert(entry.tag, statedUsingOf(entry.tag) + premisesOf(entry.proofs));
            byTag.insert(entry.tag, entry);
        }
        std::vector<QStringList> circles;
        const QStringList order = dependencyOrder(tags, needs, circles);
        group.entries.clear();
        for (const QString& tag : order)
            group.entries.push_back(byTag.value(tag));
        for (const QStringList& circle : circles)
            add(doc.problems, "build-cycle", doc.targetFile, 1,
                QStringLiteral("Premises go round in a circle (%1); the build lists them in tag order.")
                    .arg(circle.join(QStringLiteral(" \u2192 "))));
    }

    // --- The copies ------------------------------------------------------------------------------
    const PaperKeys keys(contents);
    QSet<QString> cited;
    // Bodies' `\paperref`s, each with where it is, for the warnings.
    struct PaperrefUse {
        QString name;
        QString file;
        int line;
    };
    std::vector<PaperrefUse> uses;
    std::vector<BuildFile> copies;
    const auto copyBody = [&](const QString& path, const QByteArray& body, int bodyLine) {
        QByteArray copy = lineCopy(body, bodyLine);
        rewriteCitations(copy, keys, cited);
        for (const LatexCommand& c : latexCommands(copy, 1, {QStringLiteral("paperref")}))
            uses.push_back({c.argument, path, c.line});
        copies.push_back({QStringLiteral("src/") + path, copy});
    };

    // A verbose version replaces a proof's body when asked for and present.
    QHash<QString, const VerboseFile*> verboseOf;
    for (const VerboseFile& file : contents.verbose)
        verboseOf.insert(file.proofId, &file);
    QHash<QString, QString> proofVariant; // `verbose` or `stale`, for loomproof's optional argument.

    for (const Group& group : groups) {
        for (const Entry& entry : group.entries) {
            doc.statements << entry.tag;
            if (!entry.stub) {
                const StatementFile* file = contents.statement(entry.tag);
                copyBody(file->path, file->reading.document.body, file->reading.bodyLine);
            }
            for (const QString& id : entry.proofs) {
                doc.proofs << id;
                const ProofFile* file = contents.proof(id);
                const VerboseFile* verboseFile = verbose ? verboseOf.value(id) : nullptr;
                if (!verboseFile) {
                    copyBody(file->path, file->reading.document.body, file->reading.bodyLine);
                    continue;
                }
                if (!verboseFile->reading.parsed) {
                    unreadable(verboseFile->path);
                    continue;
                }
                copyBody(verboseFile->path, verboseFile->reading.document.body, verboseFile->reading.bodyLine);
                // Stale, as M1's `verbose-stale`: its fingerprint isn't the proof's current one.
                const ProofNode* node = graph.proof(id);
                const QString current = node ? node->checking.fingerprint : QString();
                if (!current.isEmpty() && verboseFile->reading.document.fingerprint != current) {
                    proofVariant.insert(id, QStringLiteral("stale"));
                    add(doc.problems, "verbose-stale", verboseFile->path,
                        verboseFile->reading.lines.of(QStringLiteral("fingerprint")),
                        QStringLiteral("The verbose version of %1 was written from an older version of the proof.").arg(id));
                } else {
                    proofVariant.insert(id, QStringLiteral("verbose"));
                }
            }
        }
    }
    if (doc.unreadable()) {
        doc.statements.clear();
        doc.proofs.clear();
        return doc;
    }

    // --- `\paperref` ---------------------------------------------------------------------------------
    // A stub's tag from a name a body used: its tag or one of its aliases.
    const auto stubNamed = [&](const QString& name) -> const StubFile* {
        if (const StubFile* stub = contents.stub(name))
            return stub;
        const QString tag = contents.tagWithAlias(name);
        return tag.isEmpty() ? nullptr : contents.stub(tag);
    };
    struct PaperrefLine {
        QString key;
        QString locator;
    };
    std::map<QString, PaperrefLine> paperrefTable; // By stub tag.
    for (const PaperrefUse& use : uses) {
        const StubFile* stub = stubNamed(use.name);
        if (!stub || !stub->reading.parsed) {
            add(doc.problems, "paperref-unknown", use.file, use.line,
                QStringLiteral("`\\paperref{%1}` doesn't name a paper's result.").arg(use.name));
            continue;
        }
        const PaperFile* paper = contents.paper(stub->paperFolder);
        const QString key = paper ? PaperKeys::currentKey(*paper) : stub->paperFolder;
        const QString version = paper && paper->reading.parsed ? paper->reading.document.cite : QString();
        const Locator* locator = stub->reading.document.locator(version);
        cited.insert(key);
        if (!locator || locator->at.isEmpty()) {
            add(doc.problems, "paperref-no-locator", use.file, use.line,
                QStringLiteral("%1's cited version `%2` has no locator for %3, so the citation has no theorem number.")
                    .arg(key, version, stub->tag));
        } else if (!locator->checkedAgainstPdf.isValid()) {
            add(doc.problems, "paperref-unchecked", use.file, use.line,
                QStringLiteral("The locator \"%1\" of %2 in %3's `%4` isn't checked against its PDF yet.")
                    .arg(locator->at, stub->tag, key, version));
        }
        paperrefTable[stub->tag] = {key, locator ? locator->at : QString()};
    }
    const bool hasBibliography = !cited.isEmpty();

    // --- `main.tex` --------------------------------------------------------------------------------
    MainWriter main;
    const GeneratedPart whole{doc.target, doc.targetFile};
    main.line(QStringLiteral("% Written by Loom for %1, %2%3. Don't edit: Loom rewrites it on every build.")
                  .arg(doc.target, preview ? QStringLiteral("a preview") : QStringLiteral("mode ") + buildModeName(mode),
                       verbose ? QStringLiteral(", verbose") : QString()),
              whole);
    main.line(QStringLiteral("\\documentclass[11pt]{article}"), whole);
    std::vector<BuildFile> preambles;
    for (const QString& path : source.config.preamble) {
        const std::optional<QByteArray> bytes = readProjectFile(source.root, path);
        if (!bytes) {
            add(doc.problems, "build-unreadable", QStringLiteral("loom.toml"), 1,
                QStringLiteral("%1 can't be built: Loom can't read the preamble file %2 that loom.toml lists.")
                    .arg(doc.target, path),
                Severity::Error);
            continue;
        }
        preambles.push_back({QStringLiteral("src/") + path, *bytes});
        main.line(QStringLiteral("\\input{src/%1}").arg(path), whole);
    }
    if (doc.unreadable()) {
        doc.statements.clear();
        doc.proofs.clear();
        return doc;
    }
    main.line(QStringLiteral("\\usepackage{loom}"), whole);

    // Every name of each stub cited, so old names work too.
    for (const auto& [tag, entry] : paperrefTable) {
        const StubFile* stub = contents.stub(tag);
        const GeneratedPart part{tag, stub->path};
        for (const QString& name : QStringList{tag} + safeAliases(stub->reading.document.aliases))
            main.line(QStringLiteral("\\loompaperref{%1}{%2}{%3}").arg(name, entry.key, escapeLatexText(entry.locator)), part);
    }

    // Labels for everything of the project the build doesn't show.
    QSet<QString> inBuild(doc.statements.begin(), doc.statements.end());
    const QSet<QString> proofsInBuild(doc.proofs.begin(), doc.proofs.end());
    struct Owner {
        QString tag;
        QString path;
        QString title;
        QStringList aliases;
    };
    std::vector<Owner> owners;
    for (const StatementFile& file : contents.statements)
        owners.push_back({file.tag, file.path, file.reading.parsed ? file.reading.document.title : QString(),
                          file.reading.parsed ? file.reading.document.aliases : QStringList()});
    for (const StubFile& file : contents.stubs)
        owners.push_back({file.tag, file.path, file.reading.parsed ? stubCitation(contents, file) : QString(),
                          file.reading.parsed ? file.reading.document.aliases : QStringList()});
    std::sort(owners.begin(), owners.end(), [](const Owner& a, const Owner& b) { return a.tag < b.tag; });
    std::vector<const ProofFile*> proofsByLetter;
    for (const ProofFile& file : contents.proofs)
        proofsByLetter.push_back(&file);
    std::stable_sort(proofsByLetter.begin(), proofsByLetter.end(), [](const ProofFile* a, const ProofFile* b) {
        if (a->folder != b->folder)
            return a->folder < b->folder;
        return letterNumber(a->id) < letterNumber(b->id);
    });
    QSet<QString> labelled;
    const auto outside = [&](const QString& label, const QString& shownAs, const QString& title, const GeneratedPart& part) {
        if (labelled.contains(label))
            return;
        labelled.insert(label);
        // The title isn't printed, but it is read: escaped whole, so a title elsewhere in the
        // project can't break this build.
        const QString text = preview ? QString() : escapeLatexText(title, false);
        main.line(shownAs == label ? QStringLiteral("\\loomoutside{%1}{%2}").arg(label, text)
                                   : QStringLiteral("\\loomoutside[%1]{%2}{%3}").arg(shownAs, label, text),
                  part);
    };
    for (const Owner& owner : owners) {
        const GeneratedPart part{owner.tag, owner.path};
        if (!inBuild.contains(owner.tag)) {
            outside(owner.tag, owner.tag, owner.title, part);
            for (const QString& alias : safeAliases(owner.aliases))
                outside(alias, owner.tag, owner.title, part);
        }
        for (const ProofFile* file : proofsByLetter) {
            if (file->folder == owner.tag && !proofsInBuild.contains(file->id))
                outside(file->id, file->id, QStringLiteral("Proof %1 of %2").arg(file->id, owner.tag), {file->id, file->path});
        }
    }
    for (const ProofFile* file : proofsByLetter) { // Proofs in a folder no statement has.
        if (!proofsInBuild.contains(file->id))
            outside(file->id, file->id, QStringLiteral("Proof %1").arg(file->id), {file->id, file->path});
    }

    main.line(QStringLiteral("\\begin{document}"), whole);
    if (preview)
        main.line(QStringLiteral("\\pagestyle{empty}"), whole);
    else
        main.line(QStringLiteral("\\loomheader{%1 \u00b7 %2}{%3}").arg(doc.target, target.title, modePhrase(mode, verbose)), whole);

    for (const Group& group : groups) {
        main.line(QString(), whole);
        if (groups.size() > 1)
            main.line(QStringLiteral("\\loomsection{%1}").arg(group.heading), whole);
        for (const Entry& entry : group.entries) {
            if (entry.stub) {
                const StubFile* stub = contents.stub(entry.tag);
                const Stub& s = stub->reading.document;
                const GeneratedPart part{entry.tag, stub->path};
                const QStringList aliases = safeAliases(s.aliases);
                main.line(QStringLiteral("\\begin{loomstatement}%1{%2}{%3}{%4}")
                              .arg(aliases.isEmpty() ? QString() : u'[' + aliases.join(u',') + u']', s.kind, entry.tag,
                                   escapeLatexText(stubCitation(contents, *stub))),
                          part);
                main.line(escapeLatexText(s.gist), part);
                main.line(QStringLiteral("\\end{loomstatement}"), part);
                continue;
            }
            const StatementFile* file = contents.statement(entry.tag);
            const Statement& s = file->reading.document;
            const GeneratedPart part{entry.tag, file->path};
            const QStringList aliases = safeAliases(s.aliases);
            main.line(QStringLiteral("\\begin{loomstatement}%1{%2}{%3}{%4}")
                          .arg(aliases.isEmpty() ? QString() : u'[' + aliases.join(u',') + u']', s.kind, entry.tag, s.title),
                      part);
            main.line(QStringLiteral("\\input{src/%1}").arg(file->path), part);
            main.line(QStringLiteral("\\end{loomstatement}"), part);
            for (const QString& id : entry.proofs) {
                const ProofFile* proofFile = contents.proof(id);
                const Proof& p = proofFile->reading.document;
                const QString variant = proofVariant.value(id);
                const VerboseFile* verboseFile = variant.isEmpty() ? nullptr : verboseOf.value(id);
                const GeneratedPart proofPart{id, proofFile->path};
                main.line(QStringLiteral("\\begin{loomproof}%1{%2}{%3}{%4}")
                              .arg(variant.isEmpty() ? QString() : u'[' + variant + u']', id, p.level,
                                   escapeLatexText(p.premises.join(QStringLiteral(", ")), false)),
                          proofPart);
                main.line(QStringLiteral("\\input{src/%1}").arg(verboseFile ? verboseFile->path : proofFile->path), proofPart);
                main.line(QStringLiteral("\\end{loomproof}"), proofPart);
            }
        }
    }
    if (hasBibliography) {
        main.line(QString(), whole);
        main.line(QStringLiteral("\\bibliographystyle{amsplain}"), whole);
        main.line(QStringLiteral("\\bibliography{refs}"), whole);
    }
    main.line(QStringLiteral("\\end{document}"), whole);

    doc.files.push_back({QStringLiteral("main.tex"), (main.lines.join(u'\n') + u'\n').toUtf8()});
    doc.mainLines = main.owners;
    for (BuildFile& file : preambles)
        doc.files.push_back(std::move(file));
    for (BuildFile& file : copies)
        doc.files.push_back(std::move(file));

    // --- `refs.bib` ------------------------------------------------------------------------------------
    if (hasBibliography) {
        const bool all = cited.contains(QStringLiteral("*"));
        const Bibliography bib = projectBibliography(contents, [&](const PaperFile& paper) {
            return all || cited.contains(PaperKeys::currentKey(paper));
        });
        doc.files.push_back({QStringLiteral("refs.bib"), bib.text});
        for (Problem problem : bib.problems) {
            // LaTeX still builds without the entry; the citation shows as undefined.
            problem.severity = Severity::Warning;
            doc.problems.push_back(problem);
        }
    }
    sortProblems(doc.problems);
    return doc;
}

} // namespace

BuildDocument composeBuild(const BuildSource& source, const QString& typedId, BuildMode mode, bool verbose)
{
    return compose(source, typedId, mode, verbose, false);
}

BuildDocument composePreview(const BuildSource& source, const QString& tag)
{
    if (!source.contents.statement(tag)) {
        BuildDocument doc;
        doc.preview = true;
        if (source.contents.stub(tag)) {
            doc.refusal = QStringLiteral("build-stub");
            doc.refusalMessage = QStringLiteral("%1 is a paper's result: its preview is its gist, not a build.").arg(tag);
        } else {
            doc.refusal = QStringLiteral("ref-unknown");
            doc.refusalMessage = QStringLiteral("`%1` isn't a statement in this project.").arg(tag);
        }
        return doc;
    }
    return compose(source, tag, BuildMode::Alone, false, true);
}

} // namespace loom
