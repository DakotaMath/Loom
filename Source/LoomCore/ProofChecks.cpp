#include "LoomCore/ProofChecks.h"

#include "LoomCore/Fingerprint.h"

namespace loom {

std::optional<QString> proofFingerprint(const ProjectContents& contents, const QString& id, QString* problem)
{
    const auto fail = [&](const QString& why) -> std::optional<QString> {
        if (problem)
            *problem = QStringLiteral("Loom can't compute the fingerprint of %1: %2").arg(id, why);
        return std::nullopt;
    };

    const ProofFile* proof = contents.proof(id);
    if (!proof)
        return fail(QStringLiteral("it isn't a proof in this project."));
    if (!proof->reading.parsed)
        return fail(QStringLiteral("Loom can't read its front matter (%1).").arg(proof->path));
    const StatementFile* statement = contents.statement(proof->folder);
    if (!statement)
        return fail(QStringLiteral("there is no statement %1.").arg(proof->folder));
    if (!statement->reading.parsed)
        return fail(QStringLiteral("Loom can't read the front matter of %1 (%2).").arg(statement->tag, statement->path));

    FingerprintInput input;
    input.statementTag = statement->tag;
    input.statementBody = statement->reading.document.body;
    input.proofBody = proof->reading.document.body;
    for (const QString& tag : proof->reading.document.premises) {
        // Paper stubs (written `premise <tag>\n0\n\n`, with no body) come with #8.
        const StatementFile* premise = contents.statement(tag);
        if (!premise)
            return fail(QStringLiteral("its premise `%1` isn't a statement in this project.").arg(tag));
        if (!premise->reading.parsed)
            return fail(QStringLiteral("Loom can't read the front matter of its premise %1 (%2).").arg(tag, premise->path));
        input.premises.push_back({tag, premise->reading.document.body});
    }
    return fingerprint(input);
}

ProofChecking proofChecking(const ProjectContents& contents, const QString& id)
{
    ProofChecking checking;
    checking.fingerprint = proofFingerprint(contents, id).value_or(QString());
    if (const ChecksFile* file = contents.checksOf(id))
        checking.checks = file->reading.checks;
    checking.standing = standingOf(checking.checks, checking.fingerprint);
    return checking;
}

QStringList outdatedChecks(const ProjectContents& before, const ProjectContents& after)
{
    QStringList outdated;
    for (const ProofFile& proof : before.proofs) {
        const ProofChecking checking = proofChecking(before, proof.id);
        if (checking.standing.checked && proofFingerprint(after, proof.id) != checking.fingerprint)
            outdated << proof.id;
    }
    return outdated;
}

} // namespace loom
