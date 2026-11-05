#include "LoomCore/IdSearch.h"

#include "LoomCore/Ids.h"
#include "LoomCore/LatexCommands.h"

#include <QRegularExpression>

#include <algorithm>
#include <limits>

namespace loom {

namespace {

/// A paper's key as the project uses it: `key` when its entry reads, else its folder's name.
QString currentKey(const PaperFile& file)
{
    return file.reading.parsed && !file.reading.document.key.isEmpty() ? file.reading.document.key : file.folder;
}

/// A paper's title, or an empty string.
QString paperTitle(const Paper& paper)
{
    return paper.work.title ? paper.work.title->text() : QString();
}

/// The folded names (ID and aliases) and their word parts.
void addNames(IdEntry& entry, const QStringList& names)
{
    for (const QString& name : names) {
        const QString folded = foldForSearch(name);
        entry.names << folded;
        entry.nameWords << folded << searchWords(name);
    }
}

/// The words of project tags: each tag whole and in parts.
QStringList tagWordsOf(const QStringList& tags)
{
    QStringList words;
    for (const QString& tag : tags)
        words << foldForSearch(tag) << searchWords(tag);
    return words;
}

/// Whether a folded query word starts any of the words.
bool startsAny(const QString& word, const QStringList& words)
{
    return std::any_of(words.begin(), words.end(), [&](const QString& w) { return w.startsWith(word); });
}

/// Whether the entry is one the field offers (Specs/m2-statements-review.md, "What it offers").
bool offers(IdField field, const IdEntry& entry, const QString& self)
{
    if (!self.isEmpty() && entry.id == self && field != IdField::Ref)
        return false;
    switch (field) {
    case IdField::Ref: return entry.kind == IdKind::Statement || entry.kind == IdKind::Proof || entry.kind == IdKind::Stub;
    case IdField::PaperRef: return entry.kind == IdKind::Stub;
    case IdField::Premises: return (entry.kind == IdKind::Statement || entry.kind == IdKind::Stub) && entry.provable;
    case IdField::StatedUsing: return entry.kind == IdKind::Statement || entry.kind == IdKind::Stub;
    case IdField::SupersededBy: return entry.kind == IdKind::Statement;
    case IdField::Cite: return entry.kind == IdKind::Paper;
    case IdField::RefutedBy: return entry.kind == IdKind::Proof && entry.statement != self;
    }
    return false;
}

/// Whether the whole query names the entry exactly: its ID or an alias, ignoring case and
/// accents, or a tag or proof ID as a typed one is read (`4kx7`, `4KX7.A`, `O` for `0`).
bool exactly(const QString& query, const IdEntry& entry)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty())
        return false;
    if (entry.names.contains(foldForSearch(trimmed)))
        return true;
    if (entry.kind == IdKind::Statement || entry.kind == IdKind::Stub) {
        const std::optional<QString> tag = normaliseTag(trimmed);
        return tag && *tag == entry.id;
    }
    if (entry.kind == IdKind::Proof) {
        const std::optional<QString> id = normaliseProofId(trimmed);
        return id && *id == entry.id;
    }
    return false;
}

} // namespace

QString IdEntry::label() const
{
    return title.isEmpty() ? id : QStringLiteral("%1 · %2").arg(id, title);
}

QString foldForSearch(const QString& text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString folded;
    folded.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() != QChar::Mark_NonSpacing)
            folded += c.toLower();
    }
    return folded;
}

QStringList searchWords(const QString& text)
{
    static const QRegularExpression separators(QStringLiteral("[^\\p{L}\\p{N}]+"));
    return foldForSearch(text).split(separators, Qt::SkipEmptyParts);
}

bool matchesEveryWord(const QString& query, const QStringList& words)
{
    const QStringList wanted = searchWords(query);
    return std::all_of(wanted.begin(), wanted.end(), [&](const QString& word) { return startsAny(word, words); });
}

