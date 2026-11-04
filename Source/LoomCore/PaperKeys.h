#pragma once

// The key rule (Specs/m1-core.md, "Papers", "The key rule"): how a paper's key is built from its
// authors and year, how papers sharing a key get letters, and what renaming a paper does.

#include "LoomCore/Papers.h"

#include <QString>

#include <optional>
#include <utility>
#include <vector>

namespace loom {

/// A surname in ASCII CamelCase (Specs/m1-core.md, "The key rule", step 1).
/// Rule: decompose it (Unicode NFKD) and drop the combining marks (`é` → `e`); replace the
/// letters that don't decompose (`ß` → `ss`, `æ` → `ae`, `Æ` → `Ae`, `ø` → `o`, `Ø` → `O`,
/// `œ` → `oe`, `Œ` → `Oe`, `ł` → `l`, `Ł` → `L`, `đ`/`ð` → `d`, `Đ`/`Ð` → `D`, `þ` → `th`,
/// `Þ` → `Th`, `ı` → `i`); split at spaces, hyphens and apostrophes; capitalise the first letter
/// of each part and leave the rest as written; join; drop everything that isn't an ASCII letter.
/// @param last E.g. `van der Berg`.
/// @returns E.g. `VanDerBerg`; empty if nothing is left (`张`). No side effects.
QString camelSurname(const QString& last);

/// The year the key uses (Specs/m1-core.md, "The key rule", step 3): the `year` of the
/// `published` version if there is one; else the year of `posted` of the arXiv version with the
/// lowest `arxiv_version`; else the year of a version's `date` (talks, lecture notes, other).
/// @returns The year, or nothing if the paper has none. No side effects.
std::optional<int> keyYear(const Paper& paper);

/// What the key rule gives before any letter for a shared key.
struct KeyBase {
    /// E.g. `SmithJoBrGa20`; empty when it can't be made.
    QString key;
    /// When it can't be made, why, as the spec words it.
    QString problem;
};

/// The key rule without step 4 (Specs/m1-core.md, "The key rule", steps 1–3): one to three
/// authors give every CamelCase surname; four or more give the first, then the first two letters
/// of each other's (one if the surname has only one); then the year's last two digits. Built
/// from the authors, or the editors when there are none.
/// @returns The key, or why it can't be made. No side effects.
KeyBase baseKey(const Paper& paper);

/// Whether `key` follows the rule for a paper whose base key is `base`: it is `base`, or `base`
/// and one lowercase letter (a shared key, which never changes once lettered). No side effects.
bool keyFitsBase(const QString& key, const QString& base);

/// Whether paper `a` comes before paper `b` in posting order, which letters shared keys the
/// first time (Specs/m1-core.md, "The key rule", step 4): the earliest of its arXiv `posted`
/// dates, else 1 January of its published year; then its title. A paper with neither comes last.
/// No side effects.
bool postedBefore(const Paper& a, const Paper& b);

/// Another paper of the project, as key assignment sees it.
struct KeyedPaper {
    QString key;
    QStringList aliases;
    /// Its entry, for posting order; may be null if it couldn't be read.
    const Paper* paper = nullptr;
};

/// The key a paper gets, and the other papers that must be renamed for it.
struct KeyAssignment {
    /// The paper's key; empty when it can't be made.
    QString key;
    /// Other papers renamed so the keys stay apart: (old key, new key).
    std::vector<std::pair<QString, QString>> renames;
    /// When there is no key, why.
    QString problem;
};

/// Gives a paper its key among the project's other papers (Specs/m1-core.md, "The key rule",
/// step 4).
/// Rule: the base key, if no other paper's key or old key is the base or the base with a letter.
/// When another paper's key is exactly the base and no lettered key exists yet, both get letters
/// in posting order (`a` for the earlier) and the other paper is renamed. Otherwise the paper
/// takes the letter after the highest one already used with this base, as a key or an old key
/// (so an existing lettered key never changes, whatever the dates); an unlettered paper with the
/// same key is first renamed to the next letter. A key only an old key of another paper uses
/// is never taken, since `\cite` of it still means that paper.
/// @param paper  The paper to key (its own `key` and `aliases` are ignored).
/// @param others Every other paper of the project.
/// @returns The key and renames, or why there is no key. No side effects.
KeyAssignment assignKey(const Paper& paper, const std::vector<KeyedPaper>& others);

} // namespace loom
