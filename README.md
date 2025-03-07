# AZ-V2X

This project uses the V2X library [Vanetza](https://www.vanetza.org/) to provide a REST API service that can send DENM (Decentralized Environmental Notification Message) messages over AMQP to a NordicWay Interchange broker.

## Dev container
This project is setup to use a dev container. To use the dev container you will need to have Docker installed on your machine and preferably Visual Studio Code or a derivative. 

### To use the dev container
0. Fetch the submodules "$ git submodule update --init --recursive"
1. Open the project in Visual Studio Code
2. Install the Remote - Containers extension
3. You should be promted to reopen the project in a container. If not, you can open the command palette and search for "Remote-Containers: Reopen in Container"

### Build native application (in the dev container)
```bash
$ mkdir build && cd build
$ cmake .. && cmake --build .
```

### Run the service
```bash
$ ./AZ-V2X --help  # Show available options
$ ./AZ-V2X -c ../ssl-certs -l debug -u <COMMON_NAME> --amqp-url=amqps://bouvet.pilotinterchange.eu:5671 --amqp-send=del-da0c2e16-1354-4a6c-bce6-654654665129 --amqp-receive=loc-1d23349e-c70e-4d73-b5fd-f654654654657
```

## Build docker image (on your local machine)
```bash
$ docker build --network=host -t az-v2x -f .devcontainer/Dockerfile .
```

### Run docker image
```bash
$ docker run --network=host \
  -v $(pwd)/ssl-certs:/workspace/ssl-certs \
  -e CERT_DIR=/workspace/ssl-certs \
  -e AMQP_URL=amqps://bouvet.pilotinterchange.eu:5671 \
  -e AMQP_SEND=del-123123 \
  -e AMQP_RECEIVE=loc-123123 \
  -e LOG_LEVEL=debug \
  -e USERNAME=<COMMON_NAME>  \
  az-v2x
```

### SSL certificates
The application requires SSL certificates to be present in the directory specified by the -c flag (or CERT_DIR environment variable). The files must match:
- '<COMMON_NAME>.key'
- '<COMMON_NAME>.crt'
- 'truststore.pem'

If connecting to bouvet.pilotinterchange.eu, you can get the certificates from https://napcore.npra.io/certificate. Just rename the files to match the above requirements.

### Command line options (Can also be set via environment variables):

| Option | Environment Variable | Description | Example |
|--------|---------------------|-------------|---------|
| `--help, -h` | - | Show help message | - |
| `--cert-dir, -c` | `CERT_DIR` | Directory containing SSL certificates | "ssl-certs/" |
| `--log-level, -l` | `LOG_LEVEL` | Logging level (debug, info, warn, error) | "info" |
| `--username, -u` | `USERNAME` | Organization/User name | "Astazero" |
| `--amqp-url` | `AMQP_URL` | AMQP(S) broker URL | "amqp://localhost:5672" |
| `--amqp-send` | `AMQP_SEND` | AMQP send address | "del-123123" |
| `--amqp-receive` | `AMQP_RECEIVE` | AMQP receive address | "loc-123123" |
| `--http-host` | `HTTP_HOST` | HTTP server host | "0.0.0.0" |
| `--http-port` | `HTTP_PORT` | HTTP server port | 8080 |
| `--ws-port` | `WS_PORT` | WebSocket server port | 8081 |

Environment variables can be used when running the service, for example:

## REST API

The service provides a REST API for sending DENM messages to the AMQP broker. The API documentation is available through Swagger UI at `http://localhost:8080/api-docs` when the service is running.

### Send a DENM message

```bash
curl -X POST http://localhost:8080/denm \
  -H "Content-Type: application/json" \
  -d '{
  "publisherId": "SE12345",
  "publicationId": "SE12345:DENM-TEST",
  "originatingCountry": "SE",
  "protocolVersion": "DENM:1.3.1",
  "messageType": "DENM",
  "longitude": 12.770160,
  "latitude": 57.772987,
  "shardId": 1,
  "shardCount": 1,
  "data": {
    "header": {
      "protocolVersion": 2,
      "messageId": 1,
      "stationId": 1234567
    },
    "management": {
      "actionId": 20,
      "detectionTime": "2025-03-03T16:30:00",
      "referenceTime": "2025-03-03T16:30:00",
      "stationType": 3,
      "eventPosition": {
        "latitude": 57.772987,
        "longitude": 12.770160,
        "altitude": 190.0
      }
    },
    "situation": {
      "informationQuality": 1,
      "causeCode": 2,
      "subCauseCode": 0
    }
  }
}'

```

#### Required fields:
Header/Application properties:
- `publisherId`: Publisher identifier (string)
- `publicationId`: Publication identifier (string)
- `originatingCountry`: Two-letter country code (string)
- `protocolVersion`: Protocol version (string)
- `messageType`: Message type (string)
- `latitude`: Latitude in degrees (number)
- `longitude`: Longitude in degrees (number)
- `quadTree`: QuadTree (string)

data:
- `header`: Header (object)
  - `protocolVersion`: Protocol version (integer, default: 2)
  - `messageId`: Message identifier (integer, default: 1)
  - `stationId`: Station identifier (integer, default: 1234567)
- `management`: Management (object)
  - `actionId`: Action identifier (integer, default: 1)
  - `detectionTime`: Detection time in ISO 8601 format (string, optional, default: current time)
  - `referenceTime`: Reference time in ISO 8601 format (string, optional, default: current time)
  - `stationType`: Station type (integer, default: 3)
  - `eventPosition`: Event position (object)
    - `latitude`: Latitude in degrees (number, default: 0)
    - `longitude`: Longitude in degrees (number, default: 0)
    - `altitude`: Altitude in meters (number, default: 0)
- `situation`: Situation (object)
  - `informationQuality`: Information quality (integer, default: 0)
  - `causeCode`: Cause code (integer, default: 1)
  - `subCauseCode`: Sub cause code (integer, default: 0)

#### Optional fields:
Header/Application properties:
- `shardId`: Shard identifier (integer, default: 1) Mandatory if sharding is enabled in capability
- `shardCount`: Shard count (integer, default: 1) Mandatory if sharding is enabled in capability

## WebSocket

The service also provides a WebSocket endpoint for sending DENM messages to the AMQP broker. The WebSocket endpoint is available at `ws://localhost:8081/ws`.





## Configure a local AMQP broker for development

You can use Apache ActiveMQ Artemis as a local AMQP broker for development. This can be started inside the dev container. 

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


