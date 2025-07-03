#pragma once
#include <string>
#include <vector>
#include <functional>


namespace ChatUI 
{
    enum class State
    {
        Lobby,
        Chat
    };

    struct Callbacks
    {
        std::function<void(const std::string& nickname)> onHost;
        std::function<void(const std::string& nickname, const std::string& ip)> onJoin;
        std::function<void(const std::string& message)> onSend;
    };

    extern State state;
    extern std::string nickname;
    extern std::string serverIP;
    extern std::string inputMessage;

    void Render(const std::vector<std::string>& chatMessages, Callbacks& callbacks);
}