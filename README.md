# esp-freertos-coremqtt-agent-task

Ready made task for usage with esp-idf of the FreeRTOS coreMQTT Agent library

## How to use

1. Clone this repository into your components folder
2. Clone [`esp-aws-iot`](https://github.com/espressif/esp-aws-iot) branch `release/202210.01-LTS` into your root folder.
3. Enable the `esp-aws-iot` by adding into your project `CMakeLists.txt`:
    ```cmake
    list(APPEND EXTRA_COMPONENT_DIRS esp-aws-iot/libraries)
    ```
4. Add root CA certificate named `root_cert_auth.crt` to the `main/certs` or (`src/certs` for platformio) folder.
5. Register the certificate by adding into your main CMakeLists.txt (not project)
    ```cmake
    target_add_binary_data(${COMPONENT_TARGET} "certs/root_cert_auth.crt" TEXT)
    ```
   This has to be after the following line:
    ```cmake
    idf_component_register(...)
    ```
   If you use platformio you need to add also following to your `platformio.ini`:
    ```ini
    board_build.embed_txtfiles =
        src/certs/root_cert_auth.crt
    ```
6. After connection to Wi-Fi you can start the task by calling:
    ```c
    #include "esp_freertos_coremqtt_agent_task.h"   

    xTaskCreate(&connectToMQTTAndStartAgent, "MQTT Agent", 10240, nullptr, 5, nullptr);
    ```

## Configuration

All configuration can be done via `idf.py menuconfig` in the `Component config / CoreMQTT Agent Task` section.

## Broker

The connection to broker has to be encrypted. (The `coreMQTT` in `esp-aws-iot` does not support unencrypted
connections.)

If you want to connect to the broker via IP the certificate of the broker has to have the IP as it's CN and with no alt
names. (mbed-tls does not support `IP` alt names.)