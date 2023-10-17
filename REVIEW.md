# Brief review and some suggestions about push1st

I have to admit that the project is well organized by classes. Although there is a couple of questions regarding names of classes and their fields and code formatting in some places. But I'm afraid that it isn't gonna be objective. 

So, a couple of paragraphs about how it works. 

There is a class `cbroker` which launches main components `capiserver`, `ccluster`. Class `capiserver` provides http request handlers over tcp and unix servers, `ctcpapiserver` and `cunixapiserver` relatively. The handlers allow you get list of channels, tokens and push messages to channels and etc.


The class `cclusters` is responsible for interaction between nodes. There is an implementation of `ping` mechanism between nodes, which seemed to be strange.

First of all the `ping` mechanism is optional but it shouldn't be. From my point of view it is required if we're gonna provide high message delivery on failing some of nodes.
What to do with subscriber clients connected to the failed node? The first proposal was that the node sends a message to connected clients with a list of active nodes before crashing, so that allows clients to reconnect to other nodes. But, on the other hand, the node may not have an opportunity to send such a message. In this case, the only possible option is that clients keep open 2 different sessions with different nodes: main and spare connections. It allows to deliver immediately messages over the other node when the main node is failed. 
Yet another strange thing is that there is a field `cnode::NodeLastActivity` which is updated by `ping` mechanism, but is never used. So, it should be used to detect main node failures by spare nodes and to deliver messages by them. 


Channels are the main objects in the project. They are defined by the `cchannel` class and are bidirectional. There are private, precense (same as private but contains a list of subscribers) and public channels. Clients create and subscribe to channels as soon as they open ws/pusher sessions, `cwssession::OnWsConnect` and `cpushersession::OnPusherSubscribe` relatively. On pushing a message to one of the channels, current node sends it to its subscribers, then pushes the message to other nodes in order to deliver the channels subscribers over them.


In order to scale client-side services it's nice to make kind of unidirectional channels which provide subscriptions and deliver a message to only one of subscribers. Receiver is selected, let's say, by its weight, which provided on subscribing to the channel. IThis allows one message to be processed by only one subscriber. So you can run different instances of same service on different servers which leads to horizontal scaling of load on the client side. In this case, the weights of subscribers can reflect server characteristics. 
