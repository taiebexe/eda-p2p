# EDA P2P

Student project for **EDA – Desktop apps with C++ and Qt**, TH Köln.
A small cross-platform C++17 / Qt 6 Quick app for communication on an IPv4 LAN.

**10 November milestone:** discover peers and broadcast text with UDP, open a
direct TCP connection, and exchange text. The window shows discovered peers,
an in-memory chat, and a UDP/TCP send selector. Each process has a unique sender
name. The TCP server chooses a free port and advertises it through discovery.

Semester goals still to implement: public profiles, images, persistent local
history, and end-to-end encrypted direct messages. Current text is plaintext;
sender names are self-reported. UDP has no delivery guarantees. The peer list
records peers seen during this run; it does not yet remove offline peers.

## Requirements

- Qt **6.5 or newer**, with a desktop kit matching the compiler and architecture.
  Install Core, Gui, Qml, Quick, Network and the Qt Quick Controls QML runtime
  (normally included in the desktop Qt installation).
- CMake **3.21+**, Ninja, and a C++17 compiler: Apple Clang on macOS, MSVC on
  Windows, or GCC/Clang on Linux.
- Git; GitHub CLI is useful for collaboration but is not needed to build.

Nothing is downloaded by this project. The commands below use an existing Qt
Online Installer installation. Adjust version and kit paths on each machine.
Use a separate build directory for each OS/kit.

## Build with Qt Creator (all platforms)

1. Open `CMakeLists.txt` in Qt Creator.
2. Select the installed **desktop** kit, with matching compiler, Qt and CMake.
   On Apple Silicon choose the macOS arm64-capable kit, not an Android kit.
3. Configure with Ninja and a build directory such as `build/`.
4. Build and run the `eda-p2p` target. No extra QML import path is needed.
5. To run a second copy, launch the built app from a terminal as described below.

## Build from the command line

### macOS (Apple Silicon)

The verified local kit is `~/Qt/6.12.0/macos`. Qt's bundled tools are installed
but not on the default shell PATH. In each build terminal:

```sh
export PATH="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos"
cmake --build build
./build/eda-p2p.app/Contents/MacOS/eda-p2p --name Alice
```

To keep the PATH change, add that `export` line to `~/.zshrc` yourself.
The build produces a development app bundle using the installed Qt libraries.

### Windows (MSVC)

Use **Developer PowerShell for VS 2022** with the x64 compiler environment and
the matching `msvc2022_64` Qt kit. These examples assume Qt was installed in
`C:\Qt`. PowerShell:

```powershell
$qtKit = 'C:\Qt\6.12.0\msvc2022_64'
$env:Path = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;$qtKit\bin;$env:Path"
cmake -S . -B build -G Ninja "-DCMAKE_PREFIX_PATH=$qtKit"
cmake --build build
.\build\eda-p2p.exe --name Alice
```

If your installer uses different tool directories, use the CMake and Ninja
paths displayed in Qt Creator. For a MinGW kit use its matching MinGW compiler
environment instead of MSVC. Do not mix kits and compilers.

### Linux

With the online installer's `gcc_64` desktop kit and an existing GCC/Clang
toolchain (including the desktop/OpenGL development libraries required by Qt):

```sh
export PATH="$HOME/Qt/Tools/CMake/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/gcc_64"
cmake --build build
./build/eda-p2p --name Alice
```

System-installed CMake/Ninja also work. A distribution-provided Qt 6.5+ is an
alternative; use its installation prefix instead of the installer kit path.

## Test the milestone

### Two instances on one machine

Connect to Wi-Fi or Ethernet with an IPv4 address. This demo broadcasts on
active LAN interfaces; it needs one even when both processes are on the same
computer. Open two terminals and run the executable twice, with `--name Alice`
and `--name Bob`. On macOS, two separate launches through Finder are also possible:

```sh
open -n build/eda-p2p.app --args --name Alice
open -n build/eda-p2p.app --args --name Bob
```

1. Both instances should list the other within about three seconds.
2. Keep **UDP** selected, enter text, and click **Send** (or press Return).
   The other window should display `[UDP] Alice-…: your text`. The sender's
   `You` entry only confirms a successful local send, not remote delivery.
3. Click Bob's entry in Alice's peer list to fill the TCP host and port, then
   click **Connect** in Alice. Both windows should show **Connected**. Connect
   from one side only; this stub supports one direct connection per instance.
4. Select **TCP** and exchange text in both directions.
5. Click **Disconnect**; both windows should return to **Not connected**.

For a manual same-machine TCP test, enter `127.0.0.1` and the port shown at the
top of the other window. Ports are chosen by the OS, so the two servers do not
conflict. UDP receivers share fixed port **45454**.

### Two machines on the LAN

Build on each machine with its own Qt kit. Join the same IPv4 subnet, start the
app on each, and repeat the steps above. Allow inbound UDP **45454** and inbound
TCP for the app (its listening port is displayed in the window). Discovery
announces the correct TCP port; no router port forwarding is needed.

On macOS, allow **Local Network** access when prompted. If access was denied,
check **System Settings → Privacy & Security → Local Network** for the app or
the launching application (Qt Creator/terminal). The bundle includes a local
network usage description. Also check **System Settings → Network → Firewall
→ Options** and allow incoming connections for the app. On Windows, allow the
app on the appropriate private network in Windows Defender Firewall. Keep
firewalls enabled; add an app rule if needed.

If discovery is empty, check the error line in the window, IPv4 connectivity,
local network permission, firewall rules, and VPN routes. Guest Wi-Fi and
campus networks can block broadcasts or isolate clients. Test on a LAN that
allows device-to-device traffic. Broadcasts do not cross routers. A manual TCP
connection to the other machine's IPv4 address and displayed port helps
separate discovery problems from connection problems.

macOS privacy details: [Apple's local network privacy guide](https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy).

### Automated socket check

```sh
ctest --test-dir build --output-on-failure
cmake --build build --target eda-p2p_qmllint
```

CTest uses two networking objects on the actual host network to verify peer
discovery and bidirectional UDP/TCP text, Unicode/newlines, and basic message
validation. It requires permission to bind sockets and broadcast on an active
IPv4 LAN. It does not replace the two-window or two-machine test. On Windows,
keep the Qt kit's `bin` directory on PATH when running tests.

## Code and protocol

- `src/main.cpp`: creates networking objects and passes them to QML.
- `qml/Main.qml`: peer list, chat, TCP controls and message input.
- `src/networking/UdpDiscovery.*`: a hello every three seconds and text
  broadcasts to each active IPv4 subnet's broadcast address.
- `src/networking/TcpPeer.*`: TCP server plus a single outgoing/incoming socket;
  signals/slots and buffered line framing, with no worker threads.
- `src/networking/Message.h`: shared JSON encoding and envelope validation.

Example UDP message:

```json
{"type":"text","sender":"Alice-a1b2c3d4","timestamp":"2026-10-29T12:00:00.000Z","payload":{"text":"Hello!","tcpPort":51234}}
```

Types are `hello` and `text`. UDP uses one JSON object per datagram. TCP uses
`text` envelopes with `payload.text`, terminated by a newline; newlines within
text are escaped by JSON. Reads buffer incomplete TCP frames. Envelopes are
limited to 8192 bytes and text to 1000 UTF-16 code units (`QString::size()`).
Malformed and unknown messages are ignored. Limits keep the demo small; image
transfer and larger messages will need a separate framing/transfer design.

Build structure follows [Qt's QML application guide](https://doc.qt.io/qt-6/cmake-build-qml-application.html).

## Collaboration

Repository: [taiebexe/eda-p2p](https://github.com/taiebexe/eda-p2p). Keep `main`
buildable; use a short branch and a pull request for each change. Give each
teammate collaborator access through GitHub repository settings.

```sh
git clone git@github.com:taiebexe/eda-p2p.git
cd eda-p2p
git config user.name "Your Name"
git config user.email "your-email@example.com"
gh auth status
git switch -c feature/short-description
```

Before a PR: build, run CTest and QML lint, then review the diff. Commit small
logical changes with short English messages, and apply `.clang-format` using
`clang-format -i` to changed C++ files. Do not commit build outputs, IDE user
files or private keys. GitHub CLI authentication and Git-over-SSH access are
separate; test SSH access if cloning fails.
