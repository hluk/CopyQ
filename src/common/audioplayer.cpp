// SPDX-License-Identifier: GPL-3.0-or-later
#include "audioplayer.h"

#ifdef WITH_AUDIO

#include <miniaudio.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>

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
    enum class State { Uninitialized, Initialized, Error };

    std::shared_ptr<ma_engine> engine;
    State state = State::Uninitialized;
    std::vector<SoundEntry> entries;

    /// Remove sounds that have finished playing.
    void collectFinished()
    {
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [](const SoundEntry &e) { return e.atEnd(); }),
            entries.end());
    }

    /// Release the engine if all sounds have finished.
    /// Called on the main thread via the end callback.
    void releaseIfIdle()
    {
        collectFinished();
        if (entries.empty() && state == State::Initialized) {
            qCDebug(logCategory) << "All sounds finished, releasing miniaudio engine";
            engine.reset();
            state = State::Uninitialized;
        }
    }

    /// Lazily initialize the miniaudio engine on first use.
    /// Opens the PulseAudio/PipeWire connection, so we defer this
    /// to avoid a background thread spinning on the audio socket
    /// when no sound is ever played.
    void ensureInitialized()
    {
        if (state != State::Uninitialized)
            return;

        qCDebug(logCategory) << "Initializing miniaudio engine";

        // Run ma_engine_init on a helper thread so we can bail out if the
        // PulseAudio (or other) backend deadlocks during device enumeration.
        auto initResult = std::make_shared<std::atomic<ma_result>>(MA_ERROR);

        engine = std::shared_ptr<ma_engine>(new ma_engine{}, [initResult](ma_engine *e) {
            if (initResult->load(std::memory_order_acquire) == MA_SUCCESS)
                ma_engine_uninit(e);
            delete e;
        });

        auto *worker = QThread::create([initResult, eng = engine]() mutable {
            initResult->store(ma_engine_init(nullptr, eng.get()), std::memory_order_release);
            eng.reset();
        });
        worker->start();

        const bool finished = worker->wait(initTimeoutMs());

        if (!finished) {
            qCWarning(logCategory)
                << "miniaudio engine init timed out after"
                << initTimeoutMs() / 1000 << "seconds — audio disabled";
            // The worker thread still holds a shared_ptr to the engine;
            // permanently disable audio so we don't race with it.
            state = State::Error;
            QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
            return;
        }

        worker->deleteLater();

        const ma_result result = initResult->load(std::memory_order_acquire);
        if (result == MA_SUCCESS) {
            qCDebug(logCategory) << "Initialized miniaudio engine successfully";
            state = State::Initialized;
        } else {
            qCWarning(logCategory) << "Failed to initialize miniaudio, result code:" << result;
            engine.reset();
        }
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
}

AudioPlayer::~AudioPlayer() = default;

QString AudioPlayer::play(const QString &filePath, float volume)
{
    d->ensureInitialized();

    if (d->state != Private::State::Initialized)
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

    // The end callback fires from the audio thread; bounce to the main
    // thread with a delay to let the audio backend drain its buffers
    // before tearing down the engine.
    ma_sound_set_end_callback(entry.get(),
        [](void *ud, ma_sound *) {
            auto *priv = static_cast<AudioPlayer::Private *>(ud);
            QTimer::singleShot(1000, QCoreApplication::instance(), [priv]() {
                priv->releaseIfIdle();
            });
        }, d.get());

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
