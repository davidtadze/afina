#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <sys/epoll.h>
#include <spdlog/logger.h>

#include <afina/execute/Command.h>
#include <protocol/Parser.h>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> pl) :
            _socket(s),
            _pStorage(std::move(ps)),
            _logger(std::move(pl)) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        }

    inline bool isAlive() const { return _is_alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    bool _is_alive;
    std::shared_ptr<spdlog::logger> _logger;

    int _already_read_bytes = 0;
    std::size_t _arg_remains;
    Protocol::Parser _parser;
    std::string _argument_for_command;
    std::unique_ptr<Execute::Command> _command_to_execute;
    char _client_buffer[4096];

    std::shared_ptr<Afina::Storage> _pStorage;

    std::vector<std::string> _answer_buf;
    int _cur_position = 0;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
