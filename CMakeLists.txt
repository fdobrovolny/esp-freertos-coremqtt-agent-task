idf_component_register(SRCS "src/core_mqtt_agent_task.c" "src/core_mqtt_agent_subs_manager.c"
        INCLUDE_DIRS "include"
        REQUIRES coreMQTT-Agent backoffAlgorithm esp-tls
)