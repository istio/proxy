def is_diagnostic_line:
  test("^[^:]+:[0-9]+:[0-9]+: (error|warning|note|remark|fatal error): ");

def is_summary_line:
  test("^[0-9]+ (warning|error|note)s? (and [0-9]+ (warning|error|note)s? )?generated\\.\\s*$");

def is_empty_line:
  test("^\\s*$");

def handle_diagnostic_line($state; $line):
  ($line | match("^([^:]+):([0-9]+):([0-9]+): (error|warning|note|remark|fatal error): (.*)$")) as $m
  | (if $state.current then
      $state.diagnostics + [$state.current + {context_lines: $state.context}]
    else
      $state.diagnostics
    end) as $diags
  | ($m.captures[4].string
    | if test("\\[([^\\]]+)\\]\\s*$") then
        match("^(.*)\\s*\\[([^\\]]+)\\]\\s*$")
        | {
          msg: (.captures[0].string | sub("^\\s+"; "") | sub("\\s+$"; "")),
          chk: .captures[1].string
        }
      else
        {msg: (. | sub("^\\s+"; "") | sub("\\s+$"; "")), chk: ""}
      end) as $parsed
  | {
    diagnostics: $diags,
    current: {
      file: $m.captures[0].string,
      line: ($m.captures[1].string | tonumber),
      column: ($m.captures[2].string | tonumber),
      severity: $m.captures[3].string,
      message: $parsed.msg,
      check: $parsed.chk,
      context_lines: []
    },
    context: []
  };

def parse_line:
  . as $state
  | $state.line as $line
  | if ($line | is_diagnostic_line) then
      handle_diagnostic_line($state; $line)
    elif ($line | is_summary_line) then
      $state
    elif ($line | is_empty_line) then
      $state
    elif $state.current then
      {
        diagnostics: $state.diagnostics,
        current: $state.current,
        context: ($state.context + [$line])
      }
    else
      $state
    end;

[., inputs]
| join("\n")
| split("\n")
| reduce .[] as $line (
  {diagnostics: [], current: null, context: []};
  . + {line: $line} | parse_line
)
| if .current then
    .diagnostics + [.current + {context_lines: .context}]
  else
    .diagnostics
  end
