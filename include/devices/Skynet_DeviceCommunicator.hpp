#ifndef SKYNET_DEVICECOMMUNICATOR_HPP__
#define SKYNET_DEVICECOMMUNICATOR_HPP__

namespace skynet
{
  class DeviceCommunicator
  {
  public:

    virtual void send_to(void* data, int id) const = 0;
    virtual void* receive_from(int id) const = 0;
    
    // CVP: Future capability, don't want to debug it now.
    /*
    void send_to(Serializable* data, int id)
    {
      send_to(data->serialize(), id);
    }

    template<typename Deserializer>
    Deserializer::output_t receive_from(int id)
    {
      return Deserializer::deserialize(receive_from(id));
    }
    */
  }; // class DeviceCommunicator

} // namespace skynet
