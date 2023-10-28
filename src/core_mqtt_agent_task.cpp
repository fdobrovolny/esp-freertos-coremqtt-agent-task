/*
 * This file is mostly based on the FreeRTOS coreMQTT-Agent-Demos
 * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/mqtt-agent-task.c
 *
 * Lab-Project-coreMQTT-Agent 201215
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
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * and esp-aws-iot
 * https://github.com/espressif/esp-aws-iot/blob/d37fd63002b4fda99523e1ac4c9822fce485e76d/examples/thing_shadow/main/shadow_demo_main.c
 * https://github.com/espressif/esp-aws-iot/blob/d37fd63002b4fda99523e1ac4c9822fce485e76d/examples/thing_shadow/main/shadow_demo_helpers.c
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

/* Kernel includes. */
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"

/* MQTT Agent ports. */
#include "freertos_agent_message.h"
#include "freertos_command_pool.h"

/* Exponential backoff retry include. */
#include "backoff_algorithm.h"

/* OpenSSL sockets transport implementation. */
#include "network_transport.h"

#include "esp_random.h"
#include "esp_log.h"
#include "core_mqtt_agent_subs_manager.h"

#include "core_mqtt_agent_task.h"

#ifdef CONFIG_MQTT_USE_ESP_SECURE_CERT_MGR
#include "esp_secure_cert_read.h"
#endif

extern const char root_cert_auth_start[]   asm("_binary_root_cert_auth_crt_start");
extern const char root_cert_auth_end[]   asm("_binary_root_cert_auth_crt_end");

/*-----------------------------------------------------------*/

/**
 * @brief Static buffer for TLS Context Semaphore.
 */
static StaticSemaphore_t xTlsContextSemaphoreBuffer;

/**
 * @brief The network context used for Openssl operation.
 */
static NetworkContext_t networkContext = {nullptr};

MQTTAgentContext_t xGlobalMqttAgentContext;

static uint8_t xNetworkBuffer[MQTT_AGENT_NETWORK_BUFFER_SIZE];

static MQTTAgentMessageContext_t xCommandQueue;

/**
 * @brief The global array of subscription elements.
 *
 * @note No thread safety is required to this array, since the updates the array
 * elements are done only from one task at a time. The subscription manager
 * implementation expects that the array of the subscription elements used for
 * storing subscriptions to be initialized to 0. As this is a global array, it
 * will be initialized to 0 by default.
 */
SubscriptionElement_t xGlobalSubscriptionList[SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS];

/**
 * @brief Logging tag.
 */
static const char *TAG = "MQTT";

/*-----------------------------------------------------------*/

/**
 * @brief The timer query function provided to the MQTT context.
 *
 * @return Time in milliseconds.
 */
static uint32_t prvGetTimeMs();


/**
 * @brief Fan out the incoming publishes to the callbacks registered by different
 * tasks. If there are no callbacks registered for the incoming publish, it will be
 * passed to the unsolicited publish handler.
 *
 * @param[in] pMqttAgentContext Agent context.
 * @param[in] packetId Packet ID of publish.
 * @param[in] pxPublishInfo Info of incoming publish.
 */
static void prvIncomingPublishCallback(MQTTAgentContext_t *pMqttAgentContext,
                                       uint16_t packetId,
                                       MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief Connect to MQTT broker with reconnection retries.
 *
 * If connection fails, retry is attempted after a timeout.
 * Timeout value will exponentially increase until maximum
 * timeout value is reached or the number of attempts are exhausted.
 *
 * @param[out] pNetworkContext The output parameter to return the created network context.
 *
 * @return EXIT_FAILURE on failure; EXIT_SUCCESS on successful connection.
 */
static int connectToServerWithBackoffRetries(NetworkContext_t *pNetworkContext);

/**
 * @brief Initializes an MQTT context, including transport interface and
 * network buffer.
 *
 * @return `MQTTSuccess` if the initialization succeeds, else `MQTTBadParameter`.
 */
static MQTTStatus_t prvMQTTInit();

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when the
 * broker ACKs the SUBSCRIBE message. This callback implementation is used for
 * handling the completion of resubscribes. Any topic filter failed to resubscribe
 * will be removed from the subscription list.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in] pxReturnInfo The result of the command.
 */
static void prvSubscriptionCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                           MQTTAgentReturnInfo_t *pxReturnInfo);

