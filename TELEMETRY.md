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

### Kafka
This mode is only available if built with [librdkafka](https://github.com/edenhill/librdkafka).

- **Topic**
  - Kafka topic to publish event data to
  - Default: `doom-telemetry`

- **Brokers**
  - A comma-separated list of one or many broker hosts/ip addresses and their ports.
  - Default: `localhost:9092`
  - Example: `host-a:9092,host-b:9092`
