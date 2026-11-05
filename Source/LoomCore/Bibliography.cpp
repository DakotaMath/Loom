#include "LoomCore/Bibliography.h"

#include <algorithm>
#include <utility>

namespace loom {

namespace {

/// Authors or editors as BibTeX reads them (Specs/m1-core.md, "The bibliography", A5): `Last,
/// First` joined by ` and `; a name whose `first` is empty is its surname in braces, `{Garcia
/// Lopez}`, since BibTeX reads a name of several words without a comma as "First von Last".
/// No side effects.
QString bibNames(const std::vector<PersonName>& names)
{
    QStringList out;
    for (const PersonName& name : names)
        out << (name.first.isEmpty() ? u'{' + name.last + u'}' : name.last + QStringLiteral(", ") + name.first);
    return out.join(QStringLiteral(" and "));
}

/// Builds one entry's fields in order, noting the unverified fields it uses.
struct EntryBuilder {
    const Paper& paper;
    const PaperVersion& cited;
    std::vector<std::pair<QString, QString>> fields;
    QStringList unverified;
    QString error;

    /// Notes a paper.toml field the entry used, by the name the warning gives it.
    void used(const PaperField& field, const QString& name)
    {
        if (!field.verified.isValid() && !unverified.contains(name))
            unverified << name;
    }

    /// Adds a BibTeX field with a computed value, if it has one.
    void text(const QString& bibName, const QString& value)
    {
        if (value.isEmpty())
            return;
        if (!bracesBalanced(value) && error.isEmpty())
            error = QStringLiteral("%1's `%2` has unbalanced braces, so its BibTeX entry was left out of refs.bib.").arg(paper.key, bibName);
        if (endsInEscape(value) && error.isEmpty())
            error = QStringLiteral("%1's `%2` ends in a `\\` that would escape its closing brace, so its BibTeX entry was "
                                   "left out of refs.bib.").arg(paper.key, bibName);
        fields.emplace_back(bibName, escapeBibValue(value));
    }

    /// Adds a BibTeX field from a paper.toml field, if it has a value.
    void field(const QString& bibName, const PaperField* field, const QString& name)
    {
        if (!field || field->text().isEmpty())
            return;
        used(*field, name);
        const bool names = std::holds_alternative<std::vector<PersonName>>(field->value);
        text(bibName, names ? bibNames(field->names()) : field->text());
    }

    /// Adds a field of the cited version.
    void version(const QString& bibName, QStringView name)
    {
        field(bibName, cited.field(name), name.toString());
    }

    /// `author`, or `editor` when there are no authors and `editorFallback`.
    void author(bool editorFallback)
    {
        if (paper.work.authors && !paper.work.authors->names().empty())
            field(QStringLiteral("author"), &*paper.work.authors, QStringLiteral("authors"));
        else if (editorFallback)
            editor();
    }

    /// `editor`, from `[work]`'s editors.
    void editor()
    {
        if (paper.work.editors)
            field(QStringLiteral("editor"), &*paper.work.editors, QStringLiteral("editors"));
    }

    /// `title`, from `[work]`.
    void title()
    {
        if (paper.work.title)
            field(QStringLiteral("title"), &*paper.work.title, QStringLiteral("title"));
    }

    /// `mrnumber`, `zbl` and `url` of the cited version.
    void mrZblUrl()
    {
        version(QStringLiteral("mrnumber"), u"mr");
        version(QStringLiteral("zbl"), u"zbl");
        version(QStringLiteral("url"), u"url");
    }

    /// For a published version: the newest arXiv version's identifier without its version.
    void eprint()
    {
        const PaperVersion* newest = nullptr;
        for (const PaperVersion& v : paper.versions) {
            if (v.kind == u"arxiv" && (!newest || v.arxivNumber() > newest->arxivNumber()))
                newest = &v;
        }
        if (!newest)
            return;
        const PaperField* id = newest->field(u"arxiv_id");
        if (!id || id->text().isEmpty())
            return;
        used(*id, newest->id + QStringLiteral(".arxiv_id"));
        text(QStringLiteral("eprint"), id->text());
        text(QStringLiteral("archiveprefix"), QStringLiteral("arXiv"));
        field(QStringLiteral("primaryclass"), newest->field(u"primary_class"), newest->id + QStringLiteral(".primary_class"));
        text(QStringLiteral("note"), QStringLiteral("arXiv:") + id->text());
    }
};

} // namespace

QString escapeBibValue(const QString& value)
{
    QString out;
    out.reserve(value.size());
    // Outside math, in `$…$`, or in `$$…$$`. `$$` is one delimiter: read as two `$`, it would
    // open and close math at once and leave the display formula outside it.
    enum class Math { None, Inline, Display } math = Math::None;
    // The backslashes just before the character: an odd run escapes it, an even run is that many
    // halves of LaTeX line breaks (`\\`), so `A\\% B` still needs its `%` escaped.
    qsizetype backslashes = 0;
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar c = value[i];
        const bool escaped = backslashes % 2 == 1;
        if (c == u'$' && !escaped) {
            const bool doubled = i + 1 < value.size() && value[i + 1] == u'$';
            if (math == Math::Inline)
                math = Math::None; // Closes `$…$`; a `$` right after opens the next formula.
            else if (doubled) {
                math = math == Math::Display ? Math::None : Math::Display;
                out += c;
                ++i; // The second `$` of the pair.
            } else if (math == Math::None)
                math = Math::Inline;
        }
        if ((c == u'&' || c == u'%' || c == u'#') && !escaped && math == Math::None)
            out += u'\\';
        out += value[i];
        backslashes = c == u'\\' ? backslashes + 1 : 0;
    }
    return out;
}

