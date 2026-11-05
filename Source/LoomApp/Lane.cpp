#include "LoomApp/Lane.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QThread>

namespace loom {

Lane::Lane(const QString& name, int threads, QObject* parent)
    : QObject(parent)
{
    setObjectName(name);
    m_pool.setObjectName(name);
    m_pool.setMaxThreadCount(threads);
}

Lane::~Lane()
{
    m_pool.waitForDone();
}

void Lane::begin()
{
    if (m_tasks++ == 0)
        emit busyChanged(true);
}

void Lane::finish()
{
    if (--m_tasks == 0)
        emit busyChanged(false);
}

bool Lane::waitUntilIdle(int timeoutMs)
{
    const QDeadlineTimer deadline(timeoutMs);
    while (busy() && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return !busy();
}

} // namespace loom
