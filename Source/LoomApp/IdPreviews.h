#pragma once

// Compiled previews of statements, for the hover over an ID and the ID picker
// (Specs/m2-statements-review.md, "IDs in the editor", "Previews": "built in the background on the
// build lane at low priority, cached by the hash of the statement's copy and the preambles, and
// rebuilt when either changes. Until one is ready, the popup shows the body's source (E2)").

#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

namespace loom {

class AppSettings;
class Lane;

/// Cuts a rendered page to what is drawn on it: the smallest rectangle holding every pixel that
/// isn't white or transparent, plus `margin` pixels on each side (within the page).
/// @returns The cut image; the image itself if nothing is drawn on it. No side effects.
QImage cropToContent(const QImage& page, int margin = 6);

/// The previews of one project's statements: built on request, one at a time and below every other
/// build of the lane, kept in memory as pictures, and checked again after the project changes.
class IdPreviews : public QObject
{
    Q_OBJECT

public:
    /// @param settings  For `latexmk` and the compile timeout.
    /// @param buildLane Where previews are built.
    IdPreviews(AppSettings* settings, Lane* buildLane, QObject* parent = nullptr);
    ~IdPreviews() override;

    /// Shows another project (or none, for an empty root): forgets every picture and stops
    /// waiting for any preview.
    void setProject(const QString& root);

    /// The latest picture of a statement's preview, or a null image if there is none yet (or it
    /// couldn't be built: then the body's source is shown). It may be from before the latest change
    /// until request() has checked it again.
    QImage image(const QString& tag) const;
    /// Whether the last try at a statement's preview found LaTeX errors.
    bool failed(const QString& tag) const;

    /// Asks for a statement's preview: if it isn't known to be current, it is built (or found in
    /// the cache) on the Build lane, and ready() says when its picture changes. Only the latest
    /// statement asked for waits; earlier ones that haven't started are dropped.
    /// Side effects: may queue a build.
    void request(const QString& tag);
    /// The project changed: every picture is checked again the next time it is asked for.
    void invalidate();
    /// Whether a preview is being built, for tests.
    bool building() const { return m_running; }

signals:
    /// A statement's picture changed (or it was found to have errors).
    void ready(const QString& tag);

private:
    /// What is known of one statement's preview.
    struct Entry {
        QString pdf;       ///< The cached PDF, project-relative; empty if none.
        QImage image;
        bool current = false; ///< Checked since the project last changed.
        bool failed = false;
    };

    /// Starts the waiting request, if nothing is running.
    void startNext();

    AppSettings* m_settings;
    Lane* m_lane;
    QString m_root;
    QHash<QString, Entry> m_entries;
    QString m_wanted;
    bool m_running = false;
    /// Stops a preview of a project that is no longer shown.
    std::shared_ptr<std::atomic_bool> m_stop;
};

} // namespace loom
