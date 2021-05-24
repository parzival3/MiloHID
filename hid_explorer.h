#ifndef HID_EXPLORER_H_
#define HID_EXPLORER_H_

#include <iostream>
#include <string>

#include "jthread.hpp"
#include "stop_token.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/usb/USB.h>
#include <array>
#include <chrono>
#include <exception>
#include <list>
#include <mutex>
#include <optional>
#include <string.h>
#include <thread>
#include <vector>

namespace hid {
#define MAXIO_SIZE 4

struct HIDDevice {
  const IOHIDDeviceRef device;
  const uint32_t vid;
  const uint32_t pid;
  const std::string path;
  const std::vector<uint32_t> device_descriptor;
  const std::optional<std::string> product;
  const std::optional<std::string> manufacturer;
  const std::optional<std::string> serial;

  HIDDevice(IOHIDDeviceRef _device, uint32_t _vid, uint32_t _pid,
            std::string _path, std::vector<uint32_t> _device_descriptor,
            std::optional<std::string> _product,
            std::optional<std::string> _manufacturer,
            std::optional<std::string> _serial)
      : device{std::move(_device)}, vid{_vid}, pid{_pid}, path{std::move(
                                                              _path)},
        device_descriptor{std::move(_device_descriptor)}, product{std::move(
                                                              _product)},
        manufacturer{std::move(_manufacturer)}, serial{std::move(_serial)} {}
};

template <class ErrorType> class HIDException : public std::exception {
private:
  ErrorType eType;
  std::string eMessage;

public:
  HIDException(ErrorType eType_p, std::string message_p)
      : eType{eType_p}, eMessage{message_p} {}
  ErrorType type() const { return eType; }
  std::string message() const { return eMessage; }
};

class Enumerator {
public:
  Enumerator();
  ~Enumerator();
  // TODO: need to implement a fiter
  void enumeration_loop(const std::stop_token &st);
  // TODO: pass the filter here
  void start_device_enumeartion_loop();

  [[nodiscard]] std::optional<int32_t>
  get_int_property(const IOHIDDeviceRef &dev,
                   const CFStringRef &property) const noexcept;
  [[nodiscard]] std::optional<std::string>
  get_str_property(const IOHIDDeviceRef &dev,
                   const CFStringRef &property) const noexcept;
  [[nodiscard]] std::optional<std::vector<uint32_t>>
  get_device_descriptor(const IOHIDDeviceRef &dev) const noexcept;
  [[nodiscard]] std::optional<std::string>
  get_device_path(const IOHIDDeviceRef &dev) const noexcept;

private:
  std::list<HIDDevice> devices;
  IOHIDManagerRef manager;
  static void deviceAddedCallBack(void *context, IOReturn result, void *sender,
                                  IOHIDDeviceRef device);
  static void deviceRemovedCallBack(void *context, IOReturn result,
                                    void *sender, IOHIDDeviceRef device);
  std::jthread enumerator_thread;
};

} // namespace hid

#endif // HID_EXPLORER_H_
