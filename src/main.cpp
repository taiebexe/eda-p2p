#include "networking/TcpPeer.h"
#include "networking/UdpDiscovery.h"

#include <QCommandLineParser>
#include <QDebug>
#include <QGuiApplication>
#include <QHostInfo>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QUuid>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("EDA P2P");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("LAN discovery and UDP/TCP text demo");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(
        {"name", "Display name (an instance ID is appended).", "name", QHostInfo::localHostName()});
    parser.process(app);
    const QString sender =
        parser.value("name") + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    TcpPeer tcp(sender);
    UdpDiscovery discovery(sender);
    QQmlApplicationEngine engine;
    engine.setInitialProperties(
        {{"discovery", QVariant::fromValue(&discovery)}, {"tcp", QVariant::fromValue(&tcp)}});
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("EdaP2P", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    QObject::connect(&discovery, &UdpDiscovery::errorOccurred, &app,
                     [](const QString& message) { qWarning().noquote() << message; });
    QObject::connect(&tcp, &TcpPeer::errorOccurred, &app,
                     [](const QString& message) { qWarning().noquote() << message; });
    // Start after QML has connected its signal handlers so startup errors are visible.
    QTimer::singleShot(0, &app, [&] {
        tcp.listen();
        discovery.start(static_cast<quint16>(tcp.port()));
    });
    return app.exec();
}
