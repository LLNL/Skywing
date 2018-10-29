#ifndef SKYNET_DEVICE_HPP__
#define SKYNET_DEVICE_HPP__

#include <memory>
#include <Skynet_DeviceCommunicator.hpp>

namepsace skynet
{  
  class Device
  {
  public:
    Device(std::unique_ptr<DeviceCommunicator> comm)
      : comm(comm_)
    { }
    
    const DeviceCommunicator& get_comm() const
    { return *comm_; }

    bool get_is_live() const
    { return is_live_; }

    void send_to(void* data, int id) const
    { comm_->send_to(data, id); }

    void* receive_from(int id) const
    { return comm_->receive_from(id); }

    // CVP: Future capability, don't want to debug it now
    /*    void send_to(Serializable* data, int id)
    { comm_->send_to(data, id); }
    
    template<typename Deserializer>
    Deserializer::output_t receive_from(int id)
    { return comm_->template receive_from<Deserializer>(id)}
    */
    
  private:
    bool is_live;
    std::unique_ptr<DeviceCommunicator> comm_;
    
  }; // class Device
} // namespace skynet
