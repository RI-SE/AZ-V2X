# AZ-V2X

# To run the application you will need to have a local AMQP broker running.

# Start a local AMQP broker
docker run --detach --name mycontainer -p 61616:61616 -p 8161:8161 -p 5672:5672 --rm apache/activemq-artemis:latest-alpine

# You can log in the docker container with
docker exec -it mycontainer /var/lib/artemis-instance/bin/artemis shell --user artemis --password artemis

# build the application
mkdir build
cd build
cmake ..
make

# run the application
./AZ-V2X
