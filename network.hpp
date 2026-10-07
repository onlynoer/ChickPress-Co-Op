#pragma once

#include <functional>
#include <memory>
#include <string>

class NetworkRuntime {
  public:
    NetworkRuntime();
    ~NetworkRuntime();

    using ClientLineCallback = std::function<void(int, const std::string&)>;
    using ClientEventCallback = std::function<void(int)>;
    using ServerLineCallback = std::function<void(const std::string&)>;

    void StartHosting(
        int port,
        ClientEventCallback onClientConnected,
        ClientEventCallback onClientDisconnected,
        ClientLineCallback onClientLine
    );

    void StartJoining(
        const std::string& host,
        int port,
        ServerLineCallback onServerLine,
        std::function<void()> onDisconnect
    );

    void Stop();
    void Poll();

    void SendToServer(const std::string& line);
    void SendToAllClients(const std::string& line);

    bool IsHosting() const;
    bool IsJoining() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
