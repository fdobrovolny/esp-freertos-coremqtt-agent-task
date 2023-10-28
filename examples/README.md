# Examples

## sub_pub_test

This demo spawns an n of publishers and then every 5 seconds
publishes a message to topic `/filter/Publisher<n>`.
To this topic it is also subscribed to and the message is
printed to the console.

```c
vStartSimpleSubscribePublishTask(2, 2048, 5);
```