IdIndex::IdIndex(const ProjectContents& contents, const ProjectGraph& graph)
{
    const auto push = [&](IdEntry entry) {
        m_byId.insert(entry.id, static_cast<qsizetype>(m_entries.size()));
        m_entries.push_back(std::move(entry));
    };

    // Statements and stubs in tag order (the graph's), each statement followed by its proofs.
    QStringList tags;
    for (const StatementFile& file : contents.statements)
        tags << file.tag;
    for (const StubFile& file : contents.stubs)
        tags << file.tag;
    tags.sort();
    tags.removeDuplicates();
    for (const QString& tag : tags) {
        if (const StatementFile* file = contents.statement(tag)) {
            const Statement& s = file->reading.document;
            IdEntry entry;
            entry.id = tag;
            entry.kind = IdKind::Statement;
            entry.kindName = s.kind;
            entry.title = s.title;
            entry.provable = isProvableKind(s.kind);
            addNames(entry, QStringList{tag} + s.aliases);
            entry.titleWords = searchWords(s.title);
            entry.tagWords = tagWordsOf(s.tags);
            entry.bodyWords = searchWords(QString::fromUtf8(s.body));
            entry.bodyWords.removeDuplicates();
            for (const QString& alias : s.aliases)
                m_tagOfAlias.insert(alias, tag);
            push(std::move(entry));

            const StatementNode* node = graph.statement(tag);
            for (const QString& proofId : node ? node->proofs : QStringList()) {
                const ProofFile* proofFile = contents.proof(proofId);
                if (!proofFile)
                    continue;
                IdEntry proof;
                proof.id = proofId;
                proof.kind = IdKind::Proof;
                proof.kindName = proofFile->reading.document.level;
                proof.title = s.title.isEmpty() ? QStringLiteral("proof of %1").arg(tag) : QStringLiteral("proof of %1").arg(s.title);
                proof.statement = tag;
                // Proofs are found by their ID only: their statement's words would list every
                // proof beside it.
                addNames(proof, {proofId});
                push(std::move(proof));
            }
        } else if (const StubFile* stub = contents.stub(tag)) {
            const Stub& s = stub->reading.document;
            const PaperFile* paper = contents.paper(stub->paperFolder);
            const QString key = paper ? currentKey(*paper) : stub->paperFolder;
            QString at;
            if (paper && paper->reading.parsed) {
                if (const Locator* locator = s.locator(paper->reading.document.cite))
                    at = locator->at;
            }
            if (at.isEmpty()) {
                if (const Locator* locator = s.locator(s.extractedFrom))
                    at = locator->at;
            }
            const QString name = s.title.isEmpty() ? s.gist : s.title;
            IdEntry entry;
            entry.id = tag;
            entry.kind = IdKind::Stub;
            entry.kindName = s.kind;
            entry.title = QStringLiteral("%1 %2 · %3").arg(key, at, name).replace(QStringLiteral("  "), QStringLiteral(" "));
            entry.provable = isProvableKind(s.kind);
            addNames(entry, QStringList{tag} + s.aliases);
            // A stub has no tags of its own.
            entry.titleWords = searchWords(key) + searchWords(at) + searchWords(s.title);
            entry.bodyWords = searchWords(s.gist);
            for (const QString& alias : s.aliases)
                m_tagOfAlias.insert(alias, tag);
            push(std::move(entry));
        }
    }

    // Proofs in a folder no statement has: still IDs `\ref` can name.
    for (const ProofFile& file : contents.proofs) {
        if (m_byId.contains(file.id))
            continue;
        IdEntry proof;
        proof.id = file.id;
        proof.kind = IdKind::Proof;
        proof.kindName = file.reading.document.level;
        proof.title = QStringLiteral("proof of %1").arg(file.folder);
        proof.statement = file.folder;
        addNames(proof, {file.id});
        push(std::move(proof));
    }

    // Papers by key.
    std::vector<const PaperFile*> papers;
    for (const PaperFile& file : contents.papers)
        papers.push_back(&file);
    std::sort(papers.begin(), papers.end(), [](const PaperFile* a, const PaperFile* b) { return currentKey(*a) < currentKey(*b); });
    for (const PaperFile* file : papers) {
        const QString key = currentKey(*file);
        if (m_byId.contains(key))
            continue;
        const Paper& p = file->reading.document;
        const std::vector<PersonName> names = file->reading.parsed ? p.keyNames() : std::vector<PersonName>();
        const QString first = names.empty() ? QString() : names.front().last;
        const QString title = file->reading.parsed ? paperTitle(p) : QString();
        IdEntry entry;
        entry.id = key;
        entry.kind = IdKind::Paper;
        entry.kindName = QStringLiteral("paper");
        entry.title = first.isEmpty() ? title : title.isEmpty() ? first : QStringLiteral("%1 · %2").arg(first, title);
        QStringList keys{key, file->folder};
        if (file->reading.parsed)
            keys << p.aliases;
        keys.removeDuplicates();
        addNames(entry, keys);
        for (const PersonName& person : names)
            entry.titleWords << searchWords(person.last);
        entry.titleWords << searchWords(title);
        if (file->reading.parsed) {
            entry.tagWords = tagWordsOf(p.tags);
            entry.bodyWords = searchWords(p.summary);
        }
        for (const QString& k : keys) {
            if (!m_keyOfFolded.contains(k.toCaseFolded()) || k == key)
                m_keyOfFolded.insert(k.toCaseFolded(), key);
        }
        push(std::move(entry));
    }

    // Every `\label`, which `\ref` may name.
    const auto collectLabels = [&](const QByteArray& body, int line) {
        for (const LatexCommand& c : latexCommands(body, line, {QStringLiteral("label")}))
            m_labels.insert(c.argument);
    };
    for (const StatementFile& file : contents.statements)
        collectLabels(file.reading.document.body, file.reading.bodyLine);
    for (const ProofFile& file : contents.proofs)
        collectLabels(file.reading.document.body, file.reading.bodyLine);
    for (const VerboseFile& file : contents.verbose)
        collectLabels(file.reading.document.body, file.reading.bodyLine);
}

