# Apache SkyWalking data collect protocol
Apache SkyWalking typically collect data from 
1. Traces
2. Metrics(Meter system)
3. Logs
4. Command data. Push the commands to the agents from Server.
5. Event. 
6. eBPF profiling tasks.
7. Agent in-process profiling tasks.

This repo hosts the protocol of SkyWalking native report protocol, defined in gRPC. Read [Protocol DOC](https://skywalking.apache.org/docs/main/next/en/api/trace-data-protocol-v3/) for more details

## Release
This repo wouldn't release separately. All source codes have been included in the main repo release. The tags match the [main repo](https://github.com/apache/skywalking) tags.

## License
Apache 2.0
