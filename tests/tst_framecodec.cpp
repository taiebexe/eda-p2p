// Unit-Test für das TCP-Framing ohne echtes Netzwerk.
// Simuliert genau das, was TCP mit unseren Daten machen darf:
// zerstückeln und zusammenkleben.
#include "core/FrameCodec.h"

#include <QTest>

class TestFrameCodec : public QObject
{
    Q_OBJECT

private slots:
    void headerIsBigEndianLength();
    void decodesSingleFrame();
    void decodesFrameArrivingByteByByte();
    void decodesTwoFramesInOneChunk();
    void rejectsOversizedHeader();
};

void TestFrameCodec::headerIsBigEndianLength()
{
    const QByteArray frame = FrameCodec::encode(QByteArray(42, 'x'));

    QCOMPARE(frame.size(), 4 + 42);
    QCOMPARE(frame.left(4), QByteArray::fromHex("0000002a"));  // 42 = 0x2A
}

void TestFrameCodec::decodesSingleFrame()
{
    FrameCodec codec;
    codec.append(FrameCodec::encode("hallo"));

    QByteArray frame;
    QCOMPARE(codec.next(frame), FrameCodec::Result::Frame);
    QCOMPARE(frame, QByteArray("hallo"));
    QCOMPARE(codec.next(frame), FrameCodec::Result::NeedMoreData);
    QCOMPARE(codec.bufferedBytes(), 0);
}

void TestFrameCodec::decodesFrameArrivingByteByByte()
{
    const QByteArray wire = FrameCodec::encode("{\"text\":\"Stückchen\"}");
    FrameCodec codec;
    QByteArray frame;

    // Alle Bytes bis auf das letzte: Es darf noch kein Frame herauskommen.
    for (qsizetype i = 0; i < wire.size() - 1; ++i) {
        codec.append(wire.mid(i, 1));
        QCOMPARE(codec.next(frame), FrameCodec::Result::NeedMoreData);
    }
    codec.append(wire.right(1));
    QCOMPARE(codec.next(frame), FrameCodec::Result::Frame);
    QCOMPARE(frame, QByteArray("{\"text\":\"Stückchen\"}"));
}

void TestFrameCodec::decodesTwoFramesInOneChunk()
{
    FrameCodec codec;
    // Zwei Nachrichten plus der Anfang einer dritten in einem Rutsch.
    const QByteArray third = FrameCodec::encode("drei");
    codec.append(FrameCodec::encode("eins") + FrameCodec::encode("zwei") + third.left(3));

    QByteArray frame;
    QCOMPARE(codec.next(frame), FrameCodec::Result::Frame);
    QCOMPARE(frame, QByteArray("eins"));
    QCOMPARE(codec.next(frame), FrameCodec::Result::Frame);
    QCOMPARE(frame, QByteArray("zwei"));
    QCOMPARE(codec.next(frame), FrameCodec::Result::NeedMoreData);

    codec.append(third.mid(3));
    QCOMPARE(codec.next(frame), FrameCodec::Result::Frame);
    QCOMPARE(frame, QByteArray("drei"));
}

void TestFrameCodec::rejectsOversizedHeader()
{
    FrameCodec codec;
    codec.append(QByteArray::fromHex("ffffffff"));  // behauptet: 4 GB folgen

    QByteArray frame;
    QCOMPARE(codec.next(frame), FrameCodec::Result::Error);
}

QTEST_APPLESS_MAIN(TestFrameCodec)
#include "tst_framecodec.moc"
