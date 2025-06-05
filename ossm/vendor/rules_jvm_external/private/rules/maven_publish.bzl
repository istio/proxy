MavenPublishInfo = provider(
    fields = {
        "coordinates": "Maven coordinates for the project, which may be None",
        "pom": "Pom.xml file for metdata",
        "javadocs": "Javadoc jar file for documentation files",
        "artifact": "Primary artifact to be published, typically a jar",
        "source_jar": "Jar with the source code for review",
        "classifier_artifacts": "Dict of extra artifacts to be published under classifiers",
    },
)

_TEMPLATE = """#!/usr/bin/env bash
export MAVEN_REPO="${{MAVEN_REPO:-{maven_repo}}}"
export GPG_SIGN="${{GPG_SIGN:-{gpg_sign}}}"
export MAVEN_USER="${{MAVEN_USER:-{user}}}"
export MAVEN_PASSWORD="${{MAVEN_PASSWORD:-{password}}}"
export USE_IN_MEMORY_PGP_KEYS="${{USE_IN_MEMORY_PGP_KEYS:-{use_in_memory_pgp_keys}}}"
export PGP_SIGNING_KEY="${{PGP_SIGNING_KEY:-{pgp_signing_key}}}"
export PGP_SIGNING_PWD="${{PGP_SIGNING_PWD:-{pgp_signing_pwd}}}"
echo Uploading "{coordinates}" to "${{MAVEN_REPO}}"
{uploader} "{coordinates}" '{pom}' '{artifact}' '{classifier_artifacts}' $@
"""

def _escape_arg(str):
    # Escape a string that will be double quoted in bash and might contain double quotes.
    return str.replace('"', "\\\"").replace("$", "\\$")

def _maven_publish_impl(ctx):
    executable = ctx.actions.declare_file("%s-publisher" % ctx.attr.name)

    maven_repo = ctx.var.get("maven_repo", "")
    gpg_sign = ctx.var.get("gpg_sign", "false")
    user = ctx.var.get("maven_user", "")
    password = ctx.var.get("maven_password", "")
    use_in_memory_pgp_keys = ctx.var.get("use_in_memory_pgp_keys", "'false'")
    pgp_signing_key = ctx.var.get("pgp_signing_key", "''")
    pgp_signing_pwd = ctx.var.get("pgp_signing_pwd", "''")
    if password:
        print("WARNING: using --define to set maven_password is insecure. Set env var MAVEN_PASSWORD=xxx instead.")

    # Expand maven coordinates for any variables to be replaced.
    coordinates = ctx.expand_make_variables("coordinates", ctx.attr.coordinates, {})
    artifacts_short_path = ctx.file.artifact.short_path if ctx.file.artifact else ""

    classifier_artifacts_dict = {}
    for target, classifier in ctx.attr.classifier_artifacts.items():
        target_files = target.files.to_list()
        if not target_files:
            fail("Target {} for classifier \"{}\" of {} has no files in its output.".format(target, classifier, coordinates))
        if len(target_files) > 1:
            print("WARNING: Target {} for classifier \"{}\" of {} has more than one file in its output, using the first one.".format(target, classifier, coordinates))
        file = target_files[0]
        classifier_artifacts_dict[classifier] = file

    ctx.actions.write(
        output = executable,
        is_executable = True,
        content = _TEMPLATE.format(
            uploader = ctx.executable._uploader.short_path,
            coordinates = _escape_arg(coordinates),
            gpg_sign = _escape_arg(gpg_sign),
            maven_repo = _escape_arg(maven_repo),
            password = _escape_arg(password),
            use_in_memory_pgp_keys = _escape_arg(use_in_memory_pgp_keys),
            pgp_signing_key = _escape_arg(pgp_signing_key),
            pgp_signing_pwd = _escape_arg(pgp_signing_pwd),
            user = _escape_arg(user),
            pom = ctx.file.pom.short_path,
            artifact = artifacts_short_path,
            classifier_artifacts = ",".join(["{}={}".format(classifier, file.short_path) for (classifier, file) in classifier_artifacts_dict.items()]),
        ),
    )

    files = [ctx.file.pom] + ctx.files.classifier_artifacts
    if ctx.file.artifact:
        files.append(ctx.file.artifact)

    return [
        DefaultInfo(
            files = depset([executable]),
            executable = executable,
            runfiles = ctx.runfiles(
                files = files,
                collect_data = True,
            ).merge(ctx.attr._uploader[DefaultInfo].data_runfiles),
        ),
        MavenPublishInfo(
            coordinates = coordinates,
            artifact = ctx.file.artifact,
            classifier_artifacts = classifier_artifacts_dict,
            javadocs = classifier_artifacts_dict.get("javadoc"),
            source_jar = classifier_artifacts_dict.get("sources"),
            pom = ctx.file.pom,
        ),
    ]

maven_publish = rule(
    _maven_publish_impl,
    doc = """Publish artifacts to a maven repository.

The maven repository may accessed locally using a `file://` URL, or
remotely using an `https://` URL. The following flags may be set
using `--define` or via environment variables (in all caps, e.g. `MAVEN_REPO`):

  gpg_sign: Whether to sign artifacts using GPG
  maven_repo: A URL for the repo to use. May be "https" or "file".
  maven_user: The user name to use when uploading to the maven repository.
  maven_password: The password to use when uploading to the maven repository.
  use_in_memory_pgp_keys: Whether to sign artifacts using in memory PGP secrets
  pgp_signing_key = The secret key to sign the artifact with
  pgp_signing_pwd = Password for the secret key

When signing with GPG, the current default key is used.
""",
    executable = True,
    attrs = {
        "coordinates": attr.string(
            mandatory = True,
        ),
        "pom": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "artifact": attr.label(
            allow_single_file = True,
        ),
        "classifier_artifacts": attr.label_keyed_string_dict(allow_files = True),
        "_uploader": attr.label(
            executable = True,
            cfg = "exec",
            default = "//private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:MavenPublisher",
            allow_files = True,
        ),
    },
)
