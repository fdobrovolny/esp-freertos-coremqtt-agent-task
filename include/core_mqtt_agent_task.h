/*
 * This file is based on esp-aws-iot
 * https://github.com/espressif/esp-aws-iot/blob/d37fd63002b4fda99523e1ac4c9822fce485e76d/examples/thing_shadow/main/demo_config.h
 *
 * AWS IoT Device SDK for Embedded C 202103.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef CORE_MQTT_AGENT_TASK_H
#define CORE_MQTT_AGENT_TASK_H

/**
 * @brief Details of the MQTT broker to connect to.
 *
 * @note Your AWS IoT Core endpoint can be found in the AWS IoT console under
 * Settings/Custom Endpoint, or using the describe-endpoint API.
 *
 */
#ifndef MQTT_BROKER_ENDPOINT
#define MQTT_BROKER_ENDPOINT    CONFIG_MQTT_BROKER_ENDPOINT
#endif

/**
 * @brief Length of MQTT server host name.
 */
#define MQTT_BROKER_ENDPOINT_LENGTH         ( ( uint16_t ) ( sizeof( MQTT_BROKER_ENDPOINT ) - 1 ) )

/**
 * @brief AWS IoT MQTT broker port number.
 *
 * In general, port 8883 is for secured MQTT connections.
 *
 * @note Port 443 requires use of the ALPN TLS extension with the ALPN protocol
 * name. When using port 8883, ALPN is not required.
 */
#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT    ( CONFIG_MQTT_BROKER_PORT )
#endif

/**
 * @brief MQTT client identifier.
 *
 * No two clients may use the same client identifier simultaneously.
 * Fallback to CONFIG_LWIP_LOCAL_HOSTNAME if not defined.
 */
#ifndef MQTT_CLIENT_IDENTIFIER
#ifndef CONFIG_MQTT_CLIENT_IDENTIFIER
#ifdef CONFIG_LWIP_LOCAL_HOSTNAME
// Use hostname as identifier if not defined
#define MQTT_CLIENT_IDENTIFIER CONFIG_LWIP_LOCAL_HOSTNAME
#else
#error "Please define a unique client identifier, CONFIG_MQTT_CLIENT_IDENTIFIER, in menuconfig"
#endif
#else
#define MQTT_CLIENT_IDENTIFIER CONFIG_MQTT_CLIENT_IDENTIFIER
#endif
#endif


/**
 * @brief Length of client identifier.
 */
#define MQTT_CLIENT_IDENTIFIER_LENGTH        ( ( uint16_t ) ( sizeof( MQTT_CLIENT_IDENTIFIER ) - 1 ) )


/* The AWS IoT message broker requires either a set of client certificate/private key
 * or username/password to authenticate the client. */
#ifdef CONFIG_MQTT_CLIENT_USERNAME
/* If a username is defined, a client password also would need to be defined for
 * client authentication. */
#ifndef CONFIG_MQTT_CLIENT_PASSWORD
#error "Please define client password(CONFIG_MQTT_CLIENT_PASSWORD) in menuconfig for client authentication based on username/password."
#endif
/* AWS IoT MQTT broker port needs to be 443 for client authentication based on
 * username/password. */
#if CONFIG_MQTT_BROKER_PORT != 443
#warning "Broker port, CONFIG_MQTT_BROKER_PORT, should be defined as 443 in menuconfig for client authentication based on username/password for AWS IoT."
#endif
#else /* !CONFIG_MQTT_CLIENT_USERNAME */
#ifndef CONFIG_MQTT_USE_ESP_SECURE_CERT_MGR
extern const char client_cert_start[] asm("_binary_client_crt_start");
extern const char client_cert_end[] asm("_binary_client_crt_end");
extern const char client_key_start[] asm("_binary_client_key_start");
extern const char client_key_end[] asm("_binary_client_key_end");
#endif /* CONFIG_MQTT_USE_ESP_SECURE_CERT_MGR */
#endif /* CONFIG_MQTT_CLIENT_USERNAME */

/**
 * @brief Dimensions the buffer used to serialize and deserialize MQTT packets.
 * @note Specified in bytes.  Must be large enough to hold the maximum
 * anticipated MQTT payload.
 */
#ifndef CONFIG_MQTT_AGENT_NETWORK_BUFFER_SIZE
#define MQTT_AGENT_NETWORK_BUFFER_SIZE    ( 5000 )
#else
#define MQTT_AGENT_NETWORK_BUFFER_SIZE    ( CONFIG_MQTT_AGENT_NETWORK_BUFFER_SIZE )
#endif

/**
 * @brief ALPN (Application-Layer Protocol Negotiation) protocol name for AWS IoT MQTT.
 *
 * This will be used if the AWS_MQTT_PORT is configured as 443 for AWS IoT MQTT broker.
 * Please see more details about the ALPN protocol for AWS IoT MQTT endpoint
 * in the link below.
 * https://aws.amazon.com/blogs/iot/mqtt-with-tls-client-authentication-on-port-443-why-it-is-useful-and-how-it-works/
 */
/**
 * @brief The ALPN protocol name for AWS IoT MQTT connections.
 *
 * This will be used if the port is 443.
 */
#ifndef MQTT_ALPN_PROTOCOL_NAME
#define MQTT_ALPN_PROTOCOL_NAME CONFIG_MQTT_ALPN_PROTOCOL_NAME
#endif

/**
 * @brief Length of ALPN protocol name.
 */
#define CONFIG_MQTT_ALPN_PROTOCOL_NAME_LENGTH        ( ( uint16_t ) ( sizeof( MQTT_ALPN_PROTOCOL_NAME ) - 1 ) )

/**
 * @brief The name of the operating system that the application is running on.
 * The current value is given as an example. Please update for your specific
 * operating system.
 */
