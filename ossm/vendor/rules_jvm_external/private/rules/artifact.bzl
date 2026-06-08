load("//private:constants.bzl", "DEFAULT_REPOSITORY_NAME")
load("//private:coursier_utilities.bzl", "strip_packaging_and_classifier_and_version")

def artifact(a, repository_name = DEFAULT_REPOSITORY_NAME):
    artifact_str = _make_artifact_str(a) if type(a) != "string" else a
    return "@%s//:%s" % (repository_name, _escape(strip_packaging_and_classifier_and_version(artifact_str)))

def maven_artifact(a):
    return artifact(a, repository_name = DEFAULT_REPOSITORY_NAME)

def java_plugin_artifact(maven_coords, plugin_class_name, repository_name = DEFAULT_REPOSITORY_NAME):
    return "%s__java_plugin__%s" % (artifact(maven_coords, repository_name), _escape(plugin_class_name))

def _escape(string):
    return string.replace(".", "_").replace("-", "_").replace(":", "_").replace("$", "_")

# inverse of parse_maven_coordinate
def _make_artifact_str(artifact_obj):
    # produce either simplified g:a or standard g:a:[p:[c:]]v Maven coordinate string
    coord = [artifact_obj["group"], artifact_obj["artifact"]]
    if "version" in artifact_obj:
        if "packaging" in artifact_obj:
            coord.extend([artifact_obj["packaging"]])
            if "classifier" in artifact_obj:
                coord.extend([artifact_obj["classifier"]])
        coord.extend([artifact_obj["version"]])
    return ":".join(coord)
