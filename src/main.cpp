#include "ChatController.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSysInfo>

namespace {
// Port aus der Kommandozeile lesen. 0 ist erlaubt (= "Betriebssystem wählt").
bool parsePort(const QString& text, quint16& port)
{
    bool ok = false;
    const uint value = text.toUInt(&ok);
    if (!ok || value > 65535)
        return false;
    port = static_cast<quint16>(value);
    return true;
}
} // namespace

int main(int argc, char* argv[])
{
    // Material-Stil (siehe Main.qml) in der kompakten Desktop-Variante:
    // kleinere Eingabefelder und Buttons als die Touch-Variante für Handys.
    // Muss VOR dem Erzeugen der App gesetzt werden.
    qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("EDA P2P"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1"));

    // Kommandozeile: Damit zwei Instanzen auf einem PC verschiedene
    // Namen/Ports bekommen, z. B.
    //   eda-p2p --name Anna --tcp-port 50001
    //   eda-p2p --name Ben  --tcp-port 50002
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("LAN-Chat per UDP-Broadcast und TCP-Direktverbindung"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption nameOption({QStringLiteral("n"), QStringLiteral("name")},
                                        QStringLiteral("Anzeigename"), QStringLiteral("name"),
                                        QSysInfo::machineHostName());
    // Alle Instanzen im LAN müssen denselben UDP-Port nutzen (gemeinsamer "Kanal").
    const QCommandLineOption udpOption({QStringLiteral("u"), QStringLiteral("udp-port")},
                                       QStringLiteral("UDP-Port für Broadcasts (für alle gleich)"),
                                       QStringLiteral("port"), QStringLiteral("45454"));
    // TCP-Ports müssen pro Rechner verschieden sein – zwei Programme können
    // nicht auf demselben TCP-Port lauschen.
    const QCommandLineOption tcpOption({QStringLiteral("t"), QStringLiteral("tcp-port")},
                                       QStringLiteral("TCP-Port für Direktverbindungen (0 = automatisch)"),
                                       QStringLiteral("port"), QStringLiteral("45455"));
    parser.addOptions({nameOption, udpOption, tcpOption});
    parser.process(app);

    quint16 udpPort = 0;
    quint16 tcpPort = 0;
    if (!parsePort(parser.value(udpOption), udpPort) || udpPort == 0
        || !parsePort(parser.value(tcpOption), tcpPort)) {
        qCritical("Ungültiger Port. Erlaubt: 1-65535 (TCP auch 0).");
        return 1;
    }

    // Reihenfolge ist Absicht: Der Controller wird VOR der Engine erzeugt und
    // damit NACH ihr zerstört (C++ zerstört in umgekehrter Reihenfolge).
    // So greift QML nie auf einen schon gelöschten Controller zu.
    ChatController controller(parser.value(nameOption), udpPort, tcpPort);
    controller.start();

    QQmlApplicationEngine engine;
    // Fehler beim Laden der QML-Datei -> Programm mit Fehlercode beenden,
    // statt ohne Fenster weiterzulaufen.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    // Main.qml deklariert "required property ChatController controller".
    // setInitialProperties füllt sie beim Erzeugen – typsicher und ohne
    // globale Kontext-Variablen (setContextProperty).
    engine.setInitialProperties({{QStringLiteral("controller"), QVariant::fromValue(&controller)}});
    engine.loadFromModule("EdaP2P", "Main");

    return QGuiApplication::exec();
}