/**
 * @brief Function to attempt to resubscribe to the topics already present in the
 * subscription list.
 *
 * This function will be invoked when this demo requests the broker to
 * reestablish the session and the broker cannot do so. This function will
 * enqueue commands to the MQTT Agent queue and will be processed once the
 * command loop starts.
 *
 * @return `MQTTSuccess` if adding subscribes to the command queue succeeds, else
 * appropriate error code from MQTTAgent_Subscribe.
 * */
static MQTTStatus_t prvHandleResubscribe();

/**
 * @brief Sends an MQTT Connect packet over the already connected TCP socket.
 *
 * @param[in] pxMQTTContext MQTT context pointer.
 * @param[in] xCleanSession If a clean session should be established.
 *
 * @return `MQTTSuccess` if connection succeeds, else appropriate error code
 * from MQTT_Connect.
 */
static MQTTStatus_t prvMQTTConnect(bool xCleanSession);

/**
 * @brief Connects a TCP socket to the MQTT broker, then creates and MQTT
 * connection to the same.
 */
static void prvConnectToMQTTBroker();

/**
 * @brief Task used to run the MQTT agent.  In this example the first task that
 * is created is responsible for creating all the other demo tasks.  Then,
 * rather than create prvMQTTAgentTask() as a separate task, it simply calls
 * prvMQTTAgentTask() to become the agent task itself.
 *
 * This task calls MQTTAgent_CommandLoop() in a loop, until MQTTAgent_Terminate()
 * is called. If an error occurs in the command loop, then it will reconnect the
 * TCP and MQTT connections.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation. Not
 * used in this example.
 */
static void prvMQTTAgentTask(void *pvParameters);

/*-----------------------------------------------------------*/

static uint32_t generateRandomNumber() {
    return esp_random();
}

