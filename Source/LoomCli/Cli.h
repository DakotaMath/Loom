#pragma once

// What every `loom` subcommand shares (Specs/m1-core.md, "The `loom` command"): exit codes,
// output, JSON, reading arguments and finding the project.

#include "LoomCore/ProjectContents.h"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <optional>
#include <utility>

namespace loom {
struct ChangeResult;
}

namespace cli {

// Exit codes, from the spec's table.
constexpr int exitDone = 0;
constexpr int exitRefused = 1;
constexpr int exitUsage = 2;
constexpr int exitCantWork = 3;

/// The options every subcommand takes, taken out of the arguments before the subcommand sees them.
struct Context {
    /// `--json`: print one JSON object on stdout instead of text.
    bool json = false;
    /// `--project <folder>`; empty means "find it from the current folder".
    QString project;
};

/// Writes text to a stream as UTF-8.
/// @param stream stdout or stderr.
/// @param text   The text, written as is (add the line break yourself).
void write(std::FILE* stream, const QString& text);

/// Writes a JSON string literal: quotes, with `"`, `\` and control characters escaped.
/// @returns The JSON text. No side effects.
QString jsonString(const QString& text);

/// Writes a JSON array of strings, e.g. `["title", "body"]`. No side effects.
QString jsonStringArray(const QStringList& items);

/// A JSON string, or `null` for an empty one. No side effects.
QString jsonOptional(const QString& text);

/// A JSON number for an index, or `null`. No side effects.
QString jsonIndex(const std::optional<qsizetype>& index);

/// Reports a failure: the message on stderr and, with `--json`, the error object on stdout.
/// @returns `exitCode`, so callers can `return fail(...);`.
int fail(int exitCode, const QString& code, const QString& message, bool json);

/// Reports wrong usage: the problem, then the usage text, on stderr. With `--json`, the error
/// object on stdout holds only the problem, so a program reading `message` gets one sentence.
/// @returns exitUsage.
int usageError(const QString& problem, bool json);

/// The usage text for every subcommand, starting `Usage:`. No side effects.
QString usageText();

/// The usage text for one subcommand (`loom help <subcommand>`): `Usage:`, its lines, and the
/// line about the common options.
/// @param subcommand E.g. `add-check`; `--version` is accepted too.
/// @returns The text, or nothing if `subcommand` isn't one of `loom`'s. No side effects.
std::optional<QString> subcommandUsage(const QString& subcommand);

/// Takes the common options `--json` and `--project <folder>` out of the command line, but only
/// where they stand as options: an argument right after an option that takes a value is that
/// option's value, so `--title --json` is a title. Every option takes a value except those in
/// valuelessOptions().
/// @param args    The arguments after `loom`; on success, what is left for the subcommand.
/// @param context Set from the options found.
/// @param projectGiven Set to whether `--project` was given (its value may be empty).
/// @param error   Set to the usage problem when it fails.
/// @returns False on wrong usage (`--project` without a folder, or given twice). No side effects.
bool takeCommonOptions(QStringList& args, Context& context, bool& projectGiven, QString& error);

/// The options of any subcommand that take no value: `--json`, `--version`, `--staged`,
/// `--verified` and `--outdate-checks`. A new flag must be added here, or takeCommonOptions()
/// would take the argument after it as its value. No side effects.
const QSet<QString>& valuelessOptions();

/// Which options a subcommand takes.
struct ArgSpec {
    qsizetype positionals = 0; ///< How many arguments that aren't options it needs, exactly.
    QSet<QString> single;      ///< Options with a value, given at most once.
    QSet<QString> repeated;    ///< Options with a value, given any number of times.
    QSet<QString> flags;       ///< Options without a value.
};

/// A subcommand's arguments, read by parseArgs().
struct Args {
    QStringList positional;
    QHash<QString, QString> single;
    QHash<QString, QStringList> repeated;
    QSet<QString> flags;

