idf_component_register(SRCS "src/core_mqtt_agent_task.cpp" "src/core_mqtt_agent_subs_manager.cpp"
        INCLUDE_DIRS "include"
        REQUIRES coreMQTT-Agent backoffAlgorithm esp-tls
)