std::vector<IdMatch> IdIndex::search(const QString& query, IdField field, const QString& self, const QStringList& recent) const
{
    const QStringList words = searchWords(query);
    std::vector<IdMatch> matches;
    for (const IdEntry& entry : m_entries) {
        if (!offers(field, entry, self))
            continue;
        if (exactly(query, entry)) {
            matches.push_back({&entry, 0});
            continue;
        }
        // Each word's best field; the result's tier is its worst word's.
        int tier = 1;
        bool all = true;
        for (const QString& word : words) {
            if (startsAny(word, entry.nameWords) || startsAny(word, entry.titleWords))
                continue;
            if (startsAny(word, entry.tagWords)) {
                tier = std::max(tier, 2);
                continue;
            }
            if (startsAny(word, entry.bodyWords)) {
                tier = std::max(tier, 3);
                continue;
            }
            all = false;
            break;
        }
        if (all)
            matches.push_back({&entry, tier});
    }
    // A proof counts as recently opened when its statement was.
    const auto recency = [&](const IdEntry& entry) {
        qsizetype at = recent.indexOf(entry.id);
        if (at < 0 && !entry.statement.isEmpty())
            at = recent.indexOf(entry.statement);
        return at < 0 ? std::numeric_limits<qsizetype>::max() : at;
    };
    std::stable_sort(matches.begin(), matches.end(), [&](const IdMatch& a, const IdMatch& b) {
        if (a.tier != b.tier)
            return a.tier < b.tier;
        const qsizetype ra = recency(*a.entry);
        const qsizetype rb = recency(*b.entry);
        if (ra != rb)
            return ra < rb;
        return a.entry->id < b.entry->id;
    });
    return matches;
}

const IdEntry* IdIndex::entry(const QString& id) const
{
    const auto found = m_byId.constFind(id);
    return found == m_byId.cend() ? nullptr : &m_entries[static_cast<std::size_t>(*found)];
}

