### cpp2sky


##### message `TracerConfig` (cpp2sky/config.proto)

| Field | Description | Type | C++ |
| ----- | ----------- | ---- | --- |
| protocol | Tracer protocol. | Static config only | Protocol | |
| service_name | Service name | Static config only | string | string |
| instance_name | Instance name | Static config only | string | string |
| address | OAP address. | Static config only | string | string |
| token | OAP token. | Static config only | string | string |
| delayed_buffer_size | The size of buffer it stores pending messages. | Static config only | uint32 | uint32 |
| ignore_operation_name_suffix | If the operation name of the first span is included in this set, this segment should be ignored. | This value can be changed with SkyWalking CDS. | (slice of) string | (slice of) string |
| cds_request_interval | CDS sync request interval. If this value is zero, CDS feature will be disabled. | Static config only | uint32 | uint32 |



