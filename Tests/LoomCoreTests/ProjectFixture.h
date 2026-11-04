#pragma once

// A small project on disk for the tests of statements and proofs: `loom.toml` with two tags and
// its preamble, and helpers that write statement and proof files in the shapes the tests vary.

#include "TestSupport.h"

#include <QByteArray>
#include <QString>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace loomtest {

struct ProjectFixture {
    QTemporaryDir temp;
    QString root;

    ProjectFixture()
    {
        REQUIRE(temp.isValid());
        root = temp.path();
        writeFile(root + "/loom.toml",
            "format = 1\nname = \"Demo\"\ncreated = 2026-11-03\n\n[tex]\npreamble = [\"TeX/preamble.tex\"]\n\n"
            "[[tags]]\nid = \"model-system\"\nkind = \"equation\"\ntitle = \"The model system\"\n");
        writeFile(root + "/TeX/preamble.tex", "% preamble\n");
    }

    /// Writes a project file.
    void write(const QString& relative, const QByteArray& bytes) const { writeFile(root + u'/' + relative, bytes); }

    /// Reads a project file.
    QByteArray read(const QString& relative) const { return readFile(root + u'/' + relative); }

    /// Writes `Statements/<tag>.tex`, with `extra` lines from line 5 (after `title`).
    void statement(const QString& tag, const QByteArray& kind, const QByteArray& extra = {}) const
    {
        write(QStringLiteral("Statements/%1.tex").arg(tag),
              "+++\nid = \"" + tag.toUtf8() + "\"\nkind = \"" + kind + "\"\ntitle = \"T\"\n" + extra
                  + "created = 2026-11-03\n+++\nBody of " + tag.toUtf8() + ".\n");
    }

    /// Writes `Proofs/<folder>/<id>.tex`: `proves` on line 3, `premises` on line 4.
    void proof(const QString& folder, const QString& id, const QByteArray& proves, const QByteArray& premises,
               const QByteArray& level = "complete") const
    {
        write(QStringLiteral("Proofs/%1/%2.tex").arg(folder, id),
              "+++\nid = \"" + id.toUtf8() + "\"\nproves = \"" + proves + "\"\npremises = " + premises + "\nlevel = \"" + level
                  + "\"\ncreated = 2026-11-03\n+++\nProof.\n");
    }
};

} // namespace loomtest
