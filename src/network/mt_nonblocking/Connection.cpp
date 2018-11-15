#include "Connection.h"

#include <iostream>
#include <memory>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Start");
    _logger->debug("Socket = {}", _socket);
    _isAlive.store(true);

    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLONESHOT;
}

// See Connection.h
void Connection::OnError() {
    _logger->debug("OnError");
    _isAlive.store(false);
    close(_event.data.fd);
}

// See Connection.h
void Connection::OnClose() {
    _logger->debug("OnClose");
    _isAlive.store(false);
}

// See Connection.h
void Connection::DoRead() {
    _logger->debug("DoRead");
    int new_readed_bytes = -1;
    while ((new_readed_bytes = read(_socket, _client_buffer + _readed_bytes, sizeof(_client_buffer) - _readed_bytes)) >
           0) {
        _readed_bytes += new_readed_bytes;
        while (_readed_bytes > 0) {
            // There is no command yet
            if (!_command_to_execute) {
                std::size_t parsed = 0;
                if (_parser.Parse(_client_buffer, _readed_bytes, parsed)) {
                    // There is no command to be launched, continue to parse input stream
                    // Here we are, current chunk finished some command, process it
                    _command_to_execute = _parser.Build(_arg_remains);
                    if (_arg_remains > 0) {
                        _arg_remains += 2;
                    }
                }

                // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                if (parsed == 0) {
                    break;
                } else {
                    std::memmove(_client_buffer, _client_buffer + parsed, _readed_bytes - parsed);
                    _readed_bytes -= parsed;
                }
            }

            // There is command, but we still wait for argument to arrive...
            if (_command_to_execute && _arg_remains > 0) {
                // There is some parsed command, and now we are reading argument
                std::size_t to_read = std::min(_arg_remains, std::size_t(_readed_bytes));
                _argument_for_command.append(_client_buffer, to_read);

                std::memmove(_client_buffer, _client_buffer + to_read, _readed_bytes - to_read);
                _arg_remains -= to_read;
                _readed_bytes -= to_read;
            }

            // Thre is command & argument - RUN!
            if (_command_to_execute && _arg_remains == 0) {
                std::string result;
                _command_to_execute->Execute(*_pStorage, _argument_for_command, result);
                result += "\r\n";

                // Save response
                {
                    std::lock_guard<std::mutex> lock_guard(_mutex);
                    _response.push_back(result);
                    _event.events |= EPOLLOUT;
                }

                // Prepare for the next command
                _command_to_execute.reset();
                _argument_for_command.resize(0);
                _parser.Reset();
            }
        } // while (_readed_bytes)
    }
}

// See Connection.h
void Connection::DoWrite() {
    _logger->debug("DoWrite");
    size_t response_size;
    std::unique_ptr<struct iovec[]> task;
    {
        std::lock_guard<std::mutex> lock_guard(_mutex);
        response_size = _response.size();
        task = std::make_unique<struct iovec[]>(response_size);
        for (int i = 0; i < response_size; i++) {
            task[i].iov_base = &(_response[i][0]);
            task[i].iov_len = _response[i].size();
        }
    }

    task[0].iov_base = static_cast<char *>(task[0].iov_base) + _response_shift;
    task[0].iov_len -= _response_shift;

    ssize_t written;

    if ((written = writev(_socket, task.get(), response_size)) > 0) {

        _response_shift += written;

        int i = 0;
        while (_response_shift - task[i++].iov_len > 0) {
        }
        if (_response_shift < 0) {
            i--;
        }
        {
            std::lock_guard<std::mutex> lock_guard(_mutex);
            std::vector<std::string>(_response.begin() + i, _response.end()).swap(_response);
            if (_response.empty()) {
                _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLONESHOT;
            }
        }
    } else {
        _logger->error("Failed to send response");
        OnError();
    }
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