    /// The value of a single option, if given.
    std::optional<QString> value(const QString& option) const;
};

/// Reads a subcommand's arguments.
/// @param name           The subcommand, for messages.
/// @param positionalName What the one positional argument is, for the usage message, e.g.
///                       `a paper's key`; empty means `an ID`.
/// @returns The arguments, or nothing with `error` set to what's wrong. No side effects.
std::optional<Args> parseArgs(const QStringList& args, const QString& name, const ArgSpec& spec, QString& error,
                              const QString& positionalName = {});

/// Finds the project folder (Specs/m1-core.md, "The project folder": the current folder or the
/// nearest parent holding `loom.toml`, unless `--project` names one, which must hold `loom.toml`
/// too).
/// @returns The folder, or nothing with `message` set. No side effects.
std::optional<QString> findProject(const Context& context, QString& message);

/// Reads a subcommand's arguments and finds the project, the first steps of most subcommands.
/// @param exitCode       Set when it fails, after the problem is reported.
/// @param positionalName As for parseArgs().
/// @returns The arguments and the project folder, or nothing.
std::optional<std::pair<Args, QString>> start(const QStringList& args, const QString& name, const ArgSpec& spec,
                                              const Context& context, int& exitCode, const QString& positionalName = {});

/// Reads a project without taking the write lock, for commands that only read, or that check
/// what they can before asking Dakota anything.
/// @param exitCode Set to exitCantWork, after reporting the problem, when it fails; else exitDone.
/// @returns The contents, or nothing. Reads the project's files.
std::optional<loom::ProjectContents> readProject(const QString& root, bool json, int& exitCode);

/// Checks that the options in `required` were given.
/// @returns An empty string, or the usage problem. No side effects.
QString missing(const Args& args, const QString& name, const QStringList& required);

/// "A, B and C". No side effects.
QString listed(const QStringList& items);

/// Whether `loom`'s input is an interactive terminal, where Dakota can type: on Windows a
/// console (not a pipe, a file or `NUL`), elsewhere a tty (Specs/m1-core.md, "Dakota's clicks":
/// agents run `loom` without one). No side effects.
bool inputIsTerminal();

/// Reads one line typed at the terminal, without its line ending. On Windows it reads the
/// console as UTF-16, so any character typed survives.
/// @returns The line, or nothing at the end of input. Side effects: consumes input.
std::optional<QString> readTerminalLine();

/// Refuses, unless input is a terminal, with *"Only Dakota can {what}, and only at a terminal."*
/// @param what     E.g. `add Dakota's checks`.
/// @param exitCode Set to exitRefused when it refuses.
/// @returns True if input is a terminal; false after reporting the refusal.
bool requireTerminal(const QString& what, bool json, int& exitCode);

/// Asks for Dakota's click (Specs/m1-core.md, "Dakota's clicks"): *"This is Dakota's decision:
/// {what}. Type yes to go ahead:"*, going ahead only on `yes`. Call requireTerminal() first.
/// @param what     What going ahead does, e.g. `add Dakota's pass of 4KX7.a`.
/// @param exitCode Set to exitRefused when the answer isn't `yes`.
/// @returns True on `yes`; false after reporting that nothing was done.
/// Side effects: writes the question to stderr and reads a line from the terminal.
bool askDakota(const QString& what, bool json, int& exitCode);

/// Reads a text file given on the command line (`--body-file`, `--changes-file`): it must be
/// UTF-8; a byte-order mark is dropped.
/// @param exitCode Set to exitUsage (not UTF-8) or exitCantWork (unreadable) when it fails.
/// @returns The bytes, or nothing after reporting the problem. Reads the file.
std::optional<QByteArray> readTextFile(const QString& path, bool json, int& exitCode);

/// Reads the text given by `--body` or `--body-file`.
/// @param exitCode Set to exitUsage or exitCantWork when it fails.
/// @returns The text's bytes, or nothing after reporting the problem. Reads the file, if any.
std::optional<QByteArray> readBody(const Args& args, bool json, int& exitCode);

/// Prints the result of a change, or the reason it failed.
/// @param text       What to print on stdout when done, without `--json`.
/// @param jsonFields The fields of the JSON object to print when done, e.g. `"id": "4KX7"`.
/// @returns The exit code: 0 done, 1 refused, 3 can't work.
int report(const loom::ChangeResult& result, bool json, const QString& text, const QString& jsonFields);

} // namespace cli
