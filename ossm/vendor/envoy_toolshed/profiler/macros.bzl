load("@aspect_bazel_lib//lib:jq.bzl", "jq")

def rules_pools(
        name,
        path,
        targets,
        buildozer_bin = "@com_github_bazelbuild_buildtools//buildozer",
        jq_toolchain = "@jq_toolchains//:resolved_toolchain",
        pool_script = "@envoy_toolshed//profiler:rules-pools.sh"):
    """Generate a dict with keys being non-`default` pools, and values
    are a list of targets configured to use the pool.
    """
    native.sh_binary(
        name = "find_rules_pools",
        srcs = [pool_script],
        data = [
            buildozer_bin,
            jq_toolchain,
            targets,
        ],
        env = {
            "BUILDOZER": "$(location %s)" % buildozer_bin,
            "REPO_PATH": path,
            "JQ_BIN": "$(JQ_BIN)",
            "TARGETS": "$(location %s)" % targets,
        },
        toolchains = [jq_toolchain],
    )

    native.genrule(
        name = name,
        outs = ["%s.json" % name],
        cmd = """
        export BUILDOZER="$(location %s)"
        export JQ_BIN="$(JQ_BIN)"
        export REPO_PATH=%s
        export TARGETS="$(location %s)"
        $(location :find_rules_pools) > $@
        """ % (buildozer_bin, path, targets),
        tags = ["no-remote-exec"],
        srcs = [targets],
        stamp = True,
        tools = [
            buildozer_bin,
            ":find_rules_pools",
        ],
        toolchains = [jq_toolchain],
    )

def ci_profile(
        name,
        profiles,
        rules_pools = ":rules_pools"):
    """Ingest profile data for a CI run, and generate reports on usage"""
    for idx, profile in enumerate(profiles):
        jq(
            name = "%s_worker_usage_%s" % (name, idx),
            srcs = [profile],
            out = "%s_worker_usage_%s.json" % (name, idx),
            filter = """
            [.traceEvents[]
              | select(.cat? == "worker_time") as $jobs
              | (if (.args["target_id"] | endswith("fuzz_test_lib")) then
                   .args["target_id"][:-4]
                 elif (.args["target_id"] | endswith("test_fuzz_lib")) then
                   .args["target_id"][:-4]
                 else
                   .args["target_id"]
                 end) as $targetID
              | (.args["process_wrapper.execution_statistics.resource_usage.maxrss"] // 0 | tonumber) as $maxMemory
              | ((.args["process_wrapper.execution_statistics.resource_usage.utime_sec"] // 0 | tonumber)
                  + (.args["process_wrapper.execution_statistics.resource_usage.utime_usec"] // 0 | tonumber) / 1e6) as $userCPUTime
              | ((.args["process_wrapper.execution_statistics.resource_usage.stime_sec"] // 0 | tonumber)
                  + (.args["process_wrapper.execution_statistics.resource_usage.stime_usec"] // 0 | tonumber) / 1e6) as $systemCPUTime
              | ($userCPUTime + $systemCPUTime) as $totalCPUTime
              | ((.dur // 0) / 1e6) as $duration
              | ($totalCPUTime > $duration) as $multiCore
              | $jobs
              | {target_id: $targetID,
                 pool: .args["action_pool"],
                 action_id: .args["action_id"],
                 max_memory_kb: $maxMemory,
                 user_cpu_time_sec: $userCPUTime,
                 system_cpu_time_sec: $systemCPUTime,
                 total_cpu_time_sec: $totalCPUTime,
                 duration_sec: $duration,
                 used_multiple_cores: $multiCore}]
            """,
        )

    jq(
        name = "%s_worker_usage" % name,
        srcs = ["%s_worker_usage_%s.json" % (name, idx) for idx, src in enumerate(profiles)],
        out = "%s_worker_usage.json" % name,
        args = ["-s"],
        filter = """
        reduce .[] as $item ([]; . + $item)
        """,
    )

    jq(
        name = "%s_worker_resources" % name,
        srcs = [":%s_worker_usage" % name],
        out = "%s_worker_resources.json" % name,
        filter = """
        reduce .[] as $item ({};
           .[$item.target_id] |=
           if . == null then
             {
               "max_memory_kb": $item.max_memory_kb,
               "used_multiple_cores": $item.used_multiple_cores
             }
           else
             {
               "max_memory_kb": (
                 if $item.max_memory_kb > .max_memory_kb then
                   $item.max_memory_kb
                 else
                   .max_memory_kb
                 end),
               "used_multiple_cores": (
                 .used_multiple_cores or $item.used_multiple_cores)
             }
           end
         )
        """,
    )

    jq(
        name = "%s_slowest" % name,
        srcs = [":%s_worker_usage" % name],
        out = "%s_slowest.json" % name,
        filter = """
        reduce .[] as $item ({};
           .[$item.target_id] |=
           if . == null then
             {
               "duration_sec": $item.duration_sec,
             }
           else
             {
               "duration_sec": (
                 if $item.duration_sec > .duration_sec then
                   $item.duration_sec
                 else
                   .duration_sec
                 end),
             }
           end
         )
        | to_entries
        | sort_by(.value.duration_sec)
        | reverse
        | from_entries
        """,
    )

    jq(
        name = "%s_mem_hogs" % name,
        srcs = [":%s_worker_usage" % name],
        out = "%s_mem_hogs.json" % name,
        filter = """
        reduce .[] as $item ({};
           .[$item.target_id] |=
           if . == null then
             {
               "max_memory_kb": $item.max_memory_kb,
             }
           else
             {
               "max_memory_kb": (
                 if $item.max_memory_kb > .max_memory_kb then
                   $item.max_memory_kb
                 else
                   .max_memory_kb
                 end),
             }
           end
         )
        | to_entries
        | sort_by(.value.max_memory_kb)
        | reverse
        | from_entries
        """,
    )

    jq(
        name = "%s_cpu_hogs" % name,
        srcs = [":%s_worker_usage" % name],
        out = "%s_cpu_hogs.json" % name,
        filter = """
        reduce .[] as $item ({};
           .[$item.target_id] |=
           if . == null then
             {
               "total_cpu_time_sec": $item.total_cpu_time_sec,
             }
           else
             {
               "total_cpu_time_sec": (
                 if $item.total_cpu_time_sec > .total_cpu_time_sec then
                   $item.total_cpu_time_sec
                 else
                   .total_cpu_time_sec
                 end),
             }
           end
         )
        | to_entries
        | sort_by(.value.total_cpu_time_sec)
        | reverse
        | from_entries
        """,
    )

    jq(
        name = "%s_report" % name,
        srcs = [
            ":rules_pools",
            ":%s_worker_resources" % name,
        ],
        out = "%s_report.json" % name,
        args = ["-s"],
        filter = """
        .[0] as $pools
        | .[1] as $targets
        | reduce ($targets | keys_unsorted[]) as $target (
          {};
          . + {
            ($target): ($targets[$target] + {
              "pool": (
                $pools
                | to_entries
                | map(select(.value | index($target)))
                | .[0].key)
            })
          }
        )
        """,
    )

    jq(
        name = "%s_pools" % name,
        srcs = [":%s_report" % name],
        out = "%s_pools.json" % name,
        # TODO: make pools/limits dynamic/configurable
        filter = """
        with_entries(
          if (.value.max_memory_kb >= 2097152) then
            .value = {"pool": "2core"}
          else
            empty
          end
        )
        """,
    )

    jq(
        name = "%s_requirements" % name,
        srcs = [":%s_pools" % name],
        out = "%s_requirements.json" % name,
        filter = """
        to_entries
        | group_by(.value.pool)
        | map({(.[0].value.pool): map(.key)})
        | add
        | . // {}
        """,
    )