uint32_t prvGetTimeMs() {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return (uint32_t) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/*-----------------------------------------------------------*/

static void prvIncomingPublishCallback(MQTTAgentContext_t *pMqttAgentContext,
                                       uint16_t packetId,
                                       MQTTPublishInfo_t *pxPublishInfo) {
    bool xPublishHandled = false;
    char cOriginalChar, *pcLocation;

    (void) packetId;

    /* Fan out the incoming publishes to the callbacks registered using
     * subscription manager. */
    xPublishHandled = handleIncomingPublishes((SubscriptionElement_t *) pMqttAgentContext->pIncomingCallbackContext,
                                              pxPublishInfo);

    /* If there are no callbacks to handle the incoming publishes,
     * handle it as an unsolicited publish. */
    if (xPublishHandled != true) {
        /* Ensure the topic string is terminated for printing.  This will over-
         * write the message ID, which is restored afterwards. */
        pcLocation = (char *) &(pxPublishInfo->pTopicName[pxPublishInfo->topicNameLength]);
        cOriginalChar = *pcLocation;
        *pcLocation = 0x00;
        ESP_LOGW(TAG, "WARN:  Received an unsolicited publish from topic %s", pxPublishInfo->pTopicName);
        *pcLocation = cOriginalChar;
    }
}

static int connectToServerWithBackoffRetries(NetworkContext_t *pNetworkContext) {
    /* Based on
     * https://github.com/espressif/esp-aws-iot/blob/d37fd63002b4fda99523e1ac4c9822fce485e76d/examples/thing_shadow/main/shadow_demo_helpers.c#L371-L485
     */
    int returnStatus = EXIT_SUCCESS;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    TlsTransportStatus_t tlsStatus = TLS_TRANSPORT_SUCCESS;
    BackoffAlgorithmContext_t reconnectParams;
    pNetworkContext->pcHostname = MQTT_BROKER_ENDPOINT;
    pNetworkContext->xPort = MQTT_BROKER_PORT;
    pNetworkContext->pxTls = nullptr;
    pNetworkContext->xTlsContextSemaphore = xSemaphoreCreateMutexStatic(&xTlsContextSemaphoreBuffer);

    pNetworkContext->disableSni = 0;
    uint16_t nextRetryBackOff;
    struct timespec tp;

    /* Initialize credentials for establishing TLS session. */
    pNetworkContext->pcServerRootCA = root_cert_auth_start;
    pNetworkContext->pcServerRootCASize = root_cert_auth_end - root_cert_auth_start;

#ifdef CONFIG_MQTT_USE_SECURE_ELEMENT
    pNetworkContext->use_secure_element = true;

#elif defined(CONFIG_MQTT_USE_ESP_SECURE_CERT_MGR)
    if (esp_secure_cert_get_device_cert(&pNetworkContext->pcClientCert, &pNetworkContext->pcClientCertSize) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to obtain flash address of device cert");
        return EXIT_FAILURE;
    }
#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
    pNetworkContext->ds_data = esp_secure_cert_get_ds_ctx();
    if (pNetworkContext->ds_data == NULL) {
        ESP_LOGE(TAG, "Failed to obtain the ds context");
        return EXIT_FAILURE;
    }
#else /* !CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
    if (esp_secure_cert_get_priv_key(&pNetworkContext->pcClientKey, &pNetworkContext->pcClientKeySize) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to obtain flash address of private_key");
        return EXIT_FAILURE;
    }
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

#else /* !CONFIG_MQTT_USE_SECURE_ELEMENT && !CONFIG_MQTT_USE_ESP_SECURE_CERT_MGR  */
#ifndef CONFIG_MQTT_CLIENT_USERNAME
    pNetworkContext->pcClientCert = client_cert_start;
    pNetworkContext->pcClientCertSize = client_cert_end - client_cert_start;
    pNetworkContext->pcClientKey = client_key_start;
    pNetworkContext->pcClientKeySize = client_key_end - client_key_start;
#endif
#endif
#ifdef CONFIG_MQTT_ALPN_PROTOCOL_NAME
    if( CONFIG_MQTT_BROKER_PORT == 443 )
    {
        /* Pass the ALPN protocol name depending on the port being used.
         * Please see more details about the ALPN protocol for AWS IoT MQTT endpoint
         * in the link below.
         * https://aws.amazon.com/blogs/iot/mqtt-with-tls-client-authentication-on-port-443-why-it-is-useful-and-how-it-works/
         */
        static const char * pcAlpnProtocols[] = { NULL, NULL };
        pcAlpnProtocols[0] = CONFIG_MQTT_ALPN_PROTOCOL_NAME;
        pNetworkContext->pAlpnProtos = pcAlpnProtocols;
    } else {
        pNetworkContext->pAlpnProtos = nullptr;
    }
#else
    pNetworkContext->pAlpnProtos = nullptr;
#endif

    /* Seed pseudo random number generator used in the demo for
     * backoff period calculation when retrying failed network operations
     * with broker. */

    /* Get current time to seed pseudo random number generator. */
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    /* Seed pseudo random number generator with nanoseconds. */
    srand(tp.tv_nsec);

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams(&reconnectParams,
                                      CONNECTION_RETRY_BACKOFF_BASE_MS,
                                      CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS,
                                      CONNECTION_RETRY_MAX_ATTEMPTS);

    /* Attempt to connect to MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase until maximum
     * attempts are reached.
     */
    do {
        /* Establish a TLS session with the MQTT broker. This example connects
         * to the MQTT broker as specified in AWS_IOT_ENDPOINT and AWS_MQTT_PORT
         * at the demo config header. */
        ESP_LOGI(TAG, "Establishing a TLS session to %.*s:%d.",
                 MQTT_BROKER_ENDPOINT_LENGTH,
                 MQTT_BROKER_ENDPOINT,
                 MQTT_BROKER_PORT);
        tlsStatus = xTlsConnect(pNetworkContext);

        if (tlsStatus != TLS_TRANSPORT_SUCCESS) {
            /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
            backoffAlgStatus = BackoffAlgorithm_GetNextBackoff(&reconnectParams, generateRandomNumber(),
                                                               &nextRetryBackOff);

            if (backoffAlgStatus == BackoffAlgorithmRetriesExhausted) {
                ESP_LOGE(TAG, "Connection to the broker failed, all attempts exhausted.");
                returnStatus = EXIT_FAILURE;
            } else if (backoffAlgStatus == BackoffAlgorithmSuccess) {
                ESP_LOGW(TAG, "Connection to the broker failed. Retrying connection "
                              "after %hu ms backoff.",
                         (unsigned short) nextRetryBackOff);
                vTaskDelay(pdMS_TO_TICKS(nextRetryBackOff));
            }
        }
    } while ((tlsStatus != TLS_TRANSPORT_SUCCESS) && (backoffAlgStatus == BackoffAlgorithmSuccess));

    return returnStatus;
}

static MQTTStatus_t prvMQTTInit() {
    /* Based on
     * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/mqtt-agent-task.c#L426-L474
     */
    TransportInterface_t xTransport;
    MQTTStatus_t xReturn;
    MQTTFixedBuffer_t xFixedBuffer = {.pBuffer = xNetworkBuffer, .size = MQTT_AGENT_NETWORK_BUFFER_SIZE};
    static uint8_t staticQueueStorageArea[MQTT_AGENT_COMMAND_QUEUE_LENGTH * sizeof(MQTTAgentCommand_t *)];
    static StaticQueue_t staticQueueStructure;
    MQTTAgentMessageInterface_t messageInterface =
            {
                    .pMsgCtx        = nullptr,
                    .send           = Agent_MessageSend,
                    .recv           = Agent_MessageReceive,
                    .getCommand     = Agent_GetCommand,
                    .releaseCommand = Agent_ReleaseCommand
            };

    ESP_LOGD(TAG, "Creating command queue.");
    xCommandQueue.queue = xQueueCreateStatic(MQTT_AGENT_COMMAND_QUEUE_LENGTH,
                                             sizeof(MQTTAgentCommand_t *),
                                             staticQueueStorageArea,
                                             &staticQueueStructure);
    configASSERT(xCommandQueue.queue);
    messageInterface.pMsgCtx = &xCommandQueue;

    /* Initialize the task pool. */
    Agent_InitializePool();

    /* Fill in Transport Interface send and receive function pointers. */
    xTransport.pNetworkContext = &networkContext;
    xTransport.send = espTlsTransportSend;
    xTransport.recv = espTlsTransportRecv;
    xTransport.writev = nullptr;

    /* Initialize MQTT library. */
    xReturn = MQTTAgent_Init(&xGlobalMqttAgentContext,
                             &messageInterface,
                             &xFixedBuffer,
                             &xTransport,
                             prvGetTimeMs,
                             prvIncomingPublishCallback,
            /* Context to pass into the callback. Passing the pointer to subscription array. */
                             xGlobalSubscriptionList);

    return xReturn;
}

static void prvSubscriptionCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                           MQTTAgentReturnInfo_t *pxReturnInfo) {
    /* Based on
     * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/mqtt-agent-task.c#L620-L650
     */
    size_t lIndex = 0;
    MQTTAgentSubscribeArgs_t *pxSubscribeArgs = (MQTTAgentSubscribeArgs_t *) pxCommandContext;

    /* If the return code is success, no further action is required as all the topic filters
     * are already part of the subscription list. */
    if (pxReturnInfo->returnCode != MQTTSuccess) {
        /* Check through each of the suback codes and determine if there are any failures. */
        for (lIndex = 0; lIndex < pxSubscribeArgs->numSubscriptions; lIndex++) {
            /* This demo doesn't attempt to resubscribe in the event that a SUBACK failed. */
            if (pxReturnInfo->pSubackCodes[lIndex] == MQTTSubAckFailure) {
                ESP_LOGE(TAG, "Failed to resubscribe to topic %.*s.",
                         pxSubscribeArgs->pSubscribeInfo[lIndex].topicFilterLength,
                         pxSubscribeArgs->pSubscribeInfo[lIndex].pTopicFilter);
                /* Remove subscription callback for unsubscribe. */
                removeSubscription(xGlobalSubscriptionList,
                                   pxSubscribeArgs->pSubscribeInfo[lIndex].pTopicFilter,
                                   pxSubscribeArgs->pSubscribeInfo[lIndex].topicFilterLength);
            }
        }

        /* Hit an assert as some of the tasks won't be able to proceed correctly without
         * the subscriptions. This logic will be updated with exponential backoff and retry.  */
        configASSERT(pdTRUE);
    }
}

