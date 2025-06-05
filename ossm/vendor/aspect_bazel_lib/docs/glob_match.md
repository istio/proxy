<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Public API

<a id="glob_match"></a>

## glob_match

<pre>
glob_match(<a href="#glob_match-expr">expr</a>, <a href="#glob_match-path">path</a>, <a href="#glob_match-match_path_separator">match_path_separator</a>)
</pre>

Test if the passed path matches the glob expression.

`*` A single asterisk stands for zero or more arbitrary characters except for the the path separator `/` if `match_path_separator` is False

`?` The question mark stands for exactly one character except for the the path separator `/` if `match_path_separator` is False

`**` A double asterisk stands for an arbitrary sequence of 0 or more characters. It is only allowed when preceded by either the beginning of the string or a slash. Likewise it must be followed by a slash or the end of the pattern.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="glob_match-expr"></a>expr |  the glob expression   |  none |
| <a id="glob_match-path"></a>path |  the path against which to match the glob expression   |  none |
| <a id="glob_match-match_path_separator"></a>match_path_separator |  whether or not to match the path separator '/' when matching `*` and `?` expressions   |  `False` |

**RETURNS**

True if the path matches the glob expression


<a id="is_glob"></a>

## is_glob

<pre>
is_glob(<a href="#is_glob-expr">expr</a>)
</pre>

Determine if the passed string is a global expression

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="is_glob-expr"></a>expr |  the potential glob expression   |  none |

**RETURNS**

True if the passed string is a global expression


