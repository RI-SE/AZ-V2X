# AZ-V2X

This project uses the V2X library [Vanetza](https://www.vanetza.org/) to provide a REST API service that can send DENM (Decentralized Environmental Notification Message) messages over AMQP to a NordicWay Interchange broker.

## Dev container
This project is setup to use a dev container. To use the dev container you will need to have Docker installed on your machine and preferably Visual Studio Code or a derivative. 

### To use the dev container
1. Open the project in Visual Studio Code
2. Install the Remote - Containers extension
3. You should be promted to reopen the project in a container. If not, you can open the command palette and search for "Remote-Containers: Reopen in Container"

# Build and Run

### Build the application
```bash
$ mkdir build && cd build
$ cmake .. && cmake --build .
```

### Run the service
```bash
$ ./AZ-V2X --help  # Show available options
$ ./AZ-V2X --amqp-url "amqp://localhost:5672" --amqp-address "examples" --http-port 8080
```

Available options:
- `--help, -h`: Show help message
- `--cert-dir, -c`: Directory containing SSL certificates (default: "ssl-certs/")
- `--log-level, -l`: Logging level (debug, info, warn, error) (default: "info")
- `--amqp-url`: AMQP broker URL (default: "amqp://localhost:5672")
- `--amqp-address`: AMQP address (default: "examples")
- `--http-host`: HTTP server host (default: "localhost")
- `--http-port`: HTTP server port (default: 8080)

## REST API

The service provides a REST API for sending DENM messages. The API documentation is available through Swagger UI at `http://localhost:8080/api-docs` when the service is running.

### Send a DENM message

```bash
curl -X POST http://localhost:8080/denm \
  -H "Content-Type: application/json" \
  -d '{
    "stationId": 1234567,
    "actionId": 20,
    "latitude": 57.779017,
    "longitude": 12.774981,
    "altitude": 190.0,
    "causeCode": 1,
    "subCauseCode": 0,
    "publisherId": "NO00001",
    "originatingCountry": "NO"
  }'
```

Required fields:
- `stationId`: Station identifier (integer)
- `latitude`: Latitude in degrees (number)
- `longitude`: Longitude in degrees (number)
- `publisherId`: Publisher identifier (string)
- `originatingCountry`: Two-letter country code (string)

Optional fields:
- `actionId`: Action identifier (integer, default: 1)
- `altitude`: Altitude in meters (number, default: 0)
- `causeCode`: Cause code (integer, default: 1)
- `subCauseCode`: Sub cause code (integer, default: 0)
- `stationType`: Station type (integer, default: 3)
- `informationQuality`: Information quality (integer, default: 0)

## Configure a local AMQP broker for development

You can use Apache ActiveMQ Artemis as a local AMQP broker for development:

### Create a directory to save the broker configuration locally
```bash
$ mkdir -p /workspace/AZ-V2X/activeMQ/config
# Make the directory writable by the container
$ sudo chmod -R 777 /workspace/AZ-V2X/activeMQ/config
```

### Run the broker
```bash
$ docker run --detach -it --name artemisbroker \
    -p 61616:61616 -p 8161:8161 -p 5672:5672 \
    -v /workspace/AZ-V2X/activeMQ/config:/var/lib/artemis-instance \
    apache/activemq-artemis:latest-alpine
```

### Turn off security for easier testing
```bash
$ docker exec -it artemisbroker /bin/bash
$ cd etc
$ vi broker.xml

# Set the following security-enabled setting to false in broker.xml
<configuration...>
   <core...>
      ...
      <security-enabled>false</security-enabled>
      ...
   </core>
</configuration>

# Restart the container
$ exit
$ docker restart artemisbroker
```

You can log in to the artemis container with:
```bash
docker exec -it artemisbroker /var/lib/artemis-instance/bin/artemis shell --user artemis --password artemis
```

### Add a guest user
```bash
user add
# follow the prompts to add the user
```