static MQTTStatus_t prvHandleResubscribe() {
    /* Based on
     * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/mqtt-agent-task.c#L556-L616
     */

    MQTTStatus_t xResult = MQTTBadParameter;
    uint32_t ulIndex = 0U;
    uint16_t usNumSubscriptions = 0U;

    /* These variables need to stay in scope until command completes. */
    static MQTTAgentSubscribeArgs_t xSubArgs = {0};
    static MQTTSubscribeInfo_t xSubInfo[SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS] = {{static_cast<MQTTQoS_t>(0)}};
    static MQTTAgentCommandInfo_t xCommandParams = {0};

    /* Loop through each subscription in the subscription list and add a subscribe
     * command to the command queue. */
    for (ulIndex = 0U; ulIndex < SUBSCRIPTION_MANAGER_MAX_SUBSCRIPTIONS; ulIndex++) {
        /* Check if there is a subscription in the subscription list. This demo
         * doesn't check for duplicate subscriptions. */
        if (xGlobalSubscriptionList[ulIndex].usFilterStringLength != 0) {
            xSubInfo[usNumSubscriptions].pTopicFilter = xGlobalSubscriptionList[ulIndex].pcSubscriptionFilterString;
            xSubInfo[usNumSubscriptions].topicFilterLength = xGlobalSubscriptionList[ulIndex].usFilterStringLength;

            /* QoS1 is used for all the subscriptions in this demo. */
            xSubInfo[usNumSubscriptions].qos = MQTTQoS1;

            ESP_LOGI(TAG, "Resubscribe to the topic %.*s will be attempted.",
                     xSubInfo[usNumSubscriptions].topicFilterLength,
                     xSubInfo[usNumSubscriptions].pTopicFilter);

            usNumSubscriptions++;
        }
    }

    if (usNumSubscriptions > 0U) {
        xSubArgs.pSubscribeInfo = xSubInfo;
        xSubArgs.numSubscriptions = usNumSubscriptions;

        /* The block time can be 0 as the command loop is not running at this point. */
        xCommandParams.blockTimeMs = 0U;
        xCommandParams.cmdCompleteCallback = prvSubscriptionCommandCallback;
        xCommandParams.pCmdCompleteCallbackContext = reinterpret_cast<MQTTAgentCommandContext_t *>(&xSubArgs);

        /* Enqueue subscribe to the command queue. These commands will be processed only
         * when command loop starts. */
        xResult = MQTTAgent_Subscribe(&xGlobalMqttAgentContext, &xSubArgs, &xCommandParams);
    } else {
        /* Mark the resubscribe as success if there is nothing to be subscribed. */
        xResult = MQTTSuccess;
    }

    if (xResult != MQTTSuccess) {
        ESP_LOGE(TAG, "Failed to enqueue the MQTT subscribe command. xResult=%s.", MQTT_Status_strerror(xResult));
    }

    return xResult;
}

