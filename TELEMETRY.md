# DOOM Telemetry Documentation

The DOOM telemetry system writes telemetry events as JSON to different output channels.

## Modes
Currently supported modes and their configs...

### File system
Writes telemetry events out to file like `doom-<timestamp in millis>.log`

### UDP
Streams events via UDP (datagram) packets to a target system. Since it's UDP, it's got low overhead at the expense of potential data-loss and no ordering guarantees.

- **Host**
  - Target host or IP to send the packets to
  - Default: `localhost`
- **Port**
  - UDP port to send data to on **Host*
  - Default: `10666`

### WebSockets
This mode is only available if built with [libtls](https://libressl.org).

Since the WebSocket support is still immature, it doesn't support a simple URI-based input. You'll need to provide piece-meal:

- **Host**
  - Target host or IP to connect to
  - Default: `localhost`
- **Port**
  - TCP port to connect to
  - Default: `8000`
- **Resource**
  - The HTTP resource to use during the Upgrade request
  - Default: `/`
- **Use TLS?**
  - Whether or not to use a TLS connection to the host
  - Default: No

> NOTE: there are some known issues with WebSockets at the moment as the library I'm using to support them I wrote myself. See [dumb-ws](https://github.com/voutilad/dumb-ws) for more information on caveats, etc. In short: this integration does the bare minimum and may not work with all WebSocket servers.

### Kafka
This mode is only available if built with [librdkafka](https://github.com/edenhill/librdkafka).

- **Topic**
  - Kafka topic to publish event data to
  - Default: `doom-telemetry`

- **Brokers**
  - A comma-separated list of one or many broker hosts/ip addresses and their ports.
  - Default: `localhost:9092`
  - Example: `host-a:9092,host-b:9092`

> NOTE: Kafka support is experimental. Currently no effort is taken to retry delivery of telemetry data. If the data is accepted by the librdkafka producer's internal queue, it is currently considered successfully recorded.
