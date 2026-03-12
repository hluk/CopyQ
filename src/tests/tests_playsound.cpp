// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/commandstatus.h"

#include <QDataStream>
#include <QTemporaryFile>

namespace {

/// Build a minimal valid WAV file containing silence.
/// PCM 8-bit mono at 100 Hz; \a durationMs in milliseconds.
/// Produces a tiny file (~100 bytes/s) for testing playback paths.
QByteArray makeSilentWav(int durationMs)
{
    const int sampleRate = 100;
    const int bitsPerSample = 8;
    const int numChannels = 1;
    const int bytesPerSample = bitsPerSample / 8;
    const int numSamples = sampleRate * durationMs / 1000;
    const int dataSize = numSamples * numChannels * bytesPerSample;

    QByteArray wav;
    QDataStream s(&wav, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);

    // RIFF header
    s.writeRawData("RIFF", 4);
    s << quint32(36 + dataSize); // file size - 8
    s.writeRawData("WAVE", 4);

    // fmt sub-chunk
    s.writeRawData("fmt ", 4);
    s << quint32(16);            // sub-chunk size (PCM)
    s << quint16(1);             // audio format (PCM)
    s << quint16(numChannels);
    s << quint32(sampleRate);
    s << quint32(sampleRate * numChannels * bytesPerSample); // byte rate
    s << quint16(numChannels * bytesPerSample);              // block align
    s << quint16(bitsPerSample);

    // data sub-chunk — 8-bit PCM silence is 0x80 (midpoint), not 0x00
    s.writeRawData("data", 4);
    s << quint32(dataSize);
    wav.append(QByteArray(dataSize, '\x80'));

    return wav;
}

/// Write a silent WAV to a QTemporaryFile; returns false on I/O error.
bool writeSilentWav(QTemporaryFile &tmp, int durationMs)
{
    if (!tmp.open())
        return false;
    const QByteArray wav = makeSilentWav(durationMs);
    if (tmp.write(wav) != wav.size())
        return false;
    if (!tmp.flush())
        return false;
    tmp.close();
    return true;
}

} // namespace

void Tests::commandPlaySound()
{
    // Object argument with missing file property
    RUN_EXPECT_ERROR_WITH_STDERR(
        "playSound({})",
        CommandException, "ScriptError: playSound: object argument requires a 'file' string property");

    // Object argument with non-string file property
    RUN_EXPECT_ERROR_WITH_STDERR(
        "playSound({file: 123})",
        CommandException, "ScriptError: playSound: object argument requires a 'file' string property");

    // Object argument with non-number volume
    RUN_EXPECT_ERROR_WITH_STDERR(
        "playSound({file: '/no/such/file.wav', volume: 'loud'})",
        CommandException, "ScriptError: playSound: 'volume' must be a number");

#ifndef WITH_AUDIO
    SKIP("Audio playback not compiled in (WITH_AUDIO is off)");
#endif

    SKIP_ON_ENV("COPYQ_TESTS_SKIP_AUDIO");

    // Missing argument — empty path reaches the server which reports file-not-found
    RUN_EXPECT_ERROR_WITH_STDERR(
        "playSound", CommandException, "ScriptError: File not found:");

    // File does not exist (string argument)
    RUN_EXPECT_ERROR_WITH_STDERR(
        "playSound" << "/no/such/file.wav", CommandException, "ScriptError: File not found:");

    // Object argument with nonexistent file
    RUN_EXPECT_ERROR_WITH_STDERR(
        "playSound({file: '/no/such/file.wav', volume: 50})",
        CommandException, "ScriptError: File not found:");

    // Existing file: playSound delegates to the server which starts
    // playback asynchronously.
    {
        QTemporaryFile tmp;
        QVERIFY(writeSilentWav(tmp, 500));

        // String argument (default volume)
        RUN("playSound" << tmp.fileName(), "");

        // Object argument with explicit volume
        RUN("eval" << QStringLiteral("playSound({file: '%1', volume: 50})").arg(tmp.fileName()), "");

        // Object argument without volume (default 100%)
        RUN("eval" << QStringLiteral("playSound({file: '%1'})").arg(tmp.fileName()), "");
    }
}