static MQTTStatus_t prvMQTTConnect(bool xCleanSession) {
    /* Based on
     * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/mqtt-agent-task.c#L478-L552
     */

    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent = false;

    /* Many fields are not used in this demo so start with everything at 0. */
    memset(&xConnectInfo, 0x00, sizeof(xConnectInfo));

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    xConnectInfo.cleanSession = xCleanSession;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    xConnectInfo.pClientIdentifier = MQTT_CLIENT_IDENTIFIER;
    xConnectInfo.clientIdentifierLength = MQTT_CLIENT_IDENTIFIER_LENGTH;

    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value. In the absence of sending any other Control
     * Packets, the Client MUST send a PINGREQ Packet.  This responsibility will
     * be moved inside the agent. */
    xConnectInfo.keepAliveSeconds = KEEP_ALIVE_INTERVAL_SECONDS;

    /* Append metrics when connecting to the AWS IoT Core broker. */
#if USE_AWS_IOT_CORE_BROKER
#ifdef CONFIG_MQTT_CLIENT_USERNAME
    xConnectInfo.pUserName = MQTT_CLIENT_USERNAME_WITH_METRICS;
    xConnectInfo.userNameLength = ( uint16_t ) strlen( MQTT_CLIENT_USERNAME_WITH_METRICS );
    xConnectInfo.pPassword = CONFIG_MQTT_CLIENT_PASSWORD;
    xConnectInfo.passwordLength = ( uint16_t ) strlen( CONFIG_MQTT_CLIENT_PASSWORD );
#else
    xConnectInfo.pUserName = AWS_IOT_METRICS_STRING;
    xConnectInfo.userNameLength = AWS_IOT_METRICS_STRING_LENGTH;
    /* Password for authentication is not used. */
    xConnectInfo.pPassword = NULL;
    xConnectInfo.passwordLength = 0U;
#endif
#else /* ifdef USE_AWS_IOT_CORE_BROKER */
#ifdef CONFIG_MQTT_CLIENT_USERNAME
    xConnectInfo.pUserName = CONFIG_MQTT_CLIENT_USERNAME;
    xConnectInfo.userNameLength = ( uint16_t ) strlen( CONFIG_MQTT_CLIENT_USERNAME );
    xConnectInfo.pPassword = CONFIG_MQTT_CLIENT_PASSWORD;
    xConnectInfo.passwordLength = ( uint16_t ) strlen( CONFIG_MQTT_CLIENT_PASSWORD );
#endif /* ifdef CONFIG_MQTT_CLIENT_USERNAME */
#endif /* ifdef USE_AWS_IOT_CORE_BROKER */

    /* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament feature
     * is not used in this demo, so it is passed as NULL. */
    xResult = MQTT_Connect(&(xGlobalMqttAgentContext.mqttContext),
                           &xConnectInfo,
                           nullptr,
                           CONNACK_RECV_TIMEOUT_MS,
                           &xSessionPresent);

    ESP_LOGI(TAG, "Session present: %d", xSessionPresent);

    /* Resume a session if desired. */
    if ((xResult == MQTTSuccess) && (xCleanSession == false)) {
        xResult = MQTTAgent_ResumeSession(&xGlobalMqttAgentContext, xSessionPresent);

        /* Resubscribe to all the subscribed topics. */
        if ((xResult == MQTTSuccess) && (xSessionPresent == false)) {
            xResult = prvHandleResubscribe();
        }
    }

    return xResult;
}

