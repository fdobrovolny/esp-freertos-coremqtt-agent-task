# esp-freertos-coremqtt-agent-task

Ready-made task for usage with esp-idf of the FreeRTOS coreMQTT Agent library

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

    initMQTTAgent();
    xTaskCreate(&connectToMQTTAndStartAgent, "MQTT Agent", 10240, nullptr, 5, nullptr);
    ```

7. Before using the MQTT agent you should wait for MQTT connection by calling:
    ```c
    waitForMQTTAgentConnection();
    ```
   This will wait until the MQTT connection is established.
   Alternatively you can set the wait yourself by calling:
    ```c
    xEventGroupWaitBits(xMQTTAgentEventGroupHandle, MQTT_AGENT_CONNECTED_FLAG, false, true, portMAX_DELAY);
    ```

## Configuration

All configuration can be done via `idf.py menuconfig` in the `Component config / CoreMQTT Agent Task` section.

## Broker

The connection to broker has to be encrypted. (The `coreMQTT` in `esp-aws-iot` does not support unencrypted
connections.)

If you want to connect to the broker via IP the certificate of the broker has to have the IP as it's CN and with no alt
names. (mbed-tls does not support `IP` alt names. See [#7436](https://github.com/Mbed-TLS/mbedtls/pull/7436))

## Using Certificate authentication

1. Add your certificates with name `client.crt` and `client.key` to the `main/certs` or (`src/certs` for platformio) folder.
2. Register the certificates by adding into your main CMakeLists.txt (not project)
    ```cmake
    target_add_binary_data(${COMPONENT_TARGET} "certs/client.crt" TEXT)
    target_add_binary_data(${COMPONENT_TARGET} "certs/client.key" TEXT)
    ```
   This has to be after the following line:
    ```cmake
    idf_component_register(...)
    ```
   If you use platformio you need to add also following to your `platformio.ini`:
    ```ini
    board_build.embed_txtfiles =
        src/certs/client.crt
        src/certs/client.key
    ```
3. Enable the `idf.py menuconfig` option `Component config / CoreMQTT Agent Task / Choose authentication method` to
   `Client certificate`

## Root CA certificate

The root CA certificate is used to verify the broker certificate. If you use a public broker you can use set in the
`idf.py menuconfig` option `Component config / CoreMQTT Agent Task / Use mbedTLS root CA`. You also need to configure
mbedTLS to bundle the root CA certificates. This can be done by setting the option `Component config / mbedTLS / Certificate Bundle`.

If you want to use your own root CA see the section [How to use](#how-to-use) for instructions how to add the root CA.

## Using AWS IoT

This tutorial describes the most common config.

1. Use custom root [Starfield Root CA Certificate](https://www.amazontrust.com/repository/SFSRootCAG2.pem) as `root_cert_auth.crt`
2. Set `Component config / CoreMQTT Agent Task / Choose authentication method` to `Client certificate`
   and add your certificates as described in [Using Certificate authentication](#using-certificate-authentication)
3. Set `Component config / CoreMQTT Agent Task / Endpoint of the MQTT broker to connect to` to endpoint of your AWS IoT Core.
   (You can find it in the AWS IoT Core console under `Settings / Device data endpoint`)
4. You can also enable `Component config / CoreMQTT Agent Task / Use AWS IoT Core broker` which will report additional
   metrics to AWS IoT Core.