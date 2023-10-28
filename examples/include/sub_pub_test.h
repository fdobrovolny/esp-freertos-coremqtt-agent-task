//
// Created by fdobrovo on 28.10.23.
//

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
