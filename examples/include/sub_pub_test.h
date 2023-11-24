/*
 * https://github.com/FreeRTOS/coreMQTT-Agent-Demos/blob/0a5b37ad5c79bc8d2a38baf3df9ccc5d7f3ac329/source/demo-tasks/simple_sub_pub_demo.c
 *
 * Based on Lab-Project-coreMQTT-Agent 201215
 */

#include "stdint.h"
#include "freertos/task.h"

#ifndef SUB_PUB_TEST_H
#define SUB_PUB_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The function that implements the task demonstrated by this file.
 */
void vStartSimpleSubscribePublishTask(uint32_t ulNumberToCreate,
                                      configSTACK_DEPTH_TYPE uxStackSize,
                                      UBaseType_t uxPriority);

#ifdef __cplusplus
}
#endif
#endif //SUB_PUB_TEST_H
