// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <memory>

/// Returns the audio backend name and version (e.g. "miniaudio 0.11.25"), or "none" if disabled.
QString audioBackendVersion();

class AudioPlayer {
public:
    static AudioPlayer &instance();

    /// Start playing an audio file at the given volume (0.0 = silent, 1.0 = 100%).
    /// Volume is clamped to [0.0, 2.0] for safety.
    /// Returns an empty string on success or an error message on failure.
    QString play(const QString &filePath, float volume);

    ~AudioPlayer();

private:
    AudioPlayer();
    AudioPlayer(const AudioPlayer &) = delete;
    AudioPlayer &operator=(const AudioPlayer &) = delete;

    struct Private;
    std::unique_ptr<Private> d;
};
