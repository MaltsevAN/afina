#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <list>
#include <memory>
#include <sys/epoll.h>
#include <sys/uio.h>

#include "protocol/Parser.h"
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> &ps, std::shared_ptr<spdlog::logger> &logger)
        : _socket(s), _pStorage(ps), _logger(logger), _isAlive(true) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _isAlive; }

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

    bool _isAlive;
    std::shared_ptr<Afina::Storage> _pStorage;
    std::shared_ptr<spdlog::logger> _logger;

    std::size_t _arg_remains;
    Protocol::Parser _parser;
    std::string _argument_for_command;
    std::unique_ptr<Execute::Command> _command_to_execute;

    int _readed_bytes = 0;
    char _client_buffer[4096];

    std::vector<std::string> _response;
    int _response_shift = 0;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
