# Sampling Delegation
This document is a technical description of how sampling delegation works in
this library. The intended audience is maintainers of the library.

Sampling delegation allows a tracer to use the trace sampling decision of a
service that it calls. The purpose of sampling delegation is to allow reverse
proxies at the ingress of a system (gateways) to use trace sampling decisions
that are decided by the actual services, as opposed to having to decide the
trace sampling decision at the proxy.  The idea is that putting a reverse proxy
in front of your service(s) should not change how you configure sampling.

See the `sampling-delegation` directory in Datadog's internal architecture
repository for the specification of sampling delegation.

## Roles
In sampling delegation, a tracer plays one or both of two roles:

- The _delegator_ is the tracer that is configured to delegate its trace
  sampling decision.  The delegator will request a sampling decision from one of
  the services it calls.
  - It will send the `X-Datadog-Delegate-Trace-Sampling` request header.
  - If it is the root service, and if delegation succeeded, then it will set the
    `_dd.is_sampling_decider:0` tag to indicate that some other service made the
    sampling decision.
- The _delegatee_ is the tracer that has received a request whose headers
  indicate that the client is delegating the sampling decision.  The delegatee
  will make a trace sampling decision using its own configuration, and then
  convey that decision back to the client.
  - It will send the `X-Datadog-Trace-Sampling-Decision` response header.
  - If its sampling decision was made locally, as opposed to delegated to yet
    another service, then it will set the `_dd.is_sampling_decider:1` tag to
    indicate that it is the service that made the sampling decision.

For a given trace, the tracer might act as the delegator, the delegatee, both,
or neither.

## Tracer Configuration
Whether a tracer should act as a delegator is determined by its configuration.

`bool TracerConfig::delegate_trace_sampling` is defined in [tracer_config.h][1]
and defaults to `false`.  Its value is overridden by the
`DD_TRACE_DELEGATE_SAMPLING` environment variable.  If `delegate_trace_sampling`
is `true`, then the tracer will act as delegator.

## Runtime State
Whether a tracer should act as a delegatee is determined by whether the
extracted trace context includes the `X-Datadog-Delegate-Trace-Sampling` request
header.  If trace context is extracted in the Datadog style, and if the
extracted context includes the `X-Datadog-Delegate-Trace-Sampling` header, then
the tracer will act as delegatee.

All logic relevant to sampling delegation happens in `TraceSegment`, defined in
[trace_segment.h][2]. The `Tracer` that creates the `TraceSegment` passes
two booleans into `TraceSegment`'s constructor:

- `bool sampling_delegation_enabled` indicates whether the `TraceSegment` will
  act as delegator.
- `bool sampling_decision_was_delegated_to_me` indicates whether the
  `TraceSegment` will act as delegatee.

`TraceSegment` then keeps track of its sampling delegation relevant state in a
private data structure, `struct SamplingDelegation` (also defined in
[trace_segment.h][2]).  `struct SamplingDelegation` contains the two booleans
passed into `TraceSegment`'s constructor, and additional booleans used
throughout the trace segment's lifetime.

### `bool TraceSegment::SamplingDelegation::sent_request_header`
`send_request_header` indicates that, as delegator, the trace segment included
the `X-Datadog-Delegate-Trace-Sampling` request header as part of trace context
sent to another service.

`sent_request_header` is used to prevent sampling delegation from being
requested of two or more services.  Once a trace segment has requested sampling
delegation once, it will not request sampling delegation again, even if it never
receives the delegated decision in response.

### `bool TraceSegment::SamplingDelegation::received_matching_response_header`
`received_matching_response_header` indicates that, as delegator, the trace
segment received a valid `X-Datadog-Trace-Sampling-Decision` response header
from a service to which the trace segment had previously sent the
`X-Datadog-Delegate-Trace-Sampling` request header.

The `X-Datadog-Trace-Sampling-Decision` response header is valid if it is valid
JSON of the form `{"priority": int, "mechanism": int}`.  See
`parse_sampling_delegation_response`, defined in [trace_segment.cpp][3].

`received_matching_response_header` is used as part of determining whether to
set the `_dd.is_sampling_decider:1` tag as delegatee.  If a trace segment is
acting as delegatee, and if it made the sampling decision, then it sets the tag
`_dd.is_sampling_decider:1` on its local root span.  However, the trace segment
might also be acting as delegator.  `received_matching_response_header` allows
the trace segment to determine whether it delegated its decision to another
service, and thus is not the "sampling decider."

