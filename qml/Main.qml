pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    required property var discovery
    required property var tcp
    width: 900
    height: 580
    minimumWidth: 700
    minimumHeight: 420
    visible: true
    title: "EDA P2P — " + discovery.sender

    function appendMessage(sender: string, text: string, transport: string) {
        chatModel.append({author: sender, body: text, channel: transport})
        chatView.positionViewAtEnd()
    }

    function sendMessage() {
        const sent = transport.currentIndex === 0
            ? discovery.sendText(messageInput.text) : tcp.sendText(messageInput.text)
        if (sent) {
            appendMessage("You", messageInput.text, transport.currentText)
            messageInput.clear()
        }
    }

    ListModel { id: peerModel }
    ListModel { id: chatModel }

    Connections {
        target: window.discovery
        function onPeerDiscovered(sender: string, address: string, tcpPort: int) {
            for (let i = 0; i < peerModel.count; ++i) {
                if (peerModel.get(i).peerName === sender) {
                    peerModel.set(i, {peerName: sender, host: address, port: tcpPort})
                    return
                }
            }
            peerModel.append({peerName: sender, host: address, port: tcpPort})
        }
        function onMessageReceived(sender: string, text: string) {
            window.appendMessage(sender, text, "UDP")
        }
        function onErrorOccurred(message: string) { statusLabel.text = message }
    }
    Connections {
        target: window.tcp
        function onMessageReceived(sender: string, text: string) {
            window.appendMessage(sender, text, "TCP")
        }
        function onErrorOccurred(message: string) { statusLabel.text = message }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: window.discovery.sender + "  •  UDP 45454  •  TCP " + window.tcp.port
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            ColumnLayout {
                Layout.preferredWidth: 240
                Layout.maximumWidth: 240
                Layout.fillHeight: true
                Label { text: "Discovered peers"; font.bold: true }
                Label {
                    visible: peerModel.count === 0
                    text: "Waiting for LAN broadcasts…"
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: peerModel
                    delegate: ItemDelegate {
                        required property string peerName
                        required property string host
                        required property int port
                        width: ListView.view.width
                        text: peerName + "\n" + host + ":" + port
                        onClicked: {
                            hostInput.text = host
                            portInput.text = port.toString()
                        }
                    }
                    ScrollBar.vertical: ScrollBar {}
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ListView {
                    id: chatView
                    anchors.fill: parent
                    clip: true
                    spacing: 12
                    model: chatModel
                    delegate: Label {
                        required property string author
                        required property string body
                        required property string channel
                        width: ListView.view.width
                        text: "[" + channel + "] " + author + ": " + body
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                    }
                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }

        RowLayout {
            Label { text: "Direct TCP" }
            TextField {
                id: hostInput
                Layout.fillWidth: true
                placeholderText: "Select a peer or enter an IPv4 address"
                text: "127.0.0.1"
            }
            TextField {
                id: portInput
                Layout.preferredWidth: 85
                placeholderText: "Port"
                validator: IntValidator { bottom: 1; top: 65535 }
            }
            Button {
                text: "Connect"
                enabled: hostInput.text.length > 0 && portInput.acceptableInput
                onClicked: window.tcp.connectToPeer(hostInput.text, Number(portInput.text))
            }
            Button {
                text: "Disconnect"
                enabled: window.tcp.connected
                onClicked: window.tcp.disconnectPeer()
            }
            Label { text: window.tcp.connected ? "Connected" : "Not connected" }
        }

        RowLayout {
            ComboBox { id: transport; model: ["UDP", "TCP"] }
            TextField {
                id: messageInput
                objectName: "messageInput"
                Layout.fillWidth: true
                placeholderText: "Message (up to 1000 characters)"
                maximumLength: 1000
                onAccepted: window.sendMessage()
            }
            Button {
                text: "Send"
                enabled: messageInput.text.trim().length > 0
                         && (transport.currentIndex === 0 || window.tcp.connected)
                onClicked: window.sendMessage()
            }
        }
        Label {
            id: statusLabel
            Layout.fillWidth: true
            text: "UDP broadcasts are best effort. Direct TCP text is unencrypted in this milestone."
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
    }
}
