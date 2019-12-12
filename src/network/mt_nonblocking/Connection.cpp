#include "Connection.h"

#include <iostream>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
  std::lock_guard<std::mutex> lock(_mutex);

  _event.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
  _event.data.fd = _socket;
  _event.data.ptr = this;

  _logger->debug("Start connection on {} socket", _socket);
}

// See Connection.h
void Connection::OnError() {
  std::lock_guard<std::mutex> lock(_mutex);
  _is_alive = false;

  _logger->warn("Error on {} socket", _socket);
}

// See Connection.h
void Connection::OnClose() {
  std::lock_guard<std::mutex> lock(_mutex);
  _is_alive = false;

  _logger->debug("Close connection on {} socket", _socket);
}

// See Connection.h
void Connection::DoRead() {
  std::lock_guard<std::mutex> lock(_mutex);
  _logger->debug("Do read on {} socket", _socket);

  try {
    int got_bytes = -1;
    while ((got_bytes = read(_socket, _client_buffer + _already_read_bytes,
                             sizeof(_client_buffer) - _already_read_bytes)) > 0) {
        _already_read_bytes += got_bytes;
        _logger->debug("Got {} bytes from socket", got_bytes);

        // Single block of data read from the socket could trigger inside actions a multiple times,
        // for example:
        // - read#0: [<command1 start>]
        // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
        while (_already_read_bytes > 0) {
            _logger->debug("Process {} bytes", _already_read_bytes);
            // There is no command yet
            if (!_command_to_execute) {
                std::size_t parsed = 0;
                if (_parser.Parse(_client_buffer, _already_read_bytes, parsed)) {
                    // There is no command to be launched, continue to parse input stream
                    // Here we are, current chunk finished some command, process it
                    _logger->debug("Found new command: {} in {} bytes", _parser.Name(), parsed);
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
                    std::memmove(_client_buffer, _client_buffer + parsed, _already_read_bytes - parsed);
                    _already_read_bytes -= parsed;
                }
            }

            // There is command, but we still wait for argument to arrive...
            if (_command_to_execute && _arg_remains > 0) {
                _logger->debug("Fill argument: {} bytes of {}", _already_read_bytes, _arg_remains);
                // There is some parsed command, and now we are reading argument
                std::size_t to_read = std::min(_arg_remains, std::size_t(_already_read_bytes));
                _argument_for_command.append(_client_buffer, to_read);

                std::memmove(_client_buffer, _client_buffer + to_read, _already_read_bytes - to_read);
                _arg_remains -= to_read;
                _already_read_bytes -= to_read;
            }

            // Thre is command & argument - RUN!
            if (_command_to_execute && _arg_remains == 0) {
                _logger->debug("Start command execution");

                std::string result;
                _command_to_execute->Execute(*_pStorage, _argument_for_command, result);

                // Save response
                result += "\r\n";

                bool add_EPOLLOUT = _answer_buf.empty();

                _answer_buf.push_back(result);
                if (add_EPOLLOUT)
                    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT;

                // Prepare for the next command
                _command_to_execute.reset();
                _argument_for_command.resize(0);
                _parser.Reset();
            }
        }
    }

    _is_alive = false;
    if (got_bytes != 0) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
  }
  catch (std::runtime_error &ex) {
      _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
  }
}

// See Connection.h
void Connection::DoWrite() {
  std::lock_guard<std::mutex> lock(_mutex);
  _logger->debug("Do write on {} socket", _socket);

  struct iovec iovecs[_answer_buf.size()];
  for (int i = 0; i < _answer_buf.size(); i++) {
      iovecs[i].iov_len = _answer_buf[i].size();
      iovecs[i].iov_base = &(_answer_buf[i][0]);
  }
  iovecs[0].iov_base = static_cast<char *>(iovecs[0].iov_base) + _cur_position;

  int written;
  if ((written = writev(_socket, iovecs, _answer_buf.size())) <= 0) {
      _is_alive = false;
      throw std::runtime_error("Failed to send response");
  }

  _cur_position += written;

  int i = 0;
  while ((i < _answer_buf.size()) && ((_cur_position - iovecs[i].iov_len) >= 0)) {
      i++;
      _cur_position -= iovecs[i].iov_len;
  }

  _answer_buf.erase(_answer_buf.begin(), _answer_buf.begin() + i);
  if (_answer_buf.empty()) {
      _event.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
  }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