An alternative way to determine whether a trace segment delegated its sampling
decision is to see whether its `SamplingDecision::origin` has the value
`SamplingDecision::Origin::DELEGATED` (see [sampling_decision.h][4]).  However,
a trace segment's sampling decision might be overridden at any time by
`TraceSegment::override_sampling_priority(int)`.  So, to answer the question
"did we delegate to another service?" it is better to keep track of whether the
trace segment received a valid and expected `X-Datadog-Trace-Sampling-Decision`
response header, which is what `received_matching_response_header` does.

### `bool TraceSegment::SamplingDelegation::sent_response_header`
`sent_response_header` indicates that, as delegatee, the trace segment sent its trace sampling
decision back to the client in the `X-Datadog-Trace-Sampling-Decision` response
header.

`sent_response_header` is used as part of determining whether to set the
`_dd.is_sampling_decider:1` tag as delegatee. The trace segment would not claim
to be the "sampling decider" if the service that delegated to it does not know
about the decision. If `sent_response_header` is true, then the trace segment
can be fairly confident that the client will receive the sampling decision.

### `bool Span::expecting_delegated_sampling_decision_`
In addition to the state maintained in `TraceSegment`, `Span` also has a
sampling delegation related `bool`.  See [span.h][5].

When sampling delegation is requested for an injected `Span`, that span
remembers that it injected the `X-Datadog-Delegate-Trace-Sampling` header.

Later, when the corresponding response is examined, the `Span` knows whether to
expect the `X-Datadog-Trace-Sampling-Decision` response header to be present.

`bool Span::expecting_delegated_sampling_decision_` prevents a `Span` from
interpreting an `X-Datadog-Trace-Sampling-Decision` response header when none
was requested.

## Reading and Writing Responses
Distributed tracing typically does not involve RPC _responses_.  When a service
X makes an HTTP/gRPC/etc. request to another service Y, X injects information
about the trace in request metadata (e.g. HTTP request headers).  Y then
extracts that information from the request.

Responses aren't involved.

Now, with sampling delegation, responses _are_ involved.

Trace context injection and extraction are about _requests_ (sending a receiving,
respectively).  For _responses_ the tracing library needs a new notion.

`TraceSegment` has two member functions for producing and consuming
response-related metadata (see [trace_segment.h][2]):

- `void TraceSegment::write_sampling_delegation_response(DictWriter&)` writes
  the `X-Datadog-Trace-Sampling-Decision` response header, if appropriate. This
  is something that a _delegatee_ does.
- `void TraceSegment::read_sampling_delegation_response(const DictReader&)`
  reads the `X-Datadog-Delegate-Trace-Sampling` response header, if present.
  This is something that a _delegator_ does.

`TraceSegment::read_sampling_delegation_response` is not called directly by an
instrumented application.
Instead, an instrumented application calls
`Span::read_sampling_delegation_response` on the `Span` that performed the
injection whose response is being examined.
`Span::read_sampling_delegation_response` then might call
`TraceSegment::read_sampling_delegation_response`.

`TraceSegment::write_sampling_delegation_response` is called directly by an
instrumented application.

Just as `Tracer::extract_span` and `Span::inject` must be called by an
instrumented application in order for trace context propagation to work,
`Span::read_sampling_delegation_response` and
`TraceSegment::write_sampling_delegation_response` must be called by an
instrumented application in order for sampling delegation to work.

## Per-Trace Configuration
In addition to the `Tracer`-wide configuration option `bool
TracerConfig::delegate_trace_sampling`, there is also a per-injection option
`Optional<bool> InjectionOptions::delegate_sampling_decision`.

`Span::inject` has an overload
`void inject(DictWriter&, const InjectionOptions&) const`.  The
`InjectionOptions` can be used to specify sampling delegation (or its absence)
for this particular injection site.  If
`InjectionOptions::delegate_sampling_decision` is null, which is the default,
then the tracer-wide configuration option is used instead.

This granularity of control is useful in NGINX, where one `location` (i.e.
upstream or backend) might be configured for sampling delegation, while another
`location` might not.

[1]: ../src/datadog/tracer_config.h
[2]: ../src/datadog/trace_segment.h
[3]: ../src/datadog/trace_segment.cpp
[4]: ../src/datadog/sampling_decision.h
[5]: ../src/datadog/sampling_decision.h
