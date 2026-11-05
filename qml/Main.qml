// Oberfläche: nur Darstellung und Weiterleitung an den ChatController.
// Keine Netzwerklogik in QML – die liegt komplett in C++.
//
// Aufbau:  Kopfzeile (Name, eigene Adresse)
//          links: Direktverbindungen (TCP)   rechts: Chatverlauf
//          unten: EIN Eingabefeld + Auswahl "An alle" oder "an einen Peer"
import QtQuick
import QtQuick.Layouts
// Material-Stil fest im Code gewählt ("compile-time style selection"):
// sieht auf Windows, macOS und Linux gleich aus – wichtig für die Demo,
// weil das Team auf verschiedenen Betriebssystemen arbeitet.
import QtQuick.Controls.Material

ApplicationWindow {
    id: window

    // Wird in main.cpp über setInitialProperties gesetzt.
    // "required" = QML bricht mit Fehler ab, falls es jemand vergisst.
    required property ChatController controller

    // Ausgewähltes Ziel im Eingabebereich: 0 = alle (UDP), 1..n = Peer n-1 (TCP).
    // -1 = nichts gewählt (nachdem ein Peer weggefallen ist, siehe unten).
    property int targetIndex: 0
    property int lastPeerCount: 0

    width: 1000
    height: 680
    minimumWidth: 720
    minimumHeight: 460
    visible: true
    title: qsTr("EDA P2P – %1").arg(controller.displayName)

    // Hell/Dunkel folgt dem Betriebssystem.
    Material.theme: Material.System
    Material.accent: Material.Teal
    Material.primary: "#00695C"        // dunkles Petrol: weiße Schrift gut lesbar

    // Farben für die zwei Kanäle – überall gleich, damit man sie sofort erkennt.
    readonly property color broadcastColor: Material.color(Material.Teal)
    readonly property color directColor: Material.color(Material.DeepOrange)

    // Eingabefeld nur leeren, wenn wirklich gesendet wurde.
    function send() {
        const ok = targetIndex === 0
                 ? controller.sendBroadcast(messageInput.text)
                 : controller.sendDirect(targetIndex - 1, messageInput.text)
        if (ok)
            messageInput.clear()
    }

    // Auswahlliste an targetIndex angleichen. Nicht als Bindung, weil die
    // ComboBox ihren Index selbst zurücksetzt, sobald sich ihr Modell ändert.
    onTargetIndexChanged: syncTarget()
    function syncTarget() {
        targetBox.currentIndex = targetIndex
    }

    function connectToPeer() {
        if (controller.connectToPeer(addressInput.text))
            addressInput.clear()
    }

    // Peer-Liste hat sich geändert (Verbindung neu, Name per "hello", Trennung).
    // Fällt ein Peer weg, verschieben sich die Indizes. Damit eine private
    // Nachricht nie beim falschen Empfänger oder bei allen landet, muss der
    // Nutzer dann das Ziel neu wählen.
    Connections {
        target: window.controller
        function onPeersChanged() {
            const count = window.controller.peers.length
            if (count < window.lastPeerCount && window.targetIndex > 0)
                window.targetIndex = -1
            window.lastPeerCount = count
        }
    }

    // --- Kopfzeile ---------------------------------------------------------
    header: ToolBar {
        Material.foreground: "white"
        padding: 8
        leftPadding: 16
        rightPadding: 16

        RowLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                text: qsTr("EDA P2P")
                font.pixelSize: 20
                font.bold: true
            }
            Item { Layout.fillWidth: true }

            Label { text: qsTr("Name:"); opacity: 0.85 }
            TextField {
                id: nameField
                Layout.preferredWidth: 170
                Material.containerStyle: Material.Filled
                text: window.controller.displayName
                selectByMouse: true
                // Erst beim Verlassen/Enter übernehmen, nicht bei jedem Tastendruck.
                onEditingFinished: window.controller.displayName = text
            }

            // Eigene Adresse: Das muss der andere für die Direktverbindung eintippen.
            InfoChip { caption: qsTr("Meine IP"); value: window.controller.localAddresses }
            InfoChip { caption: qsTr("TCP"); value: window.controller.tcpPort }
            InfoChip { caption: qsTr("UDP"); value: window.controller.udpPort }
        }
    }

    // Kleine "Kachel" für Beschriftung + Wert in der Kopfzeile.
    component InfoChip: Rectangle {
        property string caption
        property var value
        radius: 6
        color: Qt.rgba(1, 1, 1, 0.15)
        implicitWidth: chipText.implicitWidth + 16
        implicitHeight: chipText.implicitHeight + 10
        Label {
            id: chipText
            anchors.centerIn: parent
            color: "white"
            text: "%1  <b>%2</b>".arg(parent.caption).arg(parent.value)
            textFormat: Text.StyledText   // nur unsere eigenen Daten, kein Text aus dem Netz
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // --- Linke Spalte: Direktverbindungen (TCP) --------------------------
        Pane {
            Layout.fillHeight: true
            Layout.preferredWidth: 280
            Material.elevation: 2
            padding: 14

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    text: qsTr("Direktverbindung (TCP)")
                    font.bold: true
                    font.pixelSize: 15
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    font.pixelSize: 12
                    text: qsTr("IP und TCP-Port des anderen eingeben. Auf demselben PC: 127.0.0.1:Port.")
                }
                TextField {
                    id: addressInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("z. B. 192.168.0.42:45455")
                    selectByMouse: true
                    onAccepted: window.connectToPeer()   // Enter-Taste
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Verbinden")
                    highlighted: true
                    enabled: addressInput.text.trim().length > 0
                    onClicked: window.connectToPeer()
                }

                MenuSeparator { Layout.fillWidth: true }

                Label {
                    text: qsTr("Verbunden (%1)").arg(peerList.count)
                    font.bold: true
                }

                ListView {
                    id: peerList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: window.controller.peers

                    // Klick auf einen Peer = ihn als Empfänger auswählen.
                    delegate: ItemDelegate {
                        required property string modelData
                        required property int index
                        width: ListView.view.width
                        text: modelData
                        highlighted: window.targetIndex === index + 1
                        onClicked: {
                            window.targetIndex = index + 1
                            messageInput.forceActiveFocus()
                        }
                    }

                    Label {
                        anchors.fill: parent
                        visible: peerList.count === 0
                        wrapMode: Text.Wrap
                        opacity: 0.6
                        font.italic: true
                        text: qsTr("Noch keine Verbindung.")
                    }
                }
            }
        }

        // --- Rechte Seite: Verlauf + Eingabe ---------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ListView {
                id: chatView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                topMargin: 12
                bottomMargin: 12
                model: window.controller.chatLog
                ScrollBar.vertical: ScrollBar {}

                // Neue Nachricht -> ans Ende scrollen.
                onCountChanged: Qt.callLater(chatView.positionViewAtEnd)

                // Ein Delegate pro Zeile. Die "required property"-Namen
                // entsprechen den roleNames() aus ChatLogModel.cpp.
                delegate: Item {
                    id: row
                    required property string time
                    required property string senderName
                    required property string messageText
                    required property string channel
                    required property bool outgoing

                    readonly property bool isSystem: channel === "System"
                    readonly property color channelColor: channel === "Direkt" ? window.directColor
                                                                              : window.broadcastColor

                    width: ListView.view.width
                    height: isSystem ? systemText.implicitHeight + 4 : bubble.height

                    // Natürliche Textbreite messen (ohne Umbruch). implicitWidth des
                    // Labels taugt nicht: Sobald es umbricht, meldet es die
                    // umgebrochene Breite, und die Blase würde zu schmal.
                    TextMetrics { id: bodyMetrics; font: body.font; text: row.messageText }
                    TextMetrics { id: metaMetrics; font: meta.font; text: meta.text }

                    // Systemmeldungen: klein, grau, mittig – keine Sprechblase.
                    Label {
                        id: systemText
                        visible: row.isSystem
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(implicitWidth, parent.width - 40)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        font.pixelSize: 12
                        opacity: 0.6
                        text: "%1 · %2".arg(row.time).arg(row.messageText)
                    }

                    // Chatnachricht: eigene rechts, fremde links.
                    Rectangle {
                        id: bubble
                        visible: !row.isSystem
                        anchors.right: row.outgoing ? parent.right : undefined
                        anchors.left: row.outgoing ? undefined : parent.left
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        width: Math.min(Math.ceil(Math.max(bodyMetrics.advanceWidth,
                                                           metaMetrics.advanceWidth)) + 32,
                                        row.width * 0.7)
                        height: content.implicitHeight + 16
                        radius: 10
                        // Leicht eingefärbt nach Kanal: grün = an alle, orange = direkt.
                        color: Qt.rgba(row.channelColor.r, row.channelColor.g, row.channelColor.b,
                                       row.outgoing ? 0.28 : 0.12)
                        border.color: Qt.rgba(row.channelColor.r, row.channelColor.g, row.channelColor.b, 0.5)

                        ColumnLayout {
                            id: content
                            anchors.fill: parent
                            anchors.margins: 8
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 2

                            Label {
                                id: meta
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.pixelSize: 11
                                color: row.channelColor
                                textFormat: Text.PlainText
                                text: "%1  ·  %2  ·  %3"
                                      .arg(row.senderName)
                                      .arg(row.channel === "Direkt" ? qsTr("direkt") : qsTr("an alle"))
                                      .arg(row.time)
                            }
                            Label {
                                id: body
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText   // Text aus dem Netz nie als HTML interpretieren
                                font.pixelSize: 15
                                text: row.messageText
                            }
                        }
                    }
                }
            }

            // --- Eingabe: EIN Feld, Ziel per Auswahlliste ---------------------
            Pane {
                Layout.fillWidth: true
                Material.elevation: 4
                padding: 10

                RowLayout {
                    anchors.fill: parent
                    spacing: 8

                    ComboBox {
                        id: targetBox
                        Layout.preferredWidth: 240
                        // Eintrag 0 = Broadcast, danach alle verbundenen Peers.
                        model: [qsTr("An alle (UDP)")].concat(
                                   window.controller.peers.map(p => qsTr("Direkt: %1").arg(p)))
                        onModelChanged: Qt.callLater(window.syncTarget)
                        displayText: currentIndex < 0 ? qsTr("Empfänger wählen …") : currentText
                        onActivated: index => window.targetIndex = index
                    }
                    TextField {
                        id: messageInput
                        Layout.fillWidth: true
                        selectByMouse: true
                        placeholderText: window.targetIndex === 0
                                         ? qsTr("Nachricht an alle im LAN …")
                                         : window.targetIndex > 0 ? qsTr("Private Nachricht …")
                                                                  : qsTr("Erst links einen Empfänger wählen")
                        onAccepted: if (sendButton.enabled) window.send()   // Enter-Taste
                    }
                    Button {
                        id: sendButton
                        text: qsTr("Senden")
                        highlighted: true
                        Material.accent: window.targetIndex > 0 ? Material.DeepOrange : Material.Teal
                        enabled: window.targetIndex >= 0 && messageInput.text.trim().length > 0
                        onClicked: window.send()
                    }
                }
            }
        }
    }

    Component.onCompleted: messageInput.forceActiveFocus()
}