def ci_changes(
        path,
        sotw,
        requirements,
        buildozer_bin = "@com_github_bazelbuild_buildtools//buildozer",
        update_script = "@envoy_toolshed//profiler:update.sh"):
    jq(
        name = "all_requirements",
        srcs = requirements,
        out = "all_requirements.json",
        args = ["-s"],
        filter = """
        reduce .[] as $item ({};
           if ($item | type == "object") then
             reduce ($item | to_entries[]) as $entry (.;
               .[$entry.key] = (
                 (.[$entry.key] // []) + ($entry.value // [])
                 | unique))
           else
             .
           end)
        """,
    )

    jq(
        name = "changes",
        srcs = [
            sotw,
            ":all_requirements",
        ],
        out = "changes.json",
        args = ["-s"],
        filter = """
        .[0] as $sotw
          | .[1] as $ideal
          | ($ideal
              | to_entries
              | map(.key as $pool
                    | .value as $idealValues
                    | {
                      ($pool): (
                          $idealValues - ($sotw[$pool] // [])
                          | to_entries
                          | map(.value | select(startswith("//")))
                          | unique)
                    }
                  )
              | add // {}) as $add
          | ($sotw
              | to_entries
              | map(.key as $pool
                     | .value as $sotwValues
                     | {
                       ($pool): (
                           $sotwValues - ($ideal[$pool] // [])
                           | to_entries
                           | map(.value)
                           | unique)
                     }
                   )
              | add // {} ) as $remove
          | {$add, $remove}
        """,
    )

    jq(
        name = "commands",
        srcs = [":changes"],
        out = "commands.txt",
        args = ["-r"],
        filter = """
        [
          (.add
            | to_entries[]
            | .key as $key
            | .value[]
            | "dict_set exec_properties Pool:\\($key)|\\(.)"
          ),
          (.remove
            | to_entries[]
            | .key as $key
            | .value[]
            | "dict_remove exec_properties Pool|\\(.)"
          )
        ]
        | flatten
        | .[]
        """,
    )

    native.sh_binary(
        name = "update",
        srcs = [update_script],
        data = [
            buildozer_bin,
            ":commands",
        ],
        env = {
            "BUILDOZER": "$(location %s)" % buildozer_bin,
            "COMMANDS_PATH": "$(location :commands)",
            "REPO_PATH": path,
        },
    )
