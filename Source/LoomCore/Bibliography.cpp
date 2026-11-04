#include "LoomCore/Bibliography.h"

#include <algorithm>
#include <utility>

namespace loom {

namespace {

/// The arXiv version number of a version: its `arxiv_version`, else the N of `arxiv-v<N>`.
qint64 arxivNumber(const PaperVersion& version)
{
    if (const PaperField* field = version.field(u"arxiv_version"); field && std::holds_alternative<qint64>(field->value))
        return std::get<qint64>(field->value);
    return version.id.mid(7).toLongLong();
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
        fields.emplace_back(bibName, escapeBibValue(value));
    }

    /// Adds a BibTeX field from a paper.toml field, if it has a value.
    void field(const QString& bibName, const PaperField* field, const QString& name)
    {
        if (!field || field->text().isEmpty())
            return;
        used(*field, name);
        text(bibName, field->text());
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
            if (v.kind == u"arxiv" && (!newest || arxivNumber(v) > arxivNumber(*newest)))
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
    bool math = false;
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar c = value[i];
        const bool escaped = i > 0 && value[i - 1] == u'\\';
        if (c == u'$' && !escaped)
            math = !math;
        if ((c == u'&' || c == u'%' || c == u'#') && !escaped && !math)
            out += u'\\';
        out += c;
    }
    return out;
}

bool bracesBalanced(const QString& value)
{
    int depth = 0;
    for (qsizetype i = 0; i < value.size(); ++i) {
        if (i > 0 && value[i - 1] == u'\\')
            continue;
        if (value[i] == u'{')
            ++depth;
        else if (value[i] == u'}' && --depth < 0)
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
    const QString type = paper.work.typeName();
    if (paper.work.type)
        b.used(*paper.work.type, QStringLiteral("type"));
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
            identifier += u'v' + QString::number(arxivNumber(*cited));
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

Bibliography formatBibliography(const std::vector<Paper>& papers)
{
    std::vector<const Paper*> sorted;
    for (const Paper& paper : papers)
        sorted.push_back(&paper);
    std::sort(sorted.begin(), sorted.end(), [](const Paper* a, const Paper* b) { return a->key < b->key; });

    Bibliography bib;
    QStringList entries;
    for (const Paper* paper : sorted) {
        const QString file = QStringLiteral("Papers/%1/paper.toml").arg(paper->key);
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

} // namespace loom
