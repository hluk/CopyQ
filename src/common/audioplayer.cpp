// SPDX-License-Identifier: GPL-3.0-or-later
#include "audioplayer.h"

#ifdef WITH_AUDIO

#include <miniaudio.h>

#include <QFileInfo>
#include <QLoggingCategory>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.audio")

// Timeout (ms) for ma_engine_init.  PulseAudio's mainloop is known to deadlock
// during device enumeration on some Linux configurations (headless CI, broken
// PipeWire-PulseAudio bridge, etc.).  Running init in a detached thread with
// a timeout prevents the whole process from hanging.
// Override with COPYQ_AUDIO_INIT_TIMEOUT_SECS for diagnostics.
int initTimeoutMs()
{
    bool ok = false;
    const int secs = qEnvironmentVariableIntValue("COPYQ_AUDIO_INIT_TIMEOUT_SECS", &ok);
    return (ok && secs > 0 ? secs : 5) * 1000;
}

// RAII wrapper for a single ma_sound.  The ma_sound is heap-allocated and
// never relocated — miniaudio's async playback thread holds a pointer to it,
// so it must stay at a stable address for its entire lifetime.
class SoundEntry {
public:
    SoundEntry() : m_sound(std::make_unique<ma_sound>()) {}

    /// Initialise from file.  Returns the ma_result code.
    ma_result initFromFile(
        ma_engine *engine, const char *path, ma_uint32 flags)
    {
        const ma_result r = ma_sound_init_from_file(
            engine, path, flags, nullptr, nullptr, m_sound.get());
        m_initialized = (r == MA_SUCCESS);
        return r;
    }

    ma_sound *get() { return m_initialized ? m_sound.get() : nullptr; }

    bool atEnd() const { return m_initialized && ma_sound_at_end(m_sound.get()); }

    ~SoundEntry()
    {
        if (m_initialized)
            ma_sound_uninit(m_sound.get());
    }

    SoundEntry(SoundEntry &&) noexcept = default;
    SoundEntry &operator=(SoundEntry &&) noexcept = default;
    SoundEntry(const SoundEntry &) = delete;
    SoundEntry &operator=(const SoundEntry &) = delete;

private:
    std::unique_ptr<ma_sound> m_sound;
    bool m_initialized = false;
};

} // namespace

struct AudioPlayer::Private {
    std::shared_ptr<ma_engine> engine;
    bool initialized = false;
    std::vector<SoundEntry> entries;

    /// Remove sounds that have finished playing.
    void collectFinished()
    {
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [](const SoundEntry &e) { return e.atEnd(); }),
            entries.end());
    }
};

// Maximum volume multiplier (200%) to prevent accidental hearing damage.
static constexpr float maxVolume = 2.0f;

QString audioBackendVersion()
{
    return QStringLiteral("miniaudio " MA_VERSION_STRING);
}

AudioPlayer &AudioPlayer::instance()
{
    static AudioPlayer s;
    return s;
}

AudioPlayer::AudioPlayer()
    : d(std::make_unique<Private>())
{
    qCDebug(logCategory) << "Initializing miniaudio engine";

    // Run ma_engine_init on a helper thread so we can bail out if the
    // PulseAudio (or other) backend deadlocks during device enumeration.
    // We use QThread::wait() instead of QEventLoop to avoid reentrancy:
    // a nested event loop would process pending events (e.g. clipboard
    // change timers) that could call playSound() and deadlock on the
    // static-local initialization guard of instance().
    auto initResult = std::make_shared<std::atomic<ma_result>>(MA_ERROR);

    // The engine is shared between AudioPlayer and the init worker thread.
    // If init times out the worker keeps the engine alive until it finishes.
    d->engine = std::shared_ptr<ma_engine>(new ma_engine{}, [initResult](ma_engine *e) {
        if (initResult->load(std::memory_order_acquire) == MA_SUCCESS)
            ma_engine_uninit(e);
        delete e;
    });

    auto *worker = QThread::create([initResult, engine = d->engine]() mutable {
        initResult->store(ma_engine_init(nullptr, engine.get()), std::memory_order_release);
        engine.reset(); // release this thread's reference to the engine
    });
    worker->start();

    const bool finished = worker->wait(initTimeoutMs());

    if (!finished) {
        qCWarning(logCategory)
            << "miniaudio engine init timed out after"
            << initTimeoutMs() / 1000 << "seconds \u2014 audio disabled";
        // The worker thread holds a shared_ptr to the engine, so the engine
        // outlives the thread even if AudioPlayer is destroyed first.
        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        return;
    }

    worker->deleteLater();

    const ma_result result = initResult->load(std::memory_order_acquire);
    if (result == MA_SUCCESS) {
        qCDebug(logCategory) << "Initialized miniaudio engine successfully";
        d->initialized = true;
    } else {
        qCWarning(logCategory) << "Failed to initialize miniaudio, result code:" << result;
        d->initialized = false;
    }
}

AudioPlayer::~AudioPlayer()
{
    if (d && d->initialized) {
        d->entries.clear();
        d->engine.reset();
    }
}

QString AudioPlayer::play(const QString &filePath, float volume)
{
    if (!d->initialized)
        return QStringLiteral("Audio engine not initialized");

    if (!QFileInfo::exists(filePath))
        return QStringLiteral("File not found: ") + filePath;

    // Reclaim memory from sounds that have finished playing.
    d->collectFinished();

    SoundEntry entry;
    const QByteArray path = filePath.toUtf8();
    const ma_uint32 flags = MA_SOUND_FLAG_ASYNC
                          | MA_SOUND_FLAG_NO_PITCH
                          | MA_SOUND_FLAG_NO_SPATIALIZATION;
    ma_result result = entry.initFromFile(d->engine.get(), path.constData(), flags);
    if (result != MA_SUCCESS)
        return QStringLiteral("Failed to play sound (error %1)").arg(result);

    ma_sound_set_volume(entry.get(), std::clamp(volume, 0.0f, maxVolume));

    result = ma_sound_start(entry.get());
    if (result != MA_SUCCESS)
        return QStringLiteral("Failed to start sound (error %1)").arg(result);

    d->entries.push_back(std::move(entry));
    return {};
}


#else // !WITH_AUDIO — stub

QString audioBackendVersion()
{
    return QStringLiteral("none");
}

AudioPlayer &AudioPlayer::instance()
{
    static AudioPlayer s;
    return s;
}

struct AudioPlayer::Private {};

AudioPlayer::AudioPlayer() = default;
AudioPlayer::~AudioPlayer() = default;

QString AudioPlayer::play(const QString &, float)
{
    return QStringLiteral("Audio playback is not available in this build");
}


#endif
