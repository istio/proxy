#
# Utilites for working with artifacts
#

load("//:specs.bzl", "utils")

def deduplicate_and_sort_artifacts(dep_tree, artifacts, excluded_artifacts, verbose):
    # The deps json returned from coursier can have duplicate artifacts with
    # different dependencies and exclusions. We want to de-duplicate the
    # artifacts and chose the ones that most closely match the exclusions
    # specified in the maven_install declaration and not chose ones with
    # empty dependencies if possible

    # First we find all of the artifacts that have exclusions
    artifacts_with_exclusions = {}
    for a in artifacts:
        coordinate = utils.artifact_coordinate(a)
        if "exclusions" in a and len(a["exclusions"]) > 0:
            deduped_exclusions = {}
            for e in excluded_artifacts:
                deduped_exclusions["{}:{}".format(e["group"], e["artifact"])] = True
            for e in a["exclusions"]:
                if e["group"] == "*" and e["artifact"] == "*":
                    deduped_exclusions = {"*:*": True}
                    break
                deduped_exclusions["{}:{}".format(e["group"], e["artifact"])] = True
            artifacts_with_exclusions[coordinate] = deduped_exclusions.keys()

    # As we de-duplicate the list keep the duplicate artifacts with exclusions separate
    # so we can look at them and select the one that has the same exclusions
    # Also prefer the duplicates with non-empty dependency lists
    duplicate_artifacts_with_exclusions = {}
    deduped_artifacts = {}
    null_artifacts = []
    for artifact in dep_tree["dependencies"]:
        if artifact["file"] == None:
            null_artifacts.append(artifact)
            continue
        if artifact["coord"] in artifacts_with_exclusions:
            if artifact["coord"] in duplicate_artifacts_with_exclusions:
                duplicate_artifacts_with_exclusions[artifact["coord"]].append(artifact)
            else:
                duplicate_artifacts_with_exclusions[artifact["coord"]] = [artifact]
        elif artifact["file"] in deduped_artifacts:
            if len(artifact["dependencies"]) > 0 and len(deduped_artifacts[artifact["file"]]["dependencies"]) == 0:
                deduped_artifacts[artifact["file"]] = artifact
        else:
            deduped_artifacts[artifact["file"]] = artifact

    # Look through the duplicates with exclusions and try to select the artifact
    # that has the same exclusions as specified in the artifact and
    # prefer the one with non-empty dependencies
    for duplicate_coord in duplicate_artifacts_with_exclusions:
        deduped_artifact_with_exclusion = duplicate_artifacts_with_exclusions[duplicate_coord][0]
        found_artifact_with_exclusion = False
        for duplicate_artifact in duplicate_artifacts_with_exclusions[duplicate_coord]:
            if "exclusions" in duplicate_artifact and sorted(duplicate_artifact["exclusions"]) == sorted(artifacts_with_exclusions[duplicate_coord]):
                if not found_artifact_with_exclusion:
                    found_artifact_with_exclusion = True
                    deduped_artifact_with_exclusion = duplicate_artifact
                elif len(duplicate_artifact["dependencies"]) > 0 and len(deduped_artifact_with_exclusion["dependencies"]) == 0:
                    deduped_artifact_with_exclusion = duplicate_artifact
        if verbose and not found_artifact_with_exclusion:
            print(
                "Could not find duplicate artifact with matching exclusions for {} when de-duplicating the dependency tree. Using exclusions {}"
                    .format(duplicate_coord, artifacts_with_exclusions[duplicate_coord]),
            )
        deduped_artifacts[deduped_artifact_with_exclusion["file"]] = deduped_artifact_with_exclusion

    # After we have added the de-duped artifacts with exclusions we need to re-sort the list
    sorted_deduped_values = []
    for key in sorted(deduped_artifacts.keys()):
        sorted_deduped_values.append(deduped_artifacts[key])

    dep_tree.update({"dependencies": sorted_deduped_values + null_artifacts})

    return dep_tree
