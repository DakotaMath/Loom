#include "LoomCore/Validation.h"

#include "LoomCore/Fingerprint.h"
#include "LoomCore/Ids.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/StatementsAndProofs.h"
#include "LoomCore/TomlWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <functional>
#include <optional>

namespace loom {

namespace {

/// Records a problem.
void add(std::vector<Problem>& problems, const char* code, const QString& file, int line, const QString& message,
         Severity severity = Severity::Warning)
{
    problems.push_back({severity, QString::fromLatin1(code), file, line, message});
}

// --- Bodies -------------------------------------------------------------------------------------

/// One LaTeX command with its argument, as checkBodies() reads them.
struct LatexCommand {
    QString name;     ///< `ref`, `paperref`, `cite` or `label`.
    QString argument; ///< What's between the braces, trimmed.
    int line = 1;     ///< The file line.
};

/// The `\ref`, `\paperref`, `\cite` and `\label` commands in a body, in order.
/// Rules (see checkBodies()): comments are skipped; command and argument on one line; optional
/// arguments skipped; the name must end there (`\reference` isn't `\ref`).
/// @param body      The body, UTF-8 with LF line endings.
/// @param firstLine The file line the body starts on.
/// @returns The commands. No side effects.
std::vector<LatexCommand> latexCommands(const QByteArray& body, int firstLine)
{
    static const QRegularExpression command(
        QStringLiteral(R"(\\(ref|paperref|cite|label)(?![A-Za-z@*])\s*(?:\[[^\]]*\]\s*)*\{([^{}]*)\})"));
    std::vector<LatexCommand> commands;
    const QStringList lines = QString::fromUtf8(body).split(u'\n');
    for (qsizetype i = 0; i < lines.size(); ++i) {
        QStringView line = lines[i];
        // A `%` after an even number of backslashes starts a comment; `\%` is a percent sign.
        for (qsizetype at = 0; at < line.size(); ++at) {
            if (line[at] != u'%')
                continue;
            qsizetype slashes = 0;
            while (at - slashes > 0 && line[at - slashes - 1] == u'\\')
                ++slashes;
            if (slashes % 2 == 0) {
                line = line.first(at);
                break;
            }
        }
        for (const QRegularExpressionMatch& match : command.globalMatchView(line))
            commands.push_back({match.captured(1), match.captured(2).trimmed(), firstLine + static_cast<int>(i)});
    }
    return commands;
}

/// What checkBodies() needs to know about the project, indexed once.
struct BodyChecker {
    const ProjectContents& contents;
    std::vector<Problem> problems;
    /// Every statement's and stub's tag, to its kind (empty if its file couldn't be read).
    QHash<QString, QString> kindOfTag;
    /// Every alias of a statement or stub, to its tag.
    QHash<QString, QString> tagOfAlias;
    /// Every stub's tag, for `\paperref`.
    QSet<QString> stubTags;
    /// Every `\label` in any body.
    QSet<QString> labels;
    /// Every paper's current key (and folder), and each old key to its paper's current key.
    QSet<QString> keys;
    QHash<QString, QString> keyOfOldKey;

