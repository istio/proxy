# Rationale for data model

## Why epoch micros for timestamp?
Zipkin timestamp is epoch micros since the beginning. While staying consistent is
its own goal, the choice of epoch micros serves two purposes:

 * Represents the highest usual accuracy of a system clock
 * Using the maximum safe integer in JSON (2^53 - 1), epoch micros lasts until 2255-06-05 UTC

Epoch nanos meets neither of these, so switching to it would not only break
compatability, but also not usually increase accuracy and likely cause data
conversion errors.

## Why micros for duration?
It is true that often duration can be measured in nanoseconds resolution. However,
Mixing units between timestamp and duration makes for little bugs. It also forces
conversion steps when people do query-based operations between timestamp and
duration.

# Rationale for API

## Get many traces: /api/v2/traceMany?traceIds=id1,id2
Get many traces was added in [/api/v2](zipkin2-api.yaml) to support
more efficient requests when trace IDs are found by means besides the query api.

Here are some use cases:
* out-of-band aggregations, such histograms with attached representative IDs
* trace comparison views
* decoupled indexing and retrieval (Ex lucene over a blob store)
* 3rd party apis that offer stable pagination (via a cursor over trace IDs).

There were several design concerns to address when integrating another feature
into an existing api root. Most of these dealt with how to inform consumers that
the feature exists.

### Why not just use http cache control on /trace/{traceId}?
It is a reasonable question why not just introduce a caching layer on trace ID,
and have callers make successive requests. There are a number of problems with
this. First, it would only help if cache-control headers were sent, but they
have never been sent. This is because results of a trace are unstable until
complete. Also, only use cases that revisit the same trace IDs are aided with
caching, for example trace comparison views. It will always cost more to prime
the cache with multiple requests vs one.

Finally, bear in mind not all use cases revisit the same trace IDs, or even
support http cache control. For example, if someone were paging over all traces
to perform an aggregation, those reads only happen once. In this case, the value
of efficiently getting a bucket of results is foreground.

### Why not just have this an internal detail of storage?
As time goes by, we've accumulated more use cases of multi-get. When there are
more than 3 integrations, we typically consider normalizing a feature.

### Why /traceMany instead of /trace/id,id2
A separate endpoint than /trace/ allows us to help consumers, such as the UI,
know the difference between unsupported (404) and empty (200 with empty list
response). /trace/{traceId}, on the other hand, returns 404 on not found. This
brings up a larger problems that a result of multiple traces is a list of
traces, which is a different data type than a list of spans.

### Why /traceMany instead of /traces?traceIds=id1,id2
When search is disabled, `/trace/{traceId}` is still mounted, eventhough
`/traces` is not. Multi-get is an optimization of `/trace/{traceId}`, so needs
to be mounted when search-like mechanisms are disabled. It is still tempting to
re-use the same endpoint, but there is another problem. A new parameter will not
fail queries in most implementations.

For example, calling `/traces?traceIds=id1,id2` in an existing service is most
likely to return as if there was no `traceIds` parameter at all, resulting in
random results returned.

### Why /traceMany
`/traceMany` is most similar to `/trace/{traceId}` except for multiple IDs. It
has to be a different name to route properly. Other endpoints use lowerCamel
case format, too.

### Why a single traceIds parameter instead of multiple query parameters?
There are a lot of prior art on a single comma-separated query parameter. Using
this style will allow more frameworks vs those who can support multi-value via
redundant properties. Splitting on comma is of no risk as it is impossible to
have a trace ID with an embedded comma (trace IDs are lower-hex).

### Why minimum 2 trace IDs?
Minimum 2 trace IDs helps in a couple ways. Firstly, empty queries are bugs. The
more interesting question is why minimum 2 instead of 1. The most important
reason is that tools should use the oldest route that can satisfy a request. If
only passing a single ID, `/trace/{traceId}` is better. Moreover, that endpoint
is more likely to be cached properly.

### Why ListOfTraces instead of indexed?
The `/traceMany` endpoint returns `ListOfTraces` of those which match the input
traceIds vs other options such as index based. This is primarily to re-use
parsers from the `/traces` endpoint. Also, by not mandating index order, this
endpoint better reflects the scatter/gather nature of most implementations,
avoiding an ordering stage. For example, implementations can choose to order,
but also choose to write the response incrementally.
