#ifndef SKYNET_SOCKETCOMMUNICATOR_HPP__
#define SKYNET_SOCKETCOMMUNICATOR_HPP__

#include <Skynet_DeviceCommunicator.hpp>

#include <string>
#include <sys.socket.h>

namespace skynet
{

  class SocketCommunicator : public DeviceCommunicator
  {
  public:
    SocketCommunicator(std::string ip_address)
      : ip_address_(ip_address)
    {
      // Need to implement peer-to-peer socket communications here.
    }

  private:
    std::string ip_address_;
    int sock_;
  }; // class SocketCommunicator

} // namespace skynet