    explicit BodyChecker(const ProjectContents& projectContents) : contents(projectContents)
    {
        const auto node = [&](const QString& tag, bool parsed, const QString& kind, const QStringList& aliases) {
            if (!kindOfTag.contains(tag))
                kindOfTag.insert(tag, parsed ? kind : QString());
            for (const QString& alias : aliases) {
                if (!tagOfAlias.contains(alias))
                    tagOfAlias.insert(alias, tag);
            }
        };
        for (const StatementFile& file : contents.statements)
            node(file.tag, file.reading.parsed, file.reading.document.kind, file.reading.document.aliases);
        for (const StubFile& file : contents.stubs) {
            node(file.tag, file.reading.parsed, file.reading.document.kind, file.reading.document.aliases);
            stubTags.insert(file.tag);
        }

        const auto collectLabels = [&](const QByteArray& body, int line) {
            for (const LatexCommand& c : latexCommands(body, line)) {
                if (c.name == u"label")
                    labels.insert(c.argument);
            }
        };
        for (const StatementFile& file : contents.statements)
            collectLabels(file.reading.document.body, file.reading.bodyLine);
        for (const ProofFile& file : contents.proofs)
            collectLabels(file.reading.document.body, file.reading.bodyLine);
        for (const VerboseFile& file : contents.verbose)
            collectLabels(file.reading.document.body, file.reading.bodyLine);

        for (const PaperFile& file : contents.papers) {
            keys.insert(file.folder);
            if (file.reading.parsed && !file.reading.document.key.isEmpty())
                keys.insert(file.reading.document.key);
        }
        for (const PaperFile& file : contents.papers) {
            if (!file.reading.parsed)
                continue;
            const QString current = file.reading.document.key.isEmpty() ? file.folder : file.reading.document.key;
            for (const QString& old : file.reading.document.aliases) {
                if (!keys.contains(old) && !keyOfOldKey.contains(old))
                    keyOfOldKey.insert(old, current);
            }
        }
    }

    /// Resolves a `\ref` or `\paperref`, reporting `ref-alias`, `ref-dangling` or
    /// `paperref-not-stub`. `\paperref{<stub-id>}` cites a paper's result (Specs/loom.md), so
    /// it must name a stub: a statement of the project, by tag or alias, is an error (it can't
    /// become `\cite[…]{Key}` when LaTeX is built), and a `\label` doesn't count.
    /// @returns The tag of the statement or stub it names, or an empty string (also after
    ///          `paperref-not-stub`, so the reference isn't compared with the front matter).
    QString resolve(const LatexCommand& c, const QString& path)
    {
        const bool paperref = c.name == u"paperref";
        const bool isAlias = !kindOfTag.contains(c.argument);
        const QString tag = isAlias ? tagOfAlias.value(c.argument) : c.argument;
        if (!tag.isEmpty()) {
            if (paperref && !stubTags.contains(tag)) {
                add(problems, "paperref-not-stub", path, c.line,
                    QStringLiteral("`\\paperref{%1}` names %2, a statement of this project, not a paper's result. Use `\\ref{%2}`.")
                        .arg(c.argument, tag),
                    Severity::Error);
                return {};
            }
            if (isAlias)
                add(problems, "ref-alias", path, c.line, QStringLiteral("`%1` is an old name of %2. Write `%2` here.").arg(c.argument, tag));
            return tag;
        }
        if (!paperref && labels.contains(c.argument))
            return {};
        add(problems, "ref-dangling", path, c.line,
            paperref ? QStringLiteral("`\\paperref{%1}` points to nothing: it isn't the tag or old name of a paper stub.").arg(c.argument)
                     : QStringLiteral("`\\ref{%1}` points to nothing: it isn't a tag, an alias, or a `\\label` in any body.").arg(c.argument));
        return {};
    }

