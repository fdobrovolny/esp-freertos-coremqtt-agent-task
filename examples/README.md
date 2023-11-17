# Examples

## sub_pub_test

This demo spawns an n of publishers and then every 5 seconds
publishes a message to topic `/filter/Publisher<n>`.
To this topic it is also subscribed to and the message is
printed to the console.

```c
vStartSimpleSubscribePublishTask(2, 2048, 5);
```

## Shadow Demo

This demo contains two tasks. The first demonstrates typical use of the Device Shadow library
by keeping the shadow up to date and reacting to changes made to the shadow.
If enabled, the second task uses the Device Shadow library to request change to the device
shadow. This serves to create events for the first task to react to for demonstration purposes.

```c
vStartShadowDemo(2048, 8);
```