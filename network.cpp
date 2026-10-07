#include "network.hpp"

#include "asio/buffer.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/read_until.hpp"
#include "asio/streambuf.hpp"
#include "asio/write.hpp"

#include <deque>
#include <istream>
#include <set>

//use AS7 as an example and build off that

struct NetworkRuntime::Impl { //PIMPL used to prevent windows and asio fighting, lowkey dont know if this runs on linux
    struct Peer { //a client connected to a host 
        std::shared_ptr<asio::ip::tcp::socket> socket;
        asio::streambuf readBuffer;
        std::deque<std::string> writeQueue;
        int playerId = -1;
    };

    asio::io_context io;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
    std::set<std::shared_ptr<Peer>> clients;
    std::shared_ptr<Peer> serverPeer;

    ClientEventCallback clientConnectedCallback;
    ClientEventCallback clientDisconnectedCallback;
    ClientLineCallback clientLineCallback;
    ServerLineCallback serverLineCallback;
    std::function<void()> disconnectCallback;

    bool hosting = false;
    bool joining = false;
    int nextClientId = 1;

    void AcceptNextClient();
    void ReadFromClient(const std::shared_ptr<Peer>& peer);
    void WriteToClient(const std::shared_ptr<Peer>& peer);
    void ReadFromServer();
    void WriteToServer();
};

NetworkRuntime::NetworkRuntime() : impl(std::make_unique<Impl>()) {}
NetworkRuntime::~NetworkRuntime() = default;

void NetworkRuntime::Impl::AcceptNextClient() {
    if(!acceptor) return;

    acceptor->async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        AcceptNextClient();
        if(ec) return;

        auto peer = std::make_shared<Peer>(); //shared ptr, keep alive
        peer->socket = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
        peer->playerId = nextClientId++;

        clients.insert(peer);
        if(clientConnectedCallback) {
            clientConnectedCallback(peer->playerId);
        }

        // Tell this client which player id it owns.
        peer->writeQueue.push_back("WELCOME " + std::to_string(peer->playerId) + "\n");
        WriteToClient(peer);

        ReadFromClient(peer);
    });
}

void NetworkRuntime::Impl::ReadFromClient(const std::shared_ptr<Peer>& peer) {
    asio::async_read_until(*peer->socket, peer->readBuffer, '\n',
        [this, peer](std::error_code ec, std::size_t bytesRead) {
            if(ec) {
                clients.erase(peer);
                if(clientDisconnectedCallback) {
                    clientDisconnectedCallback(peer->playerId);
                }
                return;
            }

            (void)bytesRead;
            std::istream input(&peer->readBuffer);
            std::string line;
            std::getline(input, line);

            if(clientLineCallback) {
                clientLineCallback(peer->playerId, line);
            }

            ReadFromClient(peer);
        }
    );
}

void NetworkRuntime::Impl::WriteToClient(const std::shared_ptr<Peer>& peer) {
    if(peer->writeQueue.empty() || !peer->socket) return;

    asio::async_write(*peer->socket, asio::buffer(peer->writeQueue.front()),
        [this, peer](std::error_code ec, std::size_t) {
            if(ec) {
                clients.erase(peer);
                if(clientDisconnectedCallback) {
                    clientDisconnectedCallback(peer->playerId);
                }
                return;
            }

            peer->writeQueue.pop_front();
            if(!peer->writeQueue.empty()) {
                WriteToClient(peer);
            }
        }
    );
}

void NetworkRuntime::Impl::ReadFromServer() {
    if(!serverPeer || !serverPeer->socket) return;

    asio::async_read_until(*serverPeer->socket, serverPeer->readBuffer, '\n',
        [this](std::error_code ec, std::size_t bytesRead) {
            if(ec) {
                hosting = false;
                joining = false;

                if(acceptor) {
                    std::error_code closeEc;
                    acceptor->close(closeEc); //(void)
                }

                for(auto& peer : clients) {
                    if(peer->socket) {
                        std::error_code closeEc;
                        peer->socket->close(closeEc);
                    }
                }
                clients.clear();

                if(serverPeer && serverPeer->socket) {
                    std::error_code closeEc;
                    serverPeer->socket->close(closeEc); //(void)
                }
                serverPeer.reset();
                acceptor.reset();

                if(disconnectCallback) {
                    disconnectCallback();
                }
                return;
            }

            (void)bytesRead;
            std::istream input(&serverPeer->readBuffer);
            std::string line;
            std::getline(input, line);

            if(serverLineCallback) {
                serverLineCallback(line);
            }

            ReadFromServer();
        }
    );
}

void NetworkRuntime::Impl::WriteToServer() {
    if(!serverPeer || !serverPeer->socket || serverPeer->writeQueue.empty()) return;

    asio::async_write(*serverPeer->socket, asio::buffer(serverPeer->writeQueue.front()),
        [this](std::error_code ec, std::size_t) {
            if(ec) {
                hosting = false;
                joining = false;

                if(acceptor) {
                    std::error_code closeEc;
                    acceptor->close(closeEc);
                }

                for(auto& peer : clients) {
                    if(peer->socket) {
                        std::error_code closeEc;
                        peer->socket->close(closeEc);
                    }
                }
                clients.clear();

                if(serverPeer && serverPeer->socket) {
                    std::error_code closeEc;
                    serverPeer->socket->close(closeEc);
                }
                serverPeer.reset();
                acceptor.reset();

                if(disconnectCallback) {
                    disconnectCallback();
                }
                return;
            }

            serverPeer->writeQueue.pop_front();
            if(!serverPeer->writeQueue.empty()) {
                WriteToServer();
            }
        }
    );
}

void NetworkRuntime::StartHosting(
    int port,
    ClientEventCallback onClientConnected,
    ClientEventCallback onClientDisconnected,
    ClientLineCallback onClientLine
) {
    auto& state = *impl;
    Stop();

    state.clientConnectedCallback = std::move(onClientConnected);
    state.clientDisconnectedCallback = std::move(onClientDisconnected);
    state.clientLineCallback = std::move(onClientLine);
    state.hosting = true;
    state.joining = false;
    state.nextClientId = 1;

    state.acceptor = std::make_unique<asio::ip::tcp::acceptor>(
        state.io,
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)
    );

    state.AcceptNextClient();
}

void NetworkRuntime::StartJoining(
    const std::string& host,
    int port,
    ServerLineCallback onServerLine,
    std::function<void()> onDisconnect
) {
    auto& state = *impl;
    Stop();

    state.serverLineCallback = std::move(onServerLine);
    state.disconnectCallback = std::move(onDisconnect);
    state.hosting = false;
    state.joining = true;

    state.serverPeer = std::make_shared<Impl::Peer>();
    state.serverPeer->socket = std::make_shared<asio::ip::tcp::socket>(state.io);

    asio::ip::address address = asio::ip::make_address(host);
    asio::ip::tcp::endpoint endpoint(address, port);
    state.serverPeer->socket->connect(endpoint);

    state.ReadFromServer();
}

void NetworkRuntime::Stop() {
    auto& state = *impl;
    state.hosting = false;
    state.joining = false;

    if(state.acceptor) {
        std::error_code ec;
        state.acceptor->close(ec); //(void)
    }

    for(auto& peer : state.clients) {
        if(peer->socket) {
            std::error_code ec;
            peer->socket->close(ec);
        }
    }
    state.clients.clear();

    if(state.serverPeer && state.serverPeer->socket) {
        std::error_code ec;
        state.serverPeer->socket->close(ec);
    }
    state.serverPeer.reset();
    state.acceptor.reset();
}

void NetworkRuntime::Poll() {
    impl->io.poll();
}

void NetworkRuntime::SendToServer(const std::string& line) {
    auto& state = *impl;
    if(!state.joining || !state.serverPeer || !state.serverPeer->socket) return;

    bool wasBusy = !state.serverPeer->writeQueue.empty();
    state.serverPeer->writeQueue.push_back(line);
    if(!wasBusy) {
        state.WriteToServer();
    }
}

void NetworkRuntime::SendToAllClients(const std::string& line) {
    auto& state = *impl;
    if(!state.hosting) return;

    for(const auto& peer : state.clients) {
        bool wasBusy = !peer->writeQueue.empty();
        peer->writeQueue.push_back(line);
        if(!wasBusy) {
            state.WriteToClient(peer);
        }
    }
}

bool NetworkRuntime::IsHosting() const {
    return impl->hosting;
}

bool NetworkRuntime::IsJoining() const {
    return impl->joining;
}