    /// Checks one body.
    /// @param listed  The tags its front matter lists (`stated_using` or `premises`); null for a
    ///                verbose proof, whose references aren't compared with a list.
    /// @param field   `stated_using` or `premises`, for the message.
    /// @param skip    Whether a referenced tag doesn't need listing.
    void body(const QString& path, const QByteArray& text, int bodyLine, const QStringList* listed, const QString& field,
              const std::function<bool(const QString&)>& skip)
    {
        QSet<QString> reported;
        for (const LatexCommand& c : latexCommands(text, bodyLine)) {
            if (c.name == u"cite") {
                for (const QString& part : c.argument.split(u',')) {
                    const QString key = part.trimmed();
                    if (const QString current = keyOfOldKey.value(key); !current.isEmpty())
                        add(problems, "cite-alias", path, c.line,
                            QStringLiteral("`\\cite{%1}` uses an old key of %2. It still works; `%2` is its current key.").arg(key, current));
                }
                continue;
            }
            if (c.name == u"label")
                continue;
            const QString tag = resolve(c, path);
            if (tag.isEmpty() || !listed || listed->contains(tag) || skip(tag) || reported.contains(tag))
                continue;
            reported.insert(tag);
            add(problems, "ref-not-listed", path, c.line,
                QStringLiteral("The text refers to %1 (`\\%2`) but `%3` doesn't list it.").arg(tag, c.name, field));
        }
    }
};

// --- The disk -----------------------------------------------------------------------------------

/// The names Loom knows at the top of a project: M1's, and those the design gives later
/// milestones (Specs/loom.md, "The project folder"), which M1 skips.
const QSet<QString>& knownTopLevelNames()
{
    static const QSet<QString> names = {
        // M1.
        QStringLiteral("loom.toml"), QStringLiteral("CLAUDE.md"), QStringLiteral("TeX"), QStringLiteral("Statements"),
        QStringLiteral("Proofs"), QStringLiteral("Papers"), QStringLiteral(".gitignore"), QStringLiteral(".gitattributes"),
        QStringLiteral(".git"), QStringLiteral(".loom"),
        // Later milestones.
        QStringLiteral("STATUS.md"), QStringLiteral("Specs"), QStringLiteral("Threads"), QStringLiteral("Numerics"),
        QStringLiteral("Figures"), QStringLiteral("Tasks"), QStringLiteral("Reviews"), QStringLiteral("Records"),
        QStringLiteral("Meetings"), QStringLiteral("Lean"), QStringLiteral("Wiki"), QStringLiteral("pyproject.toml"),
        QStringLiteral("uv.lock")};
    return names;
}

// --- The last commit ----------------------------------------------------------------------------

/// The ID a project file gives a statement, proof or stub: `Statements/<tag>.tex`,
/// `Proofs/<tag>/<tag>.<letters>.tex` or `Papers/<Key>/Statements/<tag>.toml`.
/// @returns The ID, or an empty string for any other file. No side effects.
QString idOfPath(const QString& path)
{
    const QStringList parts = path.split(u'/');
    if (parts.size() == 2 && parts[0] == u"Statements" && parts[1].endsWith(u".tex") && isTag(parts[1].chopped(4)))
        return parts[1].chopped(4);
    if (parts.size() == 3 && parts[0] == u"Proofs" && parts[2].endsWith(u".tex") && !parts[2].endsWith(u".verbose.tex")) {
        const std::optional<ProofIdParts> id = splitProofId(parts[2].chopped(4));
        if (id && id->tag == parts[1])
            return parts[2].chopped(4);
    }
    if (parts.size() == 4 && parts[0] == u"Papers" && parts[2] == u"Statements" && parts[3].endsWith(u".toml")
        && isTag(parts[3].chopped(5)))
        return parts[3].chopped(5);
    return {};
}

/// Whether a path is a check history: `Proofs/<tag>/<proof-id>.checks.toml`. No side effects.
bool isChecksPath(const QString& path)
{
    const QStringList parts = path.split(u'/');
    return parts.size() == 3 && parts[0] == u"Proofs" && parts[2].endsWith(u".checks.toml");
}

// --- Git ----------------------------------------------------------------------------------------

/// One object of a listing by `git ls-files --stage -z` or `git ls-tree -r -z`.
struct GitEntry {
    QString mode;   ///< `100644`, `100755`, `120000` (a link), `160000` (a submodule).
    QByteArray sha; ///< The object's name.
    QString path;   ///< Relative to the folder git ran in, with `/`.
};

/// Reads `git ls-files --stage -z` (`<mode> <object> <stage>\t<path>\0`) or `git ls-tree -r -z`
/// (`<mode> <type> <object>\t<path>\0`). Index entries in a merge conflict (stage 1–3) are left
/// out; git refuses to commit while there are any. No side effects.
std::vector<GitEntry> parseListing(const QByteArray& output, bool index)
{
    std::vector<GitEntry> entries;
    for (const QByteArray& record : output.split('\0')) {
        const qsizetype tab = record.indexOf('\t');
        if (tab < 0)
            continue;
        const QList<QByteArray> fields = record.first(tab).split(' ');
        if (fields.size() != 3 || (index && fields[2] != "0"))
            continue;
        entries.push_back({QString::fromLatin1(fields[0]), index ? fields[1] : fields[2], QString::fromUtf8(record.sliced(tab + 1))});
    }
    return entries;
}

/// Reads objects with `git cat-file --batch`.
/// @param shas The objects' names.
/// @returns Their contents, in the same order, or nothing if git failed or any is missing.
/// Side effects: runs git in `root`.
std::optional<std::vector<QByteArray>> readObjects(const GitTool& git, const QString& root, const std::vector<QByteArray>& shas)
{
    if (shas.empty())
        return std::vector<QByteArray>();
    QByteArray input;
    for (const QByteArray& sha : shas)
        input += sha + '\n';
    const GitRun run = runGit(git, {QStringLiteral("cat-file"), QStringLiteral("--batch")}, root, 120'000, input);
    if (!run.ok())
        return std::nullopt;
    // Each object is `<sha> <type> <size>\n<contents>\n`.
    std::vector<QByteArray> objects;
    const QByteArray& out = run.outputBytes;
    qsizetype at = 0;
    for (std::size_t i = 0; i < shas.size(); ++i) {
        const qsizetype headerEnd = out.indexOf('\n', at);
        if (headerEnd < 0)
            return std::nullopt;
        const QList<QByteArray> header = out.sliced(at, headerEnd - at).split(' ');
        bool sized = false;
        const qsizetype size = header.size() == 3 ? header[2].toLongLong(&sized) : 0;
        if (!sized || headerEnd + 1 + size > out.size())
            return std::nullopt;
        objects.push_back(out.sliced(headerEnd + 1, size));
        at = headerEnd + 1 + size + 1;
    }
    return objects;
}

/// Whether staged copies need a file's content: the files Loom reads in full. The rest are
/// written empty, since only their existence is checked.
bool contentNeeded(const QString& path)
{
    return path == u"loom.toml" || path.startsWith(u"Statements/") || path.startsWith(u"Proofs/") || path.startsWith(u"Papers/");
}

/// A failed validation.
ValidationResult cantWork(const char* code, const QString& message)
{
    ValidationResult result;
    result.outcome = ValidationOutcome::CantWork;
    result.code = QString::fromLatin1(code);
    result.message = message;
    return result;
}

/// The failure when git didn't run (`git-missing`) or didn't finish in time (`git-timeout`).
/// @param arguments The command's arguments, to name it. No side effects.
ValidationResult gitDidntRun(const GitRun& run, const QStringList& arguments)
{
    ValidationResult result = cantWork("git-missing", gitDidntRunMessage(run, arguments));
    result.code = gitDidntRunCode(run, result.code);
    return result;
}

/// Why git failed, for a message: its stderr's first line, else its exit code.
QString whyGitFailed(const GitRun& run)
{
    QString why = run.errors.trimmed().section(u'\n', 0, 0);
    while (why.endsWith(u'.'))
        why.chop(1);
    if (run.timedOut)
        return QStringLiteral("it didn't finish in time");
    return why.isEmpty() ? QStringLiteral("it exited with code %1").arg(run.exitCode) : why;
}

/// Copies git's index into `folder` (see validateProject()).
/// @returns An empty result on success, else the failure. Side effects: runs git; writes files.
std::optional<ValidationResult> copyIndex(const GitTool& git, const QString& root, const QString& folder)
{
    const QStringList list = {QStringLiteral("ls-files"), QStringLiteral("--stage"), QStringLiteral("-z")};
    const GitRun listing = runGit(git, list, root);
    if (!listing.started)
        return gitDidntRun(listing, list);
    if (!listing.ok())
        return cantWork("not-a-repository",
            QStringLiteral("`loom validate --staged` checks what git has staged, but `%1` isn't a git repository (%2).")
                .arg(QDir::toNativeSeparators(root), whyGitFailed(listing)));

    std::vector<GitEntry> wanted;
    std::vector<QByteArray> shas;
    std::vector<QString> empty;
    for (const GitEntry& entry : parseListing(listing.outputBytes, true)) {
        if (entry.mode == u"160000" || !isProjectPath(entry.path))
            continue; // A submodule, or a name Loom never reads (and Windows may not allow).
        if (contentNeeded(entry.path)) {
            wanted.push_back(entry);
            shas.push_back(entry.sha);
        } else {
            empty.push_back(entry.path);
        }
    }
    const std::optional<std::vector<QByteArray>> objects = readObjects(git, root, shas);
    if (!objects)
        return cantWork("git-failed", QStringLiteral("Loom couldn't read the staged files from git (`git cat-file --batch` failed)."));
    for (std::size_t i = 0; i < wanted.size(); ++i) {
        if (const QString error = writeProjectFile(folder, wanted[i].path, (*objects)[i]); !error.isEmpty())
            return cantWork("unreadable", QStringLiteral("Loom couldn't copy the staged `%1` to check it: %2").arg(wanted[i].path, error));
    }
    for (const QString& path : empty) {
        if (const QString error = writeProjectFile(folder, path, {}); !error.isEmpty())
            return cantWork("unreadable", QStringLiteral("Loom couldn't copy the staged `%1` to check it: %2").arg(path, error));
    }
    return std::nullopt;
}

/// The failure of a git command readCommit() runs: `check-changed` and `deleted-since-commit`
/// can't be checked, which validation must say rather than skip them.
ValidationResult commitUnreadable(const QString& command, const GitRun& run)
{
    return cantWork("git-failed", QStringLiteral("Loom couldn't read the last commit to compare the checks with it: `%1` failed (%2).")
                                      .arg(command, whyGitFailed(run)));
}

/// Reads the project's files at `HEAD`.
/// Rule (Specs/m1-core.md, "Checks", rule 1, and "IDs", "Permanent"): only two cases mean there
/// is nothing to compare with, and the checks against the commit are skipped: no repository
/// (`git rev-parse --git-dir` fails saying so), and a repository without a commit yet (`git
/// rev-parse --verify --quiet HEAD^{commit}` exits 1 without a word). Any other failure, such as
/// a repository git refuses to use ("dubious ownership") or a fatal error reading `HEAD`, is
/// `git-failed`. (A `HEAD` naming a missing object also exits 1 silently, so it reads as "no
/// commit yet"; git's own commands fail loudly there.)
/// @param committed Set to the files, if there is a commit.
/// @returns Nothing if it worked (or there's nothing to compare with), else the failure.
///          Side effects: runs git.
std::optional<ValidationResult> readCommit(const GitTool& git, const QString& root, std::optional<CommittedFiles>& committed)
{
    // In English, whatever the user's locale, so that "not a git repository" can be recognised.
    GitTool english = git;
    english.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    const QStringList gitDir = {QStringLiteral("rev-parse"), QStringLiteral("--git-dir")};
    const GitRun repository = runGit(english, gitDir, root);
    if (!repository.started)
        return gitDidntRun(repository, gitDir);
    if (!repository.ok()) {
        if (repository.errors.contains(u"not a git repository"))
            return std::nullopt;
        return commitUnreadable(QStringLiteral("git rev-parse --git-dir"), repository);
    }

    const QStringList verifyHead = {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"),
                                    QStringLiteral("HEAD^{commit}")};
    const GitRun head = runGit(git, verifyHead, root);
    if (!head.started)
        return gitDidntRun(head, verifyHead);
    if (!head.ok()) {
        if (head.exitCode == 1 && head.errors.trimmed().isEmpty())
            return std::nullopt; // No commit yet: the first commit through the hook.
        return commitUnreadable(QStringLiteral("git rev-parse HEAD"), head);
    }

    // Paths relative to the project, limited to it, as `ls-files` gives them.
    const GitRun tree = runGit(git, {QStringLiteral("ls-tree"), QStringLiteral("-r"), QStringLiteral("-z"), QStringLiteral("HEAD")}, root);
    if (!tree.ok())
        return cantWork("git-failed", QStringLiteral("Loom couldn't list the last commit's files: `git ls-tree` failed (%1).").arg(whyGitFailed(tree)));
    CommittedFiles files;
    std::vector<QString> checksPaths;
    std::vector<QByteArray> shas;
    for (const GitEntry& entry : parseListing(tree.outputBytes, false)) {
        files.paths << entry.path;
        if (isChecksPath(entry.path)) {
            checksPaths.push_back(entry.path);
            shas.push_back(entry.sha);
        }
    }
    const std::optional<std::vector<QByteArray>> objects = readObjects(git, root, shas);
    if (!objects)
        return cantWork("git-failed", QStringLiteral("Loom couldn't read the last commit's check histories (`git cat-file --batch` failed)."));
    for (std::size_t i = 0; i < checksPaths.size(); ++i)
        files.checks.insert(checksPaths[i], (*objects)[i]);
    committed = std::move(files);
    return std::nullopt;
}

} // namespace

int ValidationResult::errors() const
{
    return static_cast<int>(std::count_if(problems.begin(), problems.end(), [](const Problem& p) { return p.severity == Severity::Error; }));
}

int ValidationResult::warnings() const
{
    return static_cast<int>(problems.size()) - errors();
}

std::vector<Problem> checkStatuses(const ProjectContents& contents, const ProjectGraph& graph)
{
    std::vector<Problem> problems;

    for (const StatementFile& file : contents.statements) {
        const StatementNode* node = graph.statement(file.tag);
        if (!node || !file.reading.parsed)
            continue;
        const FieldLines& lines = file.reading.lines;
        if (node->status == Status::False) {
            // What would prove it if it weren't refuted: a complete, checked proof whose premises
            // are proved or cited (Specs/m1-core.md, "The proved set"). It isn't in the proved set
            // itself, so nothing resting on it is either.
            for (const QString& id : node->proofs) {
                const ProofNode* proof = graph.proof(id);
                if (!proof || proof->gives != Gives::CompleteChecked)
                    continue;
                const bool premisesHold = std::all_of(proof->premises.begin(), proof->premises.end(), [&](const QString& tag) {
                    const StatementNode* premise = graph.statement(tag);
                    return premise && (premise->status == Status::Proved || premise->status == Status::Cited);
                });
                if (premisesHold) {
                    add(problems, "refuted-but-proved", file.path, lines.of(QStringLiteral("refuted_by")),
                        QStringLiteral("%1 is marked false, but %2 would prove it. One of them is wrong.").arg(file.tag, id));
                    break;
                }
            }
        }
        if (node->kind == u"conjecture" && node->status == Status::Proved)
            add(problems, "conjecture-proved", file.path, lines.of(QStringLiteral("kind")),
                QStringLiteral("Conjecture %1 is now proved. Change its kind to theorem (or lemma, …) when you're ready.").arg(file.tag));
    }

    for (const ProofFile& file : contents.proofs) {
        if (!file.reading.parsed)
            continue;
        const QStringList& premises = file.reading.document.premises;
        QSet<QString> seen;
        for (qsizetype i = 0; i < premises.size(); ++i) {
            const StatementNode* premise = graph.statement(premises[i]);
            if (!premise || seen.contains(premises[i]))
                continue;
            seen.insert(premises[i]);
            const int line = file.reading.lines.of(QStringLiteral("premises"), i);
            if (premise->status == Status::False)
                add(problems, "premise-false", file.path, line, QStringLiteral("%1 uses %2, which is false.").arg(file.id, premise->id));
            if (!premise->supersededBy.isEmpty())
                add(problems, "premise-superseded", file.path, line,
                    QStringLiteral("%1 uses %2, which is superseded by %3.").arg(file.id, premise->id, premise->supersededBy));
        }
    }

    for (const VerboseFile& file : contents.verbose) {
        const ProofNode* proof = graph.proof(file.proofId);
        const QString& written = file.reading.document.fingerprint;
        if (!file.reading.parsed || !proof || proof->checking.fingerprint.isEmpty() || !isFingerprint(written))
            continue;
        if (written != proof->checking.fingerprint)
            add(problems, "verbose-stale", file.path, file.reading.lines.of(QStringLiteral("fingerprint")),
                QStringLiteral("The verbose version of %1 was written from an older version of the proof.").arg(file.proofId));
    }
    return problems;
}

std::vector<Problem> checkBodies(const ProjectContents& contents)
{
    BodyChecker checker(contents);
    for (const StatementFile& file : contents.statements) {
        const StatementReading& r = file.reading;
        if (!r.parsed)
            continue;
        checker.body(file.path, r.document.body, r.bodyLine, &r.document.statedUsing, QStringLiteral("stated_using"),
                     [&](const QString& tag) { return tag == file.tag; });
    }
    for (const ProofFile& file : contents.proofs) {
        const ProofReading& r = file.reading;
        if (!r.parsed)
            continue;
        // A proof's refs to the statement it proves, and to statements without a status, don't count.
        checker.body(file.path, r.document.body, r.bodyLine, &r.document.premises, QStringLiteral("premises"),
                     [&](const QString& tag) {
                         const QString kind = checker.kindOfTag.value(tag);
                         return tag == file.folder || (statementKinds.contains(kind) && !isProvableKind(kind));
                     });
    }
    for (const VerboseFile& file : contents.verbose) {
        if (file.reading.parsed)
            checker.body(file.path, file.reading.document.body, file.reading.bodyLine, nullptr, {},
                         [](const QString&) { return true; });
    }
    return checker.problems;
}

std::vector<Problem> checkDisk(const QString& root, const ProjectContents& contents)
{
    std::vector<Problem> problems;
    const QFileInfoList top = QDir(root).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& entry : top) {
        const QString name = entry.fileName();
        if (!knownTopLevelNames().contains(name))
            add(problems, "unknown-top-level", name, 1,
                QStringLiteral("Loom doesn't know `%1` at the top of the project. Move it into a folder Loom knows, or remove it.")
                    .arg(entry.isDir() ? name + u'/' : name));
    }

