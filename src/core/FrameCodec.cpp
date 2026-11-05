#include "core/FrameCodec.h"

#include <QtEndian>

QByteArray FrameCodec::encode(const QByteArray& payload)
{
    if (payload.size() > static_cast<qsizetype>(MaxFrameSize))
        return {};

    QByteArray frame(HeaderSize, Qt::Uninitialized);
    // qToBigEndian schreibt die Zahl unabhängig von der CPU immer gleich
    // (höchstwertiges Byte zuerst). x86 und ARM sind Little Endian – ohne
    // Umwandlung würden verschiedene Rechner die Länge verschieden lesen.
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()), frame.data());
    frame.append(payload);
    return frame;
}

void FrameCodec::append(const QByteArray& bytes)
{
    m_buffer.append(bytes);
}

FrameCodec::Result FrameCodec::next(QByteArray& frameOut)
{
    // 1) Haben wir wenigstens den Header?
    if (m_buffer.size() < HeaderSize)
        return Result::NeedMoreData;

    // 2) Länge lesen – den Header aber noch NICHT entfernen, denn vielleicht
    //    fehlen die Nutzdaten noch und wir müssen beim nächsten Mal neu lesen.
    const quint32 length = qFromBigEndian<quint32>(m_buffer.constData());
    if (length > MaxFrameSize)
        return Result::Error;

    // 3) Sind die Nutzdaten vollständig da?
    if (m_buffer.size() - HeaderSize < static_cast<qsizetype>(length))
        return Result::NeedMoreData;

    // 4) Frame herausschneiden und aus dem Puffer entfernen.
    frameOut = m_buffer.mid(HeaderSize, length);
    m_buffer.remove(0, HeaderSize + static_cast<qsizetype>(length));
    return Result::Frame;
}
