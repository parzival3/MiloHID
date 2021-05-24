#include "hid_explorer.h"

// get hid devices
// To get the hid devices we need to open the hid_manager and get the list of
// devices most likely we need separated thread to detect the changes in the
// devices The idea is to keep the thread runnig and we need some sincronization
// like for example a queue that list all the devices
// Let's call this class the enumerator
namespace hid {
void Enumerator::deviceAddedCallBack([[maybe_unused]] void *context,
                                     [[maybe_unused]] IOReturn result,
                                     [[maybe_unused]] void *sender,
                                     [[maybe_unused]] IOHIDDeviceRef device) {
  std::cout << "New device added\n";
  auto enumerator = static_cast<Enumerator *>(context);
  auto vid = enumerator->get_int_property(device, CFSTR(kIOHIDVendorIDKey));
  if (!vid.has_value()) {
    std::cerr << "ERROR: coulden't get VID for current device\n";
    std::cerr << "ERROR: no device added\n";
    return;
  }

  auto pid = enumerator->get_int_property(device, CFSTR(kIOHIDProductIDKey));
  if (!pid.has_value()) {
    std::cerr << "ERROR: coulden't get PID for current device\n";
    std::cerr << "ERROR: no device added\n";
    return;
  }

  auto device_descriptor = enumerator->get_device_descriptor(device);
  if (!device_descriptor.has_value()) {
    std::cerr << "ERROR: couldn't get the device descriptor\n";
    std::cerr << "ERROR: no device added\n";
  }

  auto path = enumerator->get_device_path(device);
  if (!path.has_value()) {
    std::cerr << "ERROR: couldn't get the device path\n";
    std::cerr << "ERROR: no device added\n";
  }

  auto manufacturer =
      enumerator->get_str_property(device, CFSTR(kIOHIDManufacturerKey));
  auto product = enumerator->get_str_property(device, CFSTR(kIOHIDProductKey));
  auto serial =
      enumerator->get_str_property(device, CFSTR(kIOHIDSerialNumberKey));

  enumerator->devices.emplace_back(device, vid.value(), pid.value(),
                                   path.value(), device_descriptor.value(),
                                   product, manufacturer, serial);
}

[[nodiscard]] std::optional<std::string>
Enumerator::get_device_path(const IOHIDDeviceRef &dev) const noexcept {
  io_service_t service = IOHIDDeviceGetService(dev);
  io_string_t path;
  IORegistryEntryGetPath(service, kIOServicePlane, path);
  if (path[0] == '\0') {
    return std::nullopt;
  }

  return std::move(path);
}

void Enumerator::deviceRemovedCallBack(
    [[maybe_unused]] void *context, [[maybe_unused]] IOReturn result,
    [[maybe_unused]] void *sender,
    [[maybe_unused]] IOHIDDeviceRef removed_device) {
  std::cout << "Device removed\n";
  auto enumerator = static_cast<Enumerator *>(context);
  auto number =
      std::erase_if(enumerator->devices, [&removed_device](const auto &hdev) {
        return hdev.device == removed_device;
      });

  if (number == 0) {
    std::cerr << "ERROR: device removed not found in the list of devices\n";
  }
}

[[nodiscard]] std::optional<int32_t>
Enumerator::get_int_property(const IOHIDDeviceRef &dev,
                             const CFStringRef &property) const noexcept {
  int32_t value = 0;
  if (CFTypeRef ref = IOHIDDeviceGetProperty(dev, property);
      ref && CFGetTypeID(ref) == CFNumberGetTypeID()) {
    CFNumberGetValue(CFNumberRef(ref), kCFNumberSInt32Type, &value);
    return value;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::string>
Enumerator::get_str_property(const IOHIDDeviceRef &dev,
                             const CFStringRef &property) const noexcept {

  if (CFTypeRef ref = IOHIDDeviceGetProperty(dev, property);
      ref && CFGetTypeID(ref) == CFStringGetTypeID()) {
    CFStringRef str = static_cast<CFStringRef>(ref);

    CFIndex len = CFStringGetLength(str);
    CFRange range{.location = 0, .length = len};

    CFIndex used_buf_len;

    std::string buffer(static_cast<unsigned long>(len), '\0');
    CFStringGetBytes(str, range, kCFStringEncodingUTF32LE, '?', FALSE,
                     reinterpret_cast<UInt8 *>(buffer.data()), len,
                     &used_buf_len);
    return std::move(buffer);
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::vector<uint32_t>>
Enumerator::get_device_descriptor(const IOHIDDeviceRef &dev) const noexcept {
  std::vector<uint32_t> descriptor;
  CFTypeRef ref = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDReportDescriptorKey));
  if (ref && CFGetTypeID(ref) == CFDataGetTypeID()) {
    CFDataRef data = CFDataRef(ref);
    auto desc_size = size_t(CFDataGetLength(data));
    if (descriptor.max_size() > desc_size) {
      descriptor.resize(desc_size);
    }
    std::copy(CFDataGetBytePtr(data), CFDataGetBytePtr(data) + desc_size,
              std::back_inserter(descriptor));
    return std::move(descriptor);
  }
  return std::nullopt;
}

Enumerator::Enumerator() {
  /*
   *
   * Creates an IOHIDManager object.
   * The IOHIDManager object is meant as a global management system
   * for communicating with HID devices.
   * allocator Allocator to be used during creation.
   * options Supply @link kIOHIDManagerOptionUsePersistentProperties @/link to
   * load properties from the default persistent property store. Otherwise
   * supply kIOHIDManagerOptionNone @/link (or 0). Returns a new
   * IOHIDManagerRef.
   */
  manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone);

  /*! IOHIDManagerSetDeviceMatching
      Sets matching criteria for device enumeration.
      Matching keys are prefixed by kIOHIDDevice and declared in
      <IOKit/hid/IOHID4Keys.h>.  Passing a NULL dictionary will result
      in all devices being enumerated. Any subsequent calls will cause
      the hid manager to release previously enumerated devices and
      restart the enuerate process using the revised criteria.  If
      interested in multiple, specific device classes, please defer to
      using IOHIDManagerSetDeviceMatchingMultiple.
      If a dispatch queue is set, this call must occur before activation.
      manager Reference to an IOHIDManager.
      matching CFDictionaryRef containg device matching criteria.
     */
  IOHIDManagerSetDeviceMatching(manager, nullptr);
  /*!
      Opens the IOHIDManager.
      This will open both current and future devices that are
      enumerated. To establish an exclusive link use the
      kIOHIDOptionsTypeSeizeDevice option. Manager Reference to an IOHIDManager.
      options Option bits to be sent down to the manager and device.
      Returns kIOReturnSuccess if successful.
  */

  IOReturn value = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
  if (value != kIOReturnSuccess)
    throw hid::HIDException<IOReturn>{
        value, "There was an error opening the device manager"};

  /* Now we should register a callback for when some devices are added
     IOHIDManagerRegisterDeviceMatchingCallback
     Registers a callback to be used a device is enumerated.
     Only device matching the set criteria will be enumerated.
     If a dispatch queue is set, this call must occur before activation.
     Devices provided in the callback will be scheduled with the same
     runloop/dispatch queue as the IOHIDManagerRef, and should not be
     rescheduled.
    */
  IOHIDManagerRegisterDeviceMatchingCallback(
      manager, Enumerator::deviceAddedCallBack, this);
  /* Now we should register a remove callback
   *
   * */
  IOHIDManagerRegisterDeviceRemovalCallback(
      manager, Enumerator::deviceRemovedCallBack, this);
}

Enumerator::~Enumerator() {
  if (manager == nullptr) {
    std::cout << "INFO: No HIDManager was open\n";
  } else {
    IOReturn result = IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    if (result != kIOReturnSuccess)
      std::cerr
          << "ERROR: Couldn't close the device manager, the error value is "
          << result << '\n';
  }
}

void Enumerator::enumeration_loop(const std::stop_token &st) {
  if (manager == nullptr) {
    std::cerr << "ERROR: The HIDMananger is not open" << __FUNCTION__ << '\n';
    return;
  }
  // Lets associate the enumerator with the current loop

  /*
   * IOHIDManagerScheduleWithRunLoop
   * Schedules HID manager with run loop.
   * Formally associates manager with client's run loop. Scheduling
   * this device with the run loop is necessary before making use of
   * any asynchronous APIs.  This will propagate to current and
   * future devices that are enumerated.
   */
  IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(),
                                  kCFRunLoopDefaultMode);
  while (!st.stop_requested()) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  // When the thread is stopped let's remove the callbacks and remove the loop
  IOHIDManagerRegisterDeviceMatchingCallback(manager, nullptr, nullptr);
  IOHIDManagerRegisterDeviceRemovalCallback(manager, nullptr, nullptr);

  IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetCurrent(),
                                    kCFRunLoopDefaultMode);
  if (manager == nullptr) {
    std::cout << "INFO: HIDManager is now null\n";
  }
}

/*
 * Start the thread enumeration
 * We can pass stop_token to jthread in order to check if the thread needs to
 * stopped instead of using a condition_variable and checking it every few
 * milliseconds
 */
void Enumerator::start_device_enumeartion_loop() {
  enumerator_thread =
      std::jthread{[this](std::stop_token st) { this->enumeration_loop(st); }};
}

} // namespace hid

template <size_t S> static void getkey(std::array<char, S> &input) {
  unsigned long i = 0;
  auto t = '\0';
  input.fill('\0');

  do {
    t = char(getchar());
    if (t == '\n' || t == EOF)
      break;
    input[i] = t;
    ++i;
  } while (i < input.size() - 1);

  input[i] = '\0';
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  std::array<char, MAXIO_SIZE> input;
  hid::Enumerator enumerator{};
  enumerator.start_device_enumeartion_loop();

  do {
    std::cout << "Type 'd' to list the devices or 'q' to quit the program\n";
    getkey(input);
  } while (input[0] != 'q' && input[0] != 'Q' && input[1] == '\0');
  return 0;
}
