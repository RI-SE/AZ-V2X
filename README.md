# AZ-V2X

This project uses the V2X library [Vanetza](https://www.vanetza.org/) to simulate a V2X station that can send and receive messages over a V2X network. To be expanded...

# Dev container
This project is setup to use a dev container. To use the dev container you will need to have Docker installed on your machine and preferably Visual Studio Code or a derivative. 

## To use the dev container
1. Open the project in Visual Studio Code
2. Install the Remote - Containers extension
3. You should be promted to reopen the project in a container. If not, you can open the command palette and search for "Remote-Containers: Reopen in Container"


# Run the application

## Build the application and run
mkdir build
cd build
cmake ..
make

./AZ-V2X

 You might also need to have a local AMQP broker running to test against. You can use the following command to start a local ActiveMQ AMQP broker (inside the dev container):

```
docker run --detach --name mycontainer -p 61616:61616 -p 8161:8161 -p 5672:5672 --rm apache/activemq-artemis:latest-alpine
```

You can log in the artemis container with

```
docker exec -it mycontainer /var/lib/artemis-instance/bin/artemis shell --user artemis --password artemis
```