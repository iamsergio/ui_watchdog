/* UiWatchdog
Copyright (C) 2017 Sérgio Martins <iamsergio@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UIWATCHDOG_H
#define UIWATCHDOG_H

#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QElapsedTimer>
#include <QDebug>

#ifdef Q_OS_WIN
# include <Windows.h>
#endif

#define MAX_TIME_BLOCKED 300 // ms

class UiWatchdog;
class UiWatchdogWorker : public QObject
{
public:
    enum Option {
        OptionNone = 0,
        OptionDebugBreak = 1
    };
    typedef int Options;

private:
    UiWatchdogWorker(Options options)
        : QObject()
        , m_watchTimer(new QTimer(this))
        , m_options(options)
    {
        connect(m_watchTimer, &QTimer::timeout, this, &UiWatchdogWorker::checkUI);
    }

    void start(int frequency_msecs = 200)
    {
        m_watchTimer->start(frequency_msecs);
        m_elapsedTimeSinceLastBeat.start();
    }

    void stop()
    {
        m_watchTimer->stop();
    }

    void checkUI()
    {
        qint64 elapsed;

        {
            QMutexLocker l(&m_mutex);
            elapsed = m_elapsedTimeSinceLastBeat.elapsed();
        }

        if (elapsed > MAX_TIME_BLOCKED) {
            qDebug() << "UI is blocked !" << elapsed;
            if ((m_options & OptionDebugBreak))
                debugBreak();
        }
    }

    void debugBreak()
    {
#ifdef Q_OS_WIN
        DebugBreak();
#endif
    }

    void reset()
    {
        QMutexLocker l(&m_mutex);
        m_elapsedTimeSinceLastBeat.restart();
    }

    QTimer *const m_watchTimer;
    QElapsedTimer m_elapsedTimeSinceLastBeat;
    QMutex m_mutex;
    const Options m_options;
    friend class UiWatchdog;
};

class UiWatchdog : public QObject
{
public:

    explicit UiWatchdog(UiWatchdogWorker::Options options = UiWatchdogWorker::OptionNone, QObject *parent = nullptr)
        : QObject(parent)
        , m_uiTimer(new QTimer(this))
        , m_options(options)
    {
        connect(m_uiTimer, &QTimer::timeout, this, &UiWatchdog::onUiBeat);
    }

    ~UiWatchdog()
    {
        stop();
    }

    void start(int frequency_msecs = 100)
    {
        if (m_worker)
            return;

        m_uiTimer->start(frequency_msecs);

        m_worker = new UiWatchdogWorker(m_options);
        m_watchDogThread = new QThread(this);
        m_worker->moveToThread(m_watchDogThread);
        m_watchDogThread->start();
        connect(m_watchDogThread, &QThread::started, m_worker, [this, frequency_msecs] {
            m_worker->start(frequency_msecs);
        });

        connect(m_worker, &QObject::destroyed, m_watchDogThread, &QThread::quit);
        connect(m_watchDogThread, &QThread::finished, m_watchDogThread, &QObject::deleteLater);
    }

    void stop()
    {
        if (!m_worker)
            return;

        m_uiTimer->stop();
        m_worker->stop();
        m_worker->deleteLater();

        m_watchDogThread = nullptr;
        m_worker = nullptr;
    }

    void onUiBeat()
    {
        m_worker->reset();
    }

private:
    QTimer *const m_uiTimer;
    QThread *m_watchDogThread = nullptr;
    UiWatchdogWorker *m_worker = nullptr;
    const UiWatchdogWorker::Options m_options;
};

#endif