IdResolution IdIndex::resolve(const QString& command, const QString& name) const
{
    IdResolution r;
    if (command == u"cite") {
        const QString key = m_keyOfFolded.value(name.toCaseFolded());
        if (!key.isEmpty())
            r = {true, IdKind::Paper, key, key != name};
        return r;
    }
    QString tag = name;
    bool alias = false;
    const IdEntry* found = entry(name);
    if (!found || found->kind == IdKind::Paper) {
        tag = m_tagOfAlias.value(name);
        found = tag.isEmpty() ? nullptr : entry(tag);
        alias = true;
    }
    if (command == u"paperref") {
        if (found && found->kind == IdKind::Stub)
            r = {true, IdKind::Stub, found->id, alias};
        return r;
    }
    if (found)
        return {true, found->kind, found->id, alias};
    if (m_labels.contains(name))
        return {true, IdKind::Label, name, false};
    return r;
}

const IdEntry* IdIndex::lookup(const QString& typed) const
{
    const QString trimmed = typed.trimmed();
    if (const IdEntry* found = entry(trimmed))
        return found;
    if (const QString tag = m_tagOfAlias.value(trimmed); !tag.isEmpty())
        return entry(tag);
    if (const std::optional<QString> tag = normaliseTag(trimmed)) {
        if (const IdEntry* found = entry(*tag))
            return found;
    }
    if (const std::optional<QString> id = normaliseProofId(trimmed)) {
        if (const IdEntry* found = entry(*id))
            return found;
    }
    if (const QString key = m_keyOfFolded.value(trimmed.toCaseFolded()); !key.isEmpty())
        return entry(key);
    return nullptr;
}

std::vector<IdMention> idMentions(const QString& text)
{
    std::vector<IdMention> mentions;
    const QStringList names = {QStringLiteral("ref"), QStringLiteral("paperref"), QStringLiteral("cite")};
    for (const LatexCommand& c : latexCommands(text.toUtf8(), 1, names)) {
        // The argument untrimmed, split at commas for `\cite`; each name trimmed in place.
        const QString line = text.section(u'\n', c.line - 1, c.line - 1);
        const QString argument = line.mid(c.argumentStart, c.argumentLength);
        const QStringList parts = c.name == u"cite" ? argument.split(u',') : QStringList{argument};
        qsizetype offset = c.argumentStart;
        for (const QString& part : parts) {
            qsizetype lead = 0;
            while (lead < part.size() && part[lead].isSpace())
                ++lead;
            const QString name = part.trimmed();
            if (!name.isEmpty())
                mentions.push_back({c.name, name, c.line, offset + lead, name.size()});
            offset += part.size() + 1;
        }
    }
    return mentions;
}

std::optional<IdTyping> idBeingTyped(QStringView lineBeforeCursor)
{
    if (commentStart(lineBeforeCursor) < lineBeforeCursor.size())
        return std::nullopt;
    static const QRegularExpression open(QStringLiteral(R"(\\(ref|paperref|cite)(?![A-Za-z@*])\s*(?:\[[^\]]*\]\s*)*\{([^{}]*)$)"));
    const QRegularExpressionMatch match = open.matchView(lineBeforeCursor);
    if (!match.hasMatch())
        return std::nullopt;
    IdTyping typing;
    typing.command = match.captured(1);
    qsizetype start = match.capturedStart(2);
    QString typed = match.captured(2);
    if (typing.command == u"cite") {
        const qsizetype comma = typed.lastIndexOf(u',');
        if (comma >= 0) {
            start += comma + 1;
            typed = typed.mid(comma + 1);
        }
    }
    // Spaces before the name aren't part of it.
    while (!typed.isEmpty() && typed.front().isSpace()) {
        typed.remove(0, 1);
        ++start;
    }
    typing.start = start;
    typing.typed = typed;
    return typing;
}

IdField fieldOfCommand(const QString& command)
{
    if (command == u"paperref")
        return IdField::PaperRef;
    if (command == u"cite")
        return IdField::Cite;
    return IdField::Ref;
}

} // namespace loom
