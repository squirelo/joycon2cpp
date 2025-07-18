#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#pragma comment(lib, "setupapi.lib")
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>
#include "JoyConDecoder.h"

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation;

constexpr uint16_t JOYCON_MANUFACTURER_ID = 1363; // Nintendo
const std::vector<uint8_t> JOYCON_MANUFACTURER_PREFIX = { 0x01, 0x00, 0x03, 0x7E };
const wchar_t* INPUT_REPORT_UUID = L"ab7de9be-89fe-49ad-828f-118f09df7fd2";
const wchar_t* WRITE_COMMAND_UUID = L"649d4ac9-8eb7-4e6c-af44-1ea54fe5f005";

void SendCustomCommands(GattCharacteristic const& characteristic)
{
    std::vector<std::vector<uint8_t>> commands = {
        { 0x0c, 0x91, 0x01, 0x02, 0x00, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 },
        { 0x0c, 0x91, 0x01, 0x04, 0x00, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 }
    };

    for (const auto& cmd : commands)
    {
        auto writer = DataWriter();
        writer.WriteBytes(cmd);
        IBuffer buffer = writer.DetachBuffer();

        auto status = characteristic.WriteValueAsync(buffer, GattWriteOption::WriteWithoutResponse).get();

        if (status == GattCommunicationStatus::Success)
        {
            std::wcout << L"Command sent successfully.\n";
        }
        else
        {
            std::wcout << L"Failed to send command.\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

struct ConnectedJoyCon {
    BluetoothLEDevice device = nullptr;
    GattCharacteristic inputChar = nullptr;
    GattCharacteristic writeChar = nullptr;
};

ConnectedJoyCon WaitForJoyCon()
{
    std::wcout << L"Looking for Right Joy-Con...\n";

    ConnectedJoyCon cj{};
    BluetoothLEDevice device = nullptr;
    bool connected = false;

    BluetoothLEAdvertisementWatcher watcher;
    std::mutex mtx;
    std::condition_variable cv;

    watcher.Received([&](auto const&, auto const& args)
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (connected) return;

            auto mfg = args.Advertisement().ManufacturerData();
            for (uint32_t i = 0; i < mfg.Size(); i++)
            {
                auto section = mfg.GetAt(i);
                if (section.CompanyId() != JOYCON_MANUFACTURER_ID) continue;
                auto reader = DataReader::FromBuffer(section.Data());
                std::vector<uint8_t> data(reader.UnconsumedBufferLength());
                reader.ReadBytes(data);
                if (data.size() >= JOYCON_MANUFACTURER_PREFIX.size() &&
                    std::equal(JOYCON_MANUFACTURER_PREFIX.begin(), JOYCON_MANUFACTURER_PREFIX.end(), data.begin()))
                {
                    device = BluetoothLEDevice::FromBluetoothAddressAsync(args.BluetoothAddress()).get();
                    if (!device) return;

                    connected = true;
                    watcher.Stop();
                    cv.notify_one();
                    return;
                }
            }
        });

    watcher.ScanningMode(BluetoothLEScanningMode::Active);
    watcher.Start();

    std::wcout << L"Scanning for Joy-Con... Please sync your RIGHT Joy-Con now.\n";
    std::wcout << L"(Waiting up to 30 seconds)\n";

    {
        std::unique_lock<std::mutex> lock(mtx);
        if (!cv.wait_for(lock, std::chrono::seconds(30), [&]() { return connected; }))
        {
            watcher.Stop();
            std::wcerr << L"Timeout: Joy-Con not found.\n";
            exit(1);
        }
    }

    cj.device = device;

    auto servicesResult = device.GetGattServicesAsync().get();
    if (servicesResult.Status() != GattCommunicationStatus::Success)
    {
        std::wcerr << L"Failed to get GATT services.\n";
        exit(1);
    }

    for (auto service : servicesResult.Services())
    {
        auto charsResult = service.GetCharacteristicsAsync().get();
        if (charsResult.Status() != GattCommunicationStatus::Success) continue;
        for (auto characteristic : charsResult.Characteristics())
        {
            if (characteristic.Uuid() == guid(INPUT_REPORT_UUID))
                cj.inputChar = characteristic;
            else if (characteristic.Uuid() == guid(WRITE_COMMAND_UUID))
                cj.writeChar = characteristic;
        }
    }

    return cj;
}

int main()
{
    init_apartment();

    std::wcout << L"Joy-Con Mouse Control\n";
    std::wcout << L"=====================\n\n";

    // Connect to right Joy-Con
    ConnectedJoyCon rightJoyCon = WaitForJoyCon();

    // Set up data callbacks
    rightJoyCon.inputChar.ValueChanged([](GattCharacteristic const&, GattValueChangedEventArgs const& args)
        {
            auto reader = DataReader::FromBuffer(args.CharacteristicValue());
            std::vector<uint8_t> buffer(reader.UnconsumedBufferLength());
            reader.ReadBytes(buffer);

            // Control mouse from Joy-Con data
            ControlMouseFromJoyCon(buffer, JoyConSide::Right, JoyConOrientation::Upright);
            
            // Optional: Also display data (comment out if you don't want console output)
            DisplayJoyConData(buffer, JoyConSide::Right, JoyConOrientation::Upright);
        });

    // Enable notifications
    auto status = rightJoyCon.inputChar.WriteClientCharacteristicConfigurationDescriptorAsync(
        GattClientCharacteristicConfigurationDescriptorValue::Notify).get();

    // Send custom commands to initialize Joy-Con
    if (rightJoyCon.writeChar)
        SendCustomCommands(rightJoyCon.writeChar);

    if (status == GattCommunicationStatus::Success)
    {
        std::wcout << L"Right Joy-Con connected successfully!\n\n";
        std::wcout << L"Mouse control is now active:\n";
        std::wcout << L"- Move Joy-Con to control mouse cursor\n";
        std::wcout << L"- A button = Left click\n";
        std::wcout << L"- B button = Right click\n";
        std::wcout << L"- Data display below (press Enter to exit):\n\n";
    }
    else
    {
        std::wcout << L"Failed to enable notifications.\n";
        return 1;
    }

    // Wait for user input to exit
    std::wstring dummy;
    std::getline(std::wcin, dummy);

    std::wcout << L"\nExiting...\n";
    return 0;
}
