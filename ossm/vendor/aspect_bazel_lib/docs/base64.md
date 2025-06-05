<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Utility functions for encoding and decoding strings with base64.

See https://en.wikipedia.org/wiki/Base64.

<a id="base64.decode"></a>

## base64.decode

<pre>
base64.decode(<a href="#base64.decode-data">data</a>)
</pre>

Decode a base64 encoded string.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="base64.decode-data"></a>data |  base64-encoded string   |  none |

**RETURNS**

A string containing the decoded data


<a id="base64.encode"></a>

## base64.encode

<pre>
base64.encode(<a href="#base64.encode-data">data</a>)
</pre>

Base64 encode a string.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="base64.encode-data"></a>data |  string to encode   |  none |

**RETURNS**

The base64-encoded string