#define OS_NAME                   "FreeRTOS"

/**
 * @brief The version of the operating system that the application is running
 * on. The current value is given as an example. Please update for your specific
 * operating system version.
 */
#define OS_VERSION                tskKERNEL_VERSION_NUMBER

/**
 * @brief The name of the hardware platform the application is running on. The
 * current value is given as an example. Please update for your specific
 * hardware platform.
 */
#define HARDWARE_PLATFORM_NAME    CONFIG_HARDWARE_PLATFORM_NAME

/**
 * @brief The name of the MQTT library used and its version, following an "@"
 * symbol.
 */
#include "core_mqtt.h"

#define MQTT_LIB    "core-mqtt@" MQTT_LIBRARY_VERSION

/**
 * @brief The MQTT metrics string expected by AWS IoT.
 */
#define AWS_IOT_METRICS_STRING                                 \
    "?SDK=" OS_NAME "&Version=" OS_VERSION \
    "&Platform=" HARDWARE_PLATFORM_NAME "&MQTTLib=" MQTT_LIB

/**
 * @brief The length of the MQTT metrics string expected by AWS IoT.
 */
#define AWS_IOT_METRICS_STRING_LENGTH    ( ( uint16_t ) ( sizeof( AWS_IOT_METRICS_STRING ) - 1 ) )

#ifdef CONFIG_MQTT_CLIENT_USERNAME

/**
 * @brief Append the username with the metrics string if #MQTT_CLIENT_USERNAME is defined.
 *
 * This is to support both metrics reporting and username/password based client
 * authentication by AWS IoT.
 */
#define MQTT_CLIENT_USERNAME_WITH_METRICS    CONFIG_MQTT_CLIENT_USERNAME AWS_IOT_METRICS_STRING
#endif


/**
 * @brief Flag to connect to the AWS IoT Core MQTT broker.
 *
 * Set to pdFALSE to connect to the MQTT broker of your choice.
 *
 * This affects AWS IoT Core specific features such as metrics reporting. (AWS_IOT_METRICS_STRING)
 */
#ifndef USE_AWS_IOT_CORE_BROKER
#ifdef CONFIG_MQTT_USE_AWS_IOT_CORE_BROKER
#define USE_AWS_IOT_CORE_BROKER    CONFIG_MQTT_USE_AWS_IOT_CORE_BROKER
#else
#define USE_AWS_IOT_CORE_BROKER   0
#endif
#endif

/* ------------------------------------- */

/**
 * @brief The maximum number of retries for connecting to server.
 */
#ifndef CONFIG_MQTT_CONNECTION_RETRY_MAX_ATTEMPTS
#define CONNECTION_RETRY_MAX_ATTEMPTS            ( 5U )
#else
#define CONNECTION_RETRY_MAX_ATTEMPTS            ( CONFIG_MQTT_CONNECTION_RETRY_MAX_ATTEMPTS )
#endif

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying connection to server.
 */
#ifndef CONFIG_MQTT_CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( 5000U )
#else
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( CONFIG_MQTT_CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS )
#endif

/**
 * @brief The base back-off delay (in milliseconds) to use for connection retry attempts.
 */
#ifndef CONFIG_MQTT_CONNECTION_RETRY_BACKOFF_BASE_MS
#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( 500U )
#else
#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( CONFIG_MQTT_CONNECTION_RETRY_BACKOFF_BASE_MS )
#endif

/**
 * @brief The maximum time interval in seconds which is allowed to elapse
 *  between two Control Packets.
 *
 *  It is the responsibility of the Client to ensure that the interval between
 *  Control Packets being sent does not exceed the this Keep Alive value. In the
 *  absence of sending any other Control Packets, the Client MUST send a
 *  PINGREQ Packet.
 */
#ifndef CONFIG_MQTT_KEEP_ALIVE_INTERVAL_SECONDS
#define KEEP_ALIVE_INTERVAL_SECONDS       ( 60U )
#else
#define KEEP_ALIVE_INTERVAL_SECONDS       ( CONFIG_MQTT_KEEP_ALIVE_INTERVAL_SECONDS )
#endif

/**
 * @brief Timeout for receiving CONNACK after sending an MQTT CONNECT packet.
 * Defined in milliseconds.
 */
#ifndef CONFIG_MQTT_CONNACK_RECV_TIMEOUT_MS
#define CONNACK_RECV_TIMEOUT_MS           ( 2000U )
#else
#define CONNACK_RECV_TIMEOUT_MS           ( CONFIG_MQTT_CONNACK_RECV_TIMEOUT_MS )
#endif

#ifndef CONFIG_MQTT_AGENT_COMMAND_QUEUE_LENGTH
#define MQTT_AGENT_COMMAND_QUEUE_LENGTH              ( 25 )
#else
#define MQTT_AGENT_COMMAND_QUEUE_LENGTH              ( CONFIG_MQTT_AGENT_COMMAND_QUEUE_LENGTH )
#endif

/* ------------------------------------- */

#include "freertos/event_groups.h"

static EventGroupHandle_t xMQTTAgentEventGroupHandle;

/*
 * @brief xMQTTAgentEventGroup flags.
 */
#define MQTT_AGENT_CONNECTED_FLAG  ( 1 << 0 )


#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Connect to MQTT broker and start MQTT agent.
 * */
void connectToMQTTAndStartAgent(void *pvParameters);

/*
 * @brief Blocking wait for MQTT connection to be established.
 * */
void waitForMQTTAgentConnection();

/*
 * @brief Initialize MQTT agent.
 *
 * Currently used to initialize Event Group for MQTT connection.
 */
void initMQTTAgent();


#ifdef __cplusplus
}
#endif
#endif //CORE_MQTT_AGENT_TASK_H
