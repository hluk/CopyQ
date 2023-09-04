// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemimagetests.h"

#include "tests/test_utils.h"

#include <QBuffer>
#include <QImageReader>
#include <QImageWriter>
#include <QImage>

ItemImageTests::ItemImageTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

void ItemImageTests::initTestCase()
{
    TEST(m_test->initTestCase());
}

void ItemImageTests::cleanupTestCase()
{
    TEST(m_test->cleanupTestCase());
}

void ItemImageTests::init()
{
    TEST(m_test->init());
}

void ItemImageTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemImageTests::supportedFormats()
{
    const auto supportedRead = QImageReader::supportedImageFormats();
    QVERIFY(supportedRead.contains("png"));
    QVERIFY(supportedRead.contains("bmp"));

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QVERIFY(supportedRead.contains("gif"));
    QVERIFY(supportedRead.contains("webp"));
#endif

    const auto supportedWrite = QImageWriter::supportedImageFormats();
    QVERIFY(supportedWrite.contains("png"));
    QVERIFY(supportedWrite.contains("bmp"));
}

void ItemImageTests::savePng()
{
    QImage image(":images/icon");
    QVERIFY(!image.isNull());
    image = image.scaled(QSize(16, 16));

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QBuffer::ReadWrite);

    QVERIFY(image.save(&buffer, "PNG"));
    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("read(0)", "TEST");
    TEST( m_test->setClipboard(data, "image/png") );
    WAIT_ON_OUTPUT("read('image/png', 0).length > 0", "true\n");
}

void ItemImageTests::saveBmp()
{
    QImage image(":images/icon");
    QVERIFY(!image.isNull());
    image = image.scaled(QSize(16, 16));

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QBuffer::ReadWrite);

    QVERIFY(image.save(&buffer, "BMP"));
    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("read(0)", "TEST");
    TEST( m_test->setClipboard(data, "image/bmp") );
    WAIT_ON_OUTPUT("read('image/png', 0).length > 0", "true\n");
}

void ItemImageTests::saveGif()
{
    if ( !QImageReader::supportedImageFormats().contains("gif") )
        SKIP("GIF image format is unsopported on this platform");

    const auto data = QByteArray::fromBase64(
        "R0lGODlhCAAIAPQAAAAAAE5WUURsU0ZuVVxkX3Jycnx8fFKCZFyRcFyScF6Ucl+Wc1+WdGmLdmuMeGGadmqqgmurg5OUlJubm56dnaGhobW1tb2+vgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAAIf8LSW1hZ2VNYWdpY2sNZ2FtbWE9MC40NTQ1NQAsAAAAAAgACAAABSYgAAwiKSZLkjCJ+AhRJDzikTTOcohAIF0EHmBSsVCEhkLBIGwCQgA7");
    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("read(0)", "TEST");
    TEST( m_test->setClipboard(data, "image/gif") );
    WAIT_ON_OUTPUT("read('image/png', 0).length > 0", "true\n");
}

void ItemImageTests::saveWebp()
{
    if ( !QImageReader::supportedImageFormats().contains("webp") )
        SKIP("WEBP image format is unsopported on this platform");

    const auto data = QByteArray::fromBase64(
        "UklGRrgAAABXRUJQVlA4WAoAAAAQAAAABwAABwAAQUxQSEEAAAAACFmTUFCTWQhN3url5ereTXLuwPn5wO5yLrvx/v7xuy4AIbD//7AhAAARw/z8wxEAAA2vqKivDAAAAEc2NkcAAABWUDggUAAAAPABAJ0BKggACAACADQljAJ0AQ8M/qJ2gAD+3i3bP+bdQx6F+ZyiDZNsPKvgXJjteWOHwsl4XY+Utm/aW5R74XTu3MMrnj1KVx5aDzswFkAA");
    TEST( m_test->setClipboard("TEST") );
    WAIT_ON_OUTPUT("read(0)", "TEST");
    TEST( m_test->setClipboard(data, "image/webp") );
    WAIT_ON_OUTPUT("read('image/png', 0).length > 0", "true\n");
}