bool endsInEscape(const QString& value)
{
    qsizetype backslashes = 0;
    for (qsizetype i = value.size() - 1; i >= 0 && value[i] == u'\\'; --i)
        ++backslashes;
    return backslashes % 2 == 1;
}

bool bracesBalanced(const QString& value)
{
    // A6: BibTeX's parser counts every brace, `\{` and `\}` too, so this does.
    int depth = 0;
    for (const QChar c : value) {
        if (c == u'{')
            ++depth;
        else if (c == u'}' && --depth < 0)
            return false;
    }
    return depth == 0;
}

BibEntry bibEntry(const Paper& paper)
{
    BibEntry entry;
    const PaperVersion* cited = paper.version(paper.cite);
    if (!cited) {
        entry.error = QStringLiteral("%1's `cite` names `%2`, which isn't one of its versions, so its BibTeX entry was left out of refs.bib.")
                          .arg(paper.key, paper.cite);
        return entry;
    }

    EntryBuilder b{paper, *cited, {}, {}, {}};
    // `type` chooses the entry type but isn't written as a field, so an unverified one isn't
    // listed: the warning names fields written (Specs/m1-core.md, "The bibliography").
    const QString type = paper.work.typeName();
    QString entryType;
    if (cited->kind == u"arxiv") {
        // An arXiv-only paper, whatever its type: the identifier carries the cited version.
        entryType = QStringLiteral("misc");
        b.author(true);
        b.title();
        if (const PaperField* posted = cited->field(u"posted"); posted && std::holds_alternative<QDate>(posted->value)) {
            b.used(*posted, QStringLiteral("posted"));
            b.text(QStringLiteral("year"), QString::number(std::get<QDate>(posted->value).year()));
        }
        if (const PaperField* id = cited->field(u"arxiv_id"); id && !id->text().isEmpty()) {
            QString identifier = id->text();
            b.used(*id, QStringLiteral("arxiv_id"));
            if (const PaperField* number = cited->field(u"arxiv_version"))
                b.used(*number, QStringLiteral("arxiv_version"));
            identifier += u'v' + QString::number(cited->arxivNumber());
            b.text(QStringLiteral("eprint"), identifier);
            b.text(QStringLiteral("archiveprefix"), QStringLiteral("arXiv"));
            b.version(QStringLiteral("primaryclass"), u"primary_class");
            b.text(QStringLiteral("note"), QStringLiteral("arXiv:") + identifier);
        } else {
            b.version(QStringLiteral("primaryclass"), u"primary_class");
        }
    } else if (type == u"article") {
        entryType = QStringLiteral("article");
        b.author(false);
        b.title();
        for (const char16_t* name : {u"journal", u"volume", u"number", u"pages", u"year", u"doi", u"month", u"publisher", u"issn"})
            b.version(QString::fromUtf16(name), name);
        b.mrZblUrl();
        b.eprint();
    } else if (type == u"book") {
        entryType = QStringLiteral("book");
        b.author(true);
        b.title();
        b.version(QStringLiteral("edition"), u"edition");
        b.version(QStringLiteral("series"), u"series");
        b.version(QStringLiteral("volume"), u"series_volume");
        for (const char16_t* name : {u"publisher", u"address", u"year", u"isbn", u"doi"})
            b.version(QString::fromUtf16(name), name);
        b.mrZblUrl();
        b.eprint();
    } else if (type == u"incollection" || type == u"inproceedings") {
        entryType = type;
        b.author(false);
        b.title();
        b.version(QStringLiteral("booktitle"), u"booktitle");
        b.editor();
        for (const char16_t* name : {u"series", u"volume", u"publisher", u"address", u"pages", u"year", u"doi"})
            b.version(QString::fromUtf16(name), name);
        b.mrZblUrl();
        b.eprint();
    } else if (type == u"phdthesis" || type == u"mastersthesis") {
        entryType = type;
        b.author(false);
        b.title();
        for (const char16_t* name : {u"school", u"address", u"year", u"url"})
            b.version(QString::fromUtf16(name), name);
    } else if (type == u"techreport") {
        entryType = type;
        b.author(false);
        b.title();
        for (const char16_t* name : {u"institution", u"number", u"year", u"url"})
            b.version(QString::fromUtf16(name), name);
    } else {
        // talk, lecture-notes, other (and a type Loom doesn't know): `@misc`.
        entryType = QStringLiteral("misc");
        b.author(false);
        b.title();
        b.version(QStringLiteral("howpublished"), u"howpublished");
        QStringList note;
        const PaperField* venue = cited->field(u"venue");
        const PaperField* date = cited->field(u"date");
        if (venue && !venue->text().isEmpty()) {
            b.used(*venue, QStringLiteral("venue"));
            note << venue->text();
        }
        if (date && std::holds_alternative<QDate>(date->value)) {
            b.used(*date, QStringLiteral("date"));
            note << date->text();
        }
        b.text(QStringLiteral("note"), note.join(QStringLiteral(", ")));
        if (date && std::holds_alternative<QDate>(date->value))
            b.text(QStringLiteral("year"), QString::number(std::get<QDate>(date->value).year()));
        b.version(QStringLiteral("url"), u"url");
    }

    if (!b.error.isEmpty()) {
        entry.error = b.error;
        return entry;
    }
    entry.text = u'@' + entryType + u'{' + paper.key + QStringLiteral(",\n");
    for (const auto& [name, value] : b.fields)
        entry.text += QStringLiteral("  %1 = {%2},\n").arg(name, value);
    entry.text += QStringLiteral("}\n");
    entry.unverified = b.unverified;
    return entry;
}

