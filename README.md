# AZ-V2X

This project uses the V2X library [Vanetza](https://www.vanetza.org/) to simulate a V2X station that can send and receive messages over a V2X network. To be expanded...

## Dev container
This project is setup to use a dev container. To use the dev container you will need to have Docker installed on your machine and preferably Visual Studio Code or a derivative. 

### To use the dev container
1. Open the project in Visual Studio Code
2. Install the Remote - Containers extension
3. You should be promted to reopen the project in a container. If not, you can open the command palette and search for "Remote-Containers: Reopen in Container"


# Run the application

### Build the application and run
```
$ mkdir build && cd build
$ cmake .. && cmake --build .
$ ./AZ-V2X
```



## Configure a local AMQP broker for development

You might also need to have a local AMQP broker running to test against. You can use the following guide to start a local ActiveMQ AMQP broker (inside the dev container):

### Create a directory to save the broker configuration locally
```
$ mkdir -p /workspace/AZ-V2X/activeMQ/config
 // Make the directory writable by the container
$ sudo chmod -R 777 /workspace/AZ-V2X/activeMQ/config
```

### Run the broker
```
$ docker run --detach -it --name artemisbroker -p 61616:61616 -p 8161:8161 -p 5672:5672 -v /workspace/AZ-V2X/activeMQ/config:/var/lib/artemis-instance apache/activemq-artemis:latest-alpine
```

### Turn off security for easier testing
```
$ docker exec -it artemisbroker /bin/bash
$ cd etc
$ vi broker.xml

// Set the following security-enabled setting to false to the broker.xml
<configuration...>
   <core...>
      ...
      <security-enabled>false</security-enabled>
      ...
   </core>
</configuration>

// Restart the container
$ exit
$ docker restart artemisbroker
```

You can log in to the artemis container with

```
docker exec -it mycontainer /var/lib/artemis-instance/bin/artemis shell --user artemis --password artemis
```

### Add a guest user
```
user add
// follow the prompts to add the user
```