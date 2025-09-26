# Aspect Workflows demonstration deployment

This deployment of [Aspect Workflows](https://www.aspect.build/workflows) is configured to run on Buildkite.

You can see this Aspect Workflows demonstration deployment live at
https://buildkite.com/aspect/rules-js.

The two components of the configuration for this repository are,

1. Aspect Workflows configuration yaml
1. Buildkite pipeline configuration (in the Buildkite UI)

## Aspect Workflows configuration yaml

This is the [config.yaml](./config.yaml) file in this directory.

## Buildkite pipeline configuration (in the Buildkite UI)

There are two pipelines configured on Buildkite.

1. Main build & test pipeline: https://buildkite.com/aspect/rules-js
2. Scheduled warming pipeline: https://buildkite.com/aspect/rules-js-warming

### Main build & test pipeline configuration

The main build & test pipeline found at https://buildkite.com/aspect/rules-js is configured
with the following yaml steps:

```
steps:
  - label: ":aspect: Setup Aspect Workflows"
    commands:
      - "rosetta steps | buildkite-agent pipeline upload"
    agents:
      queue: aspect-small
```

> [!IMPORTANT]
> The Setup Aspect Workflows step above is configured to run on the `aspect-small` queue that is serviced by a runner group made up of lightweight and inexpensive `e2.small` GCP VM instances.

### Scheduled warming pipeline configuration

The scheduled warming pipeline found at https://buildkite.com/aspect/rules-js-warming is
configured with the following yaml steps:

```
env:
  ASPECT_WORKFLOWS_BIN_DIR: /etc/aspect/workflows/bin
steps:
  - label: ":fire: Create warming archives"
    commands: |
      echo "--- :aspect-build: Configure environment"
      ${ASPECT_WORKFLOWS_BIN_DIR}/configure_workflows_env
      echo "--- :stethoscope: Agent health checks"
      ${ASPECT_WORKFLOWS_BIN_DIR}/agent_health_check
      echo "--- :bazel: Create warming archive"
      rosetta run warming
      ${ASPECT_WORKFLOWS_BIN_DIR}/warming_archive
    agents:
      queue: aspect-warming
```

The warming pipeline is not configured to trigger on commits or PRs. Instead, a scheduled is
configured for this pipeline with the cron interval `0 1 * * * America/Vancouver` so that it
runs periodically to create up-to-date warming archives that caches repository rules so that the
"default" build & test runners don't have to re-fetch them on their first build.
