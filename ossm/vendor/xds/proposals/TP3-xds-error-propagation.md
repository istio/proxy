TP3: xds-error-propagation
----
* Author(s): anicr7
* Approvers: markdroth, adisuissa
* Implemented in: <xDS client, ...>
* Last updated: 2024-12-12

## Abstract

This proposal introduces a mechanism for the xDS control plane to communicate errors to clients in cases where the control plane is unable to send a resource that the client has subscribed to (e.g., because the resource does not exist or because the client lacks permission for the resource), thus improving debuggability. In addition, this also provides a mechanism in the [SotW protocol variants](https://www.envoyproxy.io/docs/envoy/latest/api-docs/xds_protocol#variants-of-the-xds-transport-protocol) for the control plane to explicitly tell the client that a resource does not exist, without the client needing to wait for a [15-second does-not-exist timeout](https://www.envoyproxy.io/docs/envoy/latest/api-docs/xds_protocol#knowing-when-a-requested-resource-does-not-exist), similar to what already exists in the incremental protocol variants.

The objective of this proposal is to suggest a way for clients to receive feedback from xDS Management Servers in case of partial/drastic failures without closing any streams or connections.

This proposal includes a new field for each subscribed Resource, called `ResourceError` which will provide detailed information for resource specific issues. The client may use this additional field to obtain notification for resources the xDS Management server couldn’t procure and provide necessary notification to the application. 

## Background

In cases where the xDS control plane is unable to send a resource that the client has subscribed to (e.g., because the resource does not exist or because the client lacks permission for the resource), the control plane does not have a good mechanism for conveying the cause of the error to the client. The only existing mechanism for conveying a specific error message to a client is for the control plane to close the entire xDS stream with a non-OK status. However, this is often undesirable behavior, because a problem with a single resource will also stop the client from receiving updates from other, unrelated resources.
Therefore, most control planes are forced to simply act as if the resource does not exist. In this situation, the client does not know why the control plane was unable to send the resource, so it cannot report useful information to human operators (e.g., via logs or request failure messages), which makes debugging challenging.

Note that while the incremental protocol variants can explicitly indicate to the client that a subscribed resource does not exist, the SotW protocol variants require the client to use a 15-second does-not-exist timeout. This adds an unnecessary delay before the client can react to the error. It has also led to cases where control plane slowness has caused clients to incorrectly react as if a resource does not exist.

### Related Proposals:

* [TP1-xds-transport-next.md](TP1-xds-transport-next.md)

## Proposal

The proposal introduces a new field to the DiscoveryResponse/DeltaDiscoveryResponse proto. This new field `resource_errors` will detail Resource-specific errors and be included in both SotW and Incremental xDS protocols. 

```textproto
// New Proto Message
// Contains detailed error detail for a single resource
message ResourceError {
 ResourceName resource_name = 1;

 google.rpc.Status error_detail = 2;
}

message DiscoveryResponse {

// The version of the response data.
string version_info = 1;

….
…


 // The control plane instance that sent the response.
  config.core.v3.ControlPlane control_plane = 6;

// NEW_FIELD
// An optional repeated list of error details that the control plane 
// can provide to notify the client of issues for the resources that 
// are subscribed.
// This allows the xDS management server to provide optional 
// notifications in case of unavailability, permissions errors 
// without the client having to wait for the `config fetch` timeout.
repeated ResourceError resource_errors = N;
...
}

message DeltaDiscoveryResponse {
  …

  // NEW FIELD
  repeated ResourceError resource_errors = N;
}
```

A corresponding new field will also be added to CSDS which should be treated the same as `CLIENT_NACKED` but also indicates a server error received via `resource_errors`.

```
service ClientStatusDiscoveryService {

  // Config status from a client-side view.
  enum ClientConfigStatus {
    ...

    // NEW FIELD
    // Client received an error from the control plane. The attached config
    // dump is the most recent accepted one. If no config is accepted yet,
    // the attached config dump will be empty.
    CLIENT_RECEIVED_ERROR = 4;
  }
}
```

### Protocol Behavior
Clients that support this mechanism will use this additional field to obtain error information for resources that the xDS server couldn’t provide. When this message is received, the xDS client should cancel any resource timers for the indicated resource. The client should then handle the error based on the status code, as follows (using [RFC-2119](https://datatracker.ietf.org/doc/html/rfc2119) terminology):

  * The following codes will be interpretted by clients as transient failures, meaning that the client MUST continue to use previously cached versions of the resource if it has them: UNAVAILABLE, INTERNAL, UNKNOWN.
  * The following codes will be interpretted by clients as data errors, which the client MAY handle by dropping any previously cached resource: NOT_FOUND, PERMISSION_DENIED
  * The behavior for any other status code is undefined. Data planes SHOULD treat these as transient failures, but future xRFCs may impose additional semantics on them.

Note that an error with status code NOT_FOUND will be interpreted to mean that the resource does not exist. The client should handle this case exactly the same way that it would if the does-not-exist timer fired.

The xDS Management server is only expected to return the error message once rather than throughout for future responses. The client is expected to remember the error message until either a new error message is returned or the resource is returned. This includes LDS and CDS where the control plane is required to send every subscribed 
resource in every response. The set of resources with errors and the set of valid resources must not intersect. 

As the clients don't look at this field right now, the xDS management server must assume the responses remain valid for older clients for backward compatibility. For example, in the SotW variant, LDS and CDS are required to send all subscribed resources in every response, which means that if a control plane wants to send an update for one of these resource types that indicates an error with a particular resource, it must still include all of the subscribed resources that it can return in that response. 

### Wildcard Subscriptions and Glob Collections

For [glob collections](TP1-xds-transport-next.md#glob), the control plane is responsible for deciding which resources are included in the subscription. The control plane in this case can provide error details for two different use cases. One when the issue is with the glob itself or later on when the issue is specific to individual resources. 

1. For errors associated with xDS-TP glob collections; the control plane `error_details` resource name will match the relevant xDS-TP glob collection. This can be used by the control plane to indicate an error with the collection as a whole.
2. For errors associated with specific individual resources that match the glob; the resource name should be the specific resource name associated with the error.

This mechanism currently doesn't support wildcard subscriptions at the moment, though it can be modified in the future if needed. 

## Rationale

The major alternative to this proposal is to use Wrapped Resources by using Resource Containers defined in https://www.envoyproxy.io/docs/envoy/latest/xds/core/v3/resource.proto#xds-core-v3-resource. 

### Wrapped Resources

xDS resource containers are the default protos used for the Incremental xDS protocol and it's usage in SoTW is controlled via the client feature `xds.config.supports-resource-in-sotw`. 

In this proposal the error information is directly passed as part of the resource field in the Resource Container, using the artifact that the field is a protobuf.Any. This enables us to designate the resource as either a `google.rpc.Status` if the xDS management server encountered problems, or as the actual `Resource` if no errors occurred. 

#### Backward Compatibility

To avoid possible confusion with this behavior, it must be protected with a client feature similar to `supports-resource-in-sotw` called `supports-resource-error-unwrapping`. 

Note: This should also be documented here: https://www.envoyproxy.io/docs/envoy/latest/api/client_features#currently-defined-client-features

#### Why not this approach?

This approach has two major drawbacks compared to the chosen approach:

* This alternative would not work for non-wrapped resources
* It introduces a backwards compatibility issue, which adding a new field wouldn’t have as clients would just ignore them. 

## Implementation

This will probably be implemented in gRPC before Envoy.

## Open issues (if applicable)