Bibliography formatBibliography(const std::vector<BibSource>& sources)
{
    std::vector<const BibSource*> sorted;
    for (const BibSource& source : sources)
        sorted.push_back(&source);
    std::sort(sorted.begin(), sorted.end(), [](const BibSource* a, const BibSource* b) { return a->paper.key < b->paper.key; });

    Bibliography bib;
    QStringList entries;
    for (const BibSource* source : sorted) {
        // The file that is there: a folder may disagree with its key (`paper-key-mismatch`).
        const QString& file = source->path;
        const Paper* paper = &source->paper;
        const BibEntry entry = bibEntry(*paper);
        if (!entry.error.isEmpty()) {
            bib.problems.push_back({Severity::Error, QStringLiteral("bib-entry-skipped"), file, 1, entry.error});
            continue;
        }
        entries << entry.text;
        if (!entry.unverified.isEmpty())
            bib.problems.push_back({Severity::Warning, QStringLiteral("bib-unverified"), file, 1,
                QStringLiteral("refs.bib uses unverified fields: %1 %2").arg(paper->key, entry.unverified.join(QStringLiteral(", ")))});
    }
    bib.text = entries.join(u'\n').toUtf8();
    sortProblems(bib.problems);
    return bib;
}

Bibliography projectBibliography(const ProjectContents& contents, const std::function<bool(const PaperFile&)>& include)
{
    std::vector<Problem> skipped;
    std::vector<BibSource> papers;
    for (const PaperFile& file : contents.papers) {
        if (include && !include(file))
            continue;
        if (!file.reading.parsed || file.reading.document.key.isEmpty()) {
            skipped.push_back({Severity::Error, QStringLiteral("bib-entry-skipped"), file.path, 1,
                QStringLiteral("Loom can't read %1 (run `loom validate`), so its BibTeX entry was left out of refs.bib.").arg(file.path)});
            continue;
        }
        const bool hasErrors = std::any_of(file.reading.problems.begin(), file.reading.problems.end(),
                                           [](const Problem& problem) { return problem.severity == Severity::Error; });
        if (hasErrors) {
            skipped.push_back({Severity::Error, QStringLiteral("bib-entry-skipped"), file.path, 1,
                QStringLiteral("%1's paper.toml has errors (`loom validate` lists them), so its BibTeX entry was left out "
                               "of refs.bib.").arg(file.reading.document.key)});
            continue;
        }
        papers.push_back({file.path, file.reading.document});
    }
    Bibliography bib = formatBibliography(papers);
    bib.problems.insert(bib.problems.end(), skipped.begin(), skipped.end());
    sortProblems(bib.problems);
    return bib;
}

} // namespace loom