static void prvConnectToMQTTBroker(void) {
    /* Based on
     * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/mqtt-agent-task.c#L920-L936
     */
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t xMQTTStatus;
    NetworkContext_t *pNetworkContext = &networkContext;
    (void) memset(pNetworkContext, 0U, sizeof(NetworkContext_t));

    /* Connect a TCP socket to the broker. */
    returnStatus = connectToServerWithBackoffRetries(pNetworkContext);
    configASSERT(returnStatus == EXIT_SUCCESS);

    /* Initialize the MQTT context with the buffer and transport interface. */
    xMQTTStatus = prvMQTTInit();
    configASSERT(xMQTTStatus == MQTTSuccess);

    /* Form an MQTT connection without a persistent session. */
    xMQTTStatus = prvMQTTConnect(true);
    configASSERT(xMQTTStatus == MQTTSuccess);
}

static void prvMQTTAgentTask(void *pvParameters) {
    /* Based on
     * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/mqtt-agent-task.c#L863-L916
     */
    BaseType_t xNetworkResult = pdFAIL;
    MQTTStatus_t xMQTTStatus = MQTTSuccess, xConnectStatus = MQTTSuccess;
    MQTTContext_t *pMqttContext = &(xGlobalMqttAgentContext.mqttContext);

    (void) pvParameters;

    do {
        /* MQTTAgent_CommandLoop() is effectively the agent implementation.  It
         * will manage the MQTT protocol until such time that an error occurs,
         * which could be a disconnect.  If an error occurs the MQTT context on
         * which the error happened is returned so there can be an attempt to
         * clean up and reconnect however the application writer prefers. */
        xMQTTStatus = MQTTAgent_CommandLoop(&xGlobalMqttAgentContext);

        /* Success is returned for disconnect or termination. The socket should
         * be disconnected. */
        if (xMQTTStatus == MQTTSuccess) {
            /* MQTT Disconnect. Disconnect the socket. */
            xTlsDisconnect(&networkContext);
        }
            /* Error. */
        else {
            /* Reconnect TCP. */
            xNetworkResult = xTlsDisconnect(&networkContext);
            configASSERT(xNetworkResult == TLS_TRANSPORT_SUCCESS);
            xNetworkResult = xTlsConnect(&networkContext);
            configASSERT(xNetworkResult == TLS_TRANSPORT_SUCCESS);
            pMqttContext->connectStatus = MQTTNotConnected;
            /* MQTT Connect with a persistent session. */
            xConnectStatus = prvMQTTConnect(false);
            configASSERT(xConnectStatus == MQTTSuccess);
        }
    } while (xMQTTStatus != MQTTSuccess);
}

void connectToMQTTAndStartAgent(void *pvParameters) {
    /* Create the TCP connection to the broker, then the MQTT connection to the
     * same. */
    prvConnectToMQTTBroker();

    /* This task has nothing left to do, so rather than create the MQTT
    * agent as a separate thread, it simply calls the function that implements
    * the agent - in effect turning itself into the agent. */
    prvMQTTAgentTask(nullptr);

    /* Should not get here.  Force an assert if the task returns from
     * prvMQTTAgentTask(). */
    configASSERT(pdTRUE);
}