    for (const ChecksFile& file : contents.checks) {
        const ChecksReading& r = file.reading;
        if (!r.parsed)
            continue;
        for (std::size_t i = 0; i < r.checks.size(); ++i) {
            const QString& record = r.checks[i].record;
            if (record.isEmpty() || !isProjectPath(record))
                continue; // Missing (check-record-missing) or malformed (bad-path): reported with the file.
            const std::optional<QString> resolved = resolveProjectPath(root, record);
            if (!resolved || !QFileInfo::exists(*resolved))
                add(problems, "check-record-not-found", file.path, r.lines[i].of(QStringLiteral("record")),
                    QStringLiteral("Check %1 links `%2`, which doesn't exist.").arg(i + 1).arg(record));
        }
    }
    return problems;
}

std::vector<Problem> checkAgainstCommit(const ProjectContents& contents, const CommittedFiles& committed)
{
    std::vector<Problem> problems;

    // Statements, proofs and stubs are never deleted.
    QSet<QString> present;
    for (const StatementFile& file : contents.statements)
        present.insert(file.tag);
    for (const ProofFile& file : contents.proofs)
        present.insert(file.id);
    for (const StubFile& file : contents.stubs)
        present.insert(file.tag);
    for (const QString& path : committed.paths) {
        const QString id = idOfPath(path);
        if (!id.isEmpty() && !present.contains(id))
            add(problems, "deleted-since-commit", path, 1,
                QStringLiteral("%1 was in the last commit but its file is gone. Tags are permanent: supersede a statement, "
                               "or mark a proof `failed`, instead of deleting it.").arg(id));
    }

    // Checks are append-only (Specs/m1-core.md, "Checks", rule 1, and A7): every committed entry
    // is still there, unchanged and in the same order relative to the other committed entries;
    // new entries may sit between them, as a merge interleaves by date. So the committed list
    // must be a subsequence of the current one. Order matters even where dates tie, since rule 2
    // then counts the later entry in the file as the newer.
    // Each committed entry is matched, in order, to the first equal entry after the previous
    // match; greedy matching finds a match for every entry exactly when the committed list is a
    // subsequence. An entry without a match was changed, moved or removed.
    for (auto it = committed.checks.constBegin(); it != committed.checks.constEnd(); ++it) {
        const QString& path = it.key();
        const ChecksReading before = readChecks(it.value(), path);
        if (!before.parsed)
            continue;
        const auto now = std::find_if(contents.checks.begin(), contents.checks.end(), [&](const ChecksFile& f) { return f.path == path; });
        if (now != contents.checks.end() && !now->reading.parsed)
            continue; // Its own errors come first.
        const std::vector<Check> empty;
        const std::vector<Check>& after = now == contents.checks.end() ? empty : now->reading.checks;
        const auto headerLine = [&](std::size_t j) {
            return j < static_cast<std::size_t>(now->reading.lines.size()) ? now->reading.lines[j].header : 1;
        };
        std::vector<bool> used(after.size(), false);
        std::size_t next = 0; // The first current entry after the last match.
        for (const Check& check : before.checks) {
            std::size_t j = next;
            while (j < after.size() && !sameCheck(check, after[j]))
                ++j;
            if (j < after.size()) {
                used[j] = true;
                next = j + 1;
                continue;
            }
            // The line: the entry itself if it moved, else where it should be, else line 1.
            int line = 1;
            std::size_t moved = 0;
            while (moved < after.size() && (used[moved] || !sameCheck(check, after[moved])))
                ++moved;
            if (moved < after.size())
                line = headerLine(moved);
            else if (next < after.size())
                line = headerLine(next);
            add(problems, "check-changed", path, line,
                QStringLiteral("A check committed earlier (%1, %2) was changed, moved or removed. Checks are never edited or "
                               "reordered; add a new one instead.")
                    .arg(check.by, check.date.isValid() ? tomlOffsetDateTime(check.date) : QStringLiteral("no date")),
                Severity::Error);
        }
    }
    return problems;
}

ValidationResult validateProject(const ValidationRequest& request)
{
    const QString root = QFileInfo(request.root).absoluteFilePath();

    // The tree to check: the project, or a copy of what git has staged. The temporary folder is
    // made only for the copy, since making one creates it on disk.
    std::optional<QTemporaryDir> staging;
    QString tree = root;
    if (request.staged) {
        staging.emplace();
        if (!staging->isValid())
            return cantWork("unreadable", QStringLiteral("Loom couldn't make a temporary folder to check the staged files in."));
        if (std::optional<ValidationResult> failed = copyIndex(request.git, root, staging->path()))
            return *failed;
        tree = staging->path();
    }

    const OpenedProject project = openProject(tree);
    switch (project.status) {
    case OpenStatus::Opened:
        break;
    case OpenStatus::NotAProject:
        return cantWork("not-a-project", request.staged ? QStringLiteral("`loom.toml` isn't staged, so the commit would have no project in it.")
                                                        : project.message);
    case OpenStatus::Unreadable:
        return cantWork("unreadable", project.message);
    case OpenStatus::FormatTooNew:
        return cantWork("format-too-new", project.message);
    }
    const ContentsLoading loading = loadContents(tree);
    if (!loading.ok)
        return cantWork("unreadable", loading.message);
    const ProjectContents& contents = loading.contents;

    std::optional<CommittedFiles> committed;
    if (std::optional<ValidationResult> failed = readCommit(request.git, root, committed))
        return *failed;

    ValidationResult result;
    result.outcome = ValidationOutcome::Checked;
    std::vector<Problem>& problems = result.problems;
    const auto take = [&](const std::vector<Problem>& more) { problems.insert(problems.end(), more.begin(), more.end()); };
    take(project.problems);
    take(checkContents(project.config, contents));
    take(checkStatuses(contents, buildGraph(contents)));
    take(checkBodies(contents));
    take(checkDisk(tree, contents));
    if (committed)
        take(checkAgainstCommit(contents, *committed));
    sortProblems(problems);
    return result;
}

} // namespace loom
