// Unit-Test für das Nachrichtenformat: Kodieren -> Dekodieren muss
// dieselbe Nachricht ergeben, und kaputte Eingaben müssen abgelehnt werden.
#include "core/Message.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

class TestMessage : public QObject
{
    Q_OBJECT

private slots:
    void roundTripKeepsAllFields();
    void roundTripKeepsUmlautsAndEmoji();
    void encodesTimestampAsUtcIso();
    void helloHasEmptyPayloadObject();
    void rejectsInvalidInput_data();
    void rejectsInvalidInput();
};

void TestMessage::roundTripKeepsAllFields()
{
    const QUuid id = QUuid::createUuid();
    const Message original = Message::makeText(id, QStringLiteral("Anna"), QStringLiteral("Hallo LAN"));

    const std::optional<Message> decoded = Message::fromJson(original.toJson());

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->type, MessageType::Text);
    QCOMPARE(decoded->senderId, id);
    QCOMPARE(decoded->senderName, QStringLiteral("Anna"));
    QCOMPARE(decoded->text(), QStringLiteral("Hallo LAN"));
    // Millisekunden bleiben erhalten (ISODateWithMs).
    QCOMPARE(decoded->timestamp, original.timestamp);
}

void TestMessage::roundTripKeepsUmlautsAndEmoji()
{
    const QString text = QStringLiteral("Grüße aus Köln – ß, € und 🚀");
    const Message original = Message::makeText(QUuid::createUuid(), QStringLiteral("Jürgen"), text);

    const std::optional<Message> decoded = Message::fromJson(original.toJson());

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->text(), text);
    QCOMPARE(decoded->senderName, QStringLiteral("Jürgen"));
}

void TestMessage::encodesTimestampAsUtcIso()
{
    Message message = Message::makeText(QUuid::createUuid(), QStringLiteral("A"), QStringLiteral("x"));
    message.timestamp = QDateTime(QDate(2026, 11, 10), QTime(9, 15, 2, 123), QTimeZone::UTC);

    const QJsonObject json = QJsonDocument::fromJson(message.toJson()).object();

    QCOMPARE(json.value(QStringLiteral("timestamp")).toString(), QStringLiteral("2026-11-10T09:15:02.123Z"));
    QCOMPARE(json.value(QStringLiteral("v")).toInt(), Message::ProtocolVersion);
}

void TestMessage::helloHasEmptyPayloadObject()
{
    const Message hello = Message::makeHello(QUuid::createUuid(), QStringLiteral("Ben"));

    const std::optional<Message> decoded = Message::fromJson(hello.toJson());

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->type, MessageType::Hello);
    QVERIFY(decoded->payload.isEmpty());
}

// Datengetriebener Test: Jede Zeile ist ein eigener Fall mit Namen.
void TestMessage::rejectsInvalidInput_data()
{
    QTest::addColumn<QByteArray>("input");

    const QByteArray validId = QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
    const QByteArray ts = "\"2026-11-10T09:15:02.123Z\"";
    auto build = [&](const QByteArray& v, const QByteArray& type, const QByteArray& id,
                     const QByteArray& timestamp, const QByteArray& payload) {
        return "{\"v\":" + v + ",\"type\":" + type + ",\"senderId\":" + id
             + ",\"senderName\":\"A\",\"timestamp\":" + timestamp + ",\"payload\":" + payload + "}";
    };

    QTest::newRow("leer") << QByteArray();
    QTest::newRow("kein JSON") << QByteArray("hallo");
    QTest::newRow("JSON-Array") << QByteArray("[1,2,3]");
    QTest::newRow("abgeschnitten") << QByteArray("{\"v\":1,\"type\":\"te");
    QTest::newRow("falsche Version") << build("2", "\"text\"", '"' + validId + '"', ts, "{}");
    QTest::newRow("Version fehlt") << QByteArray("{\"type\":\"text\"}");
    QTest::newRow("Typ leer") << build("1", "\"\"", '"' + validId + '"', ts, "{}");
    QTest::newRow("ungültige ID") << build("1", "\"text\"", "\"abc\"", ts, "{}");
    QTest::newRow("ungültiger Zeitstempel") << build("1", "\"text\"", '"' + validId + '"', "\"gestern\"", "{}");
    QTest::newRow("payload kein Objekt") << build("1", "\"text\"", '"' + validId + '"', ts, "\"hi\"");
}

void TestMessage::rejectsInvalidInput()
{
    QFETCH(QByteArray, input);
    QVERIFY(!Message::fromJson(input).has_value());
}

QTEST_APPLESS_MAIN(TestMessage)
#include "tst_message.moc"
