# Copyright 2018 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""ACTool related actions."""

load(
    "@bazel_skylib//lib:collections.bzl",
    "collections",
)
load(
    "@bazel_skylib//lib:paths.bzl",
    "paths",
)
load(
    "@bazel_skylib//lib:sets.bzl",
    "sets",
)
load(
    "@build_bazel_apple_support//lib:apple_support.bzl",
    "apple_support",
)
load(
    "//apple:utils.bzl",
    "group_files_by_directory",
)
load(
    "//apple/internal:apple_product_type.bzl",
    "apple_product_type",
)
load(
    "//apple/internal:intermediates.bzl",
    "intermediates",
)
load(
    "//apple/internal/utils:xctoolrunner.bzl",
    xctoolrunner_support = "xctoolrunner",
)

_supports_visionos = hasattr(apple_common.platform_type, "visionos")

def _actool_args_for_special_file_types(
        *,
        asset_files,
        bundle_id,
        platform_prerequisites,
        primary_icon_name,
        product_type):
    """Returns command line arguments needed to compile special assets.

    This function is called by `actool` to scan for specially recognized asset
    types, such as app icons and launch images, and determine any extra command
    line arguments that need to be passed to `actool` to handle them. It also
    checks the validity of those assets, if any (for example, by permitting only
    one app icon set or launch image set to be present).

    Args:
      asset_files: The asset catalog files.
      bundle_id: The bundle ID to configure for this target.
      platform_prerequisites: Struct containing information on the platform being targeted.
      primary_icon_name: An optional String to identify the name of the primary app icon when
        alternate app icons have been provided for the app.
      product_type: The product type identifier used to describe the current bundle type.

    Returns:
      An array of extra arguments to pass to `actool`, which may be empty.
    """
    args = []

    if product_type in (
        apple_product_type.messages_extension,
        apple_product_type.messages_sticker_pack_extension,
    ):
        appicon_extension = "stickersiconset"
        icon_files = [f for f in asset_files if ".stickersiconset/" in f.path]

        args.extend([
            "--include-sticker-content",
            "--stickers-icon-role",
            "extension",
            "--sticker-pack-identifier-prefix",
            bundle_id + ".sticker-pack.",
        ])

        # Fail if the user has included .appiconset folders in their asset catalog;
        # Message extensions must use .stickersiconset instead.
        #
        # NOTE: This is mostly caught via the validation in ios_extension of the
        # app_icons attribute; however, since different resource attributes from
        # *_library targets are merged into here, other resources could show up
        # so until the resource handing is revisited (b/77804841), things could
        # still show up that don't make sense.
        appiconset_files = [f for f in asset_files if ".appiconset/" in f.path]
        if appiconset_files:
            appiconset_dirs = group_files_by_directory(
                appiconset_files,
                ["appiconset"],
                attr = "app_icons",
            ).keys()
            formatted_dirs = "[\n  %s\n]" % ",\n  ".join(appiconset_dirs)
            fail("Message extensions must use Messages Extensions Icon Sets " +
                 "(named .stickersiconset), not traditional App Icon Sets " +
                 "(.appiconset). Found the following: " +
                 formatted_dirs, "app_icons")

    elif platform_prerequisites.platform_type == apple_common.platform_type.tvos:
        appicon_extension = "brandassets"
        icon_files = [f for f in asset_files if ".brandassets/" in f.path]
    elif (_supports_visionos and
          platform_prerequisites.platform_type == apple_common.platform_type.visionos):
        appicon_extension = "solidimagestack"
        icon_files = [f for f in asset_files if ".solidimagestack/" in f.path]
    else:
        appicon_extension = "appiconset"
        icon_files = [f for f in asset_files if ".appiconset/" in f.path]

    # Add arguments for app icons, if there are any.
    if icon_files:
        icon_dirs = group_files_by_directory(
            icon_files,
            [appicon_extension],
            attr = "app_icons",
        ).keys()
        if len(icon_dirs) != 1 and not primary_icon_name:
            formatted_dirs = "[\n  %s\n]" % ",\n  ".join(icon_dirs)

            # Alternate icons are only supported for UIKit applications on iOS, tvOS, visionOS and
            # iOS-on-macOS (Catalyst)
            if (platform_prerequisites.platform_type == apple_common.platform_type.watchos or
                platform_prerequisites.platform_type == apple_common.platform_type.macos or
                product_type != apple_product_type.application):
                fail("The asset catalogs should contain exactly one directory named " +
                     "*.%s among its asset catalogs, " % appicon_extension +
                     "but found the following: " + formatted_dirs, "app_icons")
            else:
                fail("""
Found multiple app icons among the asset catalogs with no primary_app_icon assigned.

If you intend to assign multiple app icons to this target, please declare which of these is intended
to be the primary app icon with the primary_app_icon attribute on the rule itself.

app_icons was assigned the following: {formatted_dirs}
""".format(formatted_dirs = formatted_dirs))
        elif primary_icon_name:
            # Check that primary_icon_name matches one of the icon sets, then add actool arguments
            # for `--alternate-app-icon` and `--app_icon` as appropriate. These do NOT overlap.
            app_icon_names = sets.make()
            for icon_dir in icon_dirs:
                app_icon_names = sets.insert(
                    app_icon_names,
                    paths.split_extension(paths.basename(icon_dir))[0],
                )
            app_icon_name_list = sets.to_list(app_icon_names)
            found_primary = False
            for app_icon_name in app_icon_name_list:
                if app_icon_name == primary_icon_name:
                    found_primary = True
                    args += ["--app-icon", primary_icon_name]
                else:
                    args += ["--alternate-app-icon", app_icon_name]
            if not found_primary:
                fail("""
Could not find the primary icon named "{primary_icon_name}" in the list of app_icons provided.

Found the following icon names from those provided: {app_icon_names}.
""".format(
                    primary_icon_name = primary_icon_name,
                    app_icon_names = ", ".join(app_icon_name_list),
                ))
        else:
            app_icon_name = paths.split_extension(paths.basename(icon_dirs[0]))[0]
            args += ["--app-icon", app_icon_name]

    # Add arguments for watch extension complication, if there is one.
    complication_files = [f for f in asset_files if ".complicationset/" in f.path]
    if product_type == apple_product_type.watch2_extension and complication_files:
        args += ["--complication", "Complication"]

    # Add arguments for launch images, if there are any.
    launch_image_files = [f for f in asset_files if ".launchimage/" in f.path]
    if launch_image_files:
        launch_image_dirs = group_files_by_directory(
            launch_image_files,
            ["launchimage"],
            attr = "launch_images",
        ).keys()
        if len(launch_image_dirs) != 1:
            formatted_dirs = "[\n  %s\n]" % ",\n  ".join(launch_image_dirs)
            fail("The asset catalogs should contain exactly one directory named " +
                 "*.launchimage among its asset catalogs, but found the " +
                 "following: " + formatted_dirs, "launch_images")

        launch_image_name = paths.split_extension(
            paths.basename(launch_image_dirs[0]),
        )[0]
        args += ["--launch-image", launch_image_name]

    return args

def _alticonstool_args(
        *,
        actions,
        alticons_files,
        input_plist,
        output_plist,
        device_families):
    alticons_dirs = group_files_by_directory(
        alticons_files,
        ["alticon"],
        attr = "alternate_icons",
    ).keys()
    args = actions.args()
    args.add_all([
        "--input",
        input_plist,
        "--output",
        output_plist,
        "--families",
        ",".join(device_families),
    ])
    args.add_all(alticons_dirs, before_each = "--alticon")
    return [args]

def compile_asset_catalog(
        *,
        actions,
        alternate_icons,
        alticonstool,
        asset_files,
        bundle_id,
        output_dir,
        output_plist,
        platform_prerequisites,
        primary_icon_name,
        product_type,
        rule_label,
        xctoolrunner):
    """Creates an action that compiles asset catalogs.

    This action populates a directory with compiled assets that must be merged
    into the application/extension bundle. It also produces a partial Info.plist
    that must be merged info the application's main plist if an app icon or
    launch image are requested (if not, the actool plist is empty).

    Args:
      actions: The actions provider from `ctx.actions`.
      alternate_icons: Alternate icons files, organized in .alticon directories.
      alticonstool: A files_to_run for the alticonstool tool.
      asset_files: An iterable of files in all asset catalogs that should be
          packaged as part of this catalog. This should include transitive
          dependencies (i.e., assets not just from the application target, but
          from any other library targets it depends on) as well as resources like
          app icons and launch images.
      bundle_id: The bundle ID to configure for this target.
      output_dir: The directory where the compiled outputs should be placed.
      output_plist: The file reference for the output plist that should be merged
        into Info.plist. May be None if the output plist is not desired.
      platform_prerequisites: Struct containing information on the platform being targeted.
      primary_icon_name: An optional String to identify the name of the primary app icon when
        alternate app icons have been provided for the app.
      product_type: The product type identifier used to describe the current bundle type.
      rule_label: The label of the target being analyzed.
      xctoolrunner: A files_to_run for the wrapper around the "xcrun" tool.
    """
    platform = platform_prerequisites.platform
    actool_platform = platform.name_in_plist.lower()

    args = [
        "actool",
        "--compile",
        xctoolrunner_support.prefixed_path(output_dir.path),
        "--platform",
        actool_platform,
        "--minimum-deployment-target",
        platform_prerequisites.minimum_os,
        "--compress-pngs",
    ]

    args.extend(_actool_args_for_special_file_types(
        asset_files = asset_files,
        bundle_id = bundle_id,
        platform_prerequisites = platform_prerequisites,
        primary_icon_name = primary_icon_name,
        product_type = product_type,
    ))
    args.extend(collections.before_each(
        "--target-device",
        platform_prerequisites.device_families,
    ))

    alticons_outputs = []
    actool_output_plist = None
    actool_outputs = [output_dir]
    if output_plist:
        if alternate_icons:
            alticons_outputs = [output_plist]
            actool_output_plist = intermediates.file(
                actions = actions,
                target_name = rule_label.name,
                output_discriminator = None,
                file_name = "{}.noalticon.plist".format(output_plist.basename),
            )
        else:
            actool_output_plist = output_plist

        actool_outputs.append(actool_output_plist)
        args.extend([
            "--output-partial-info-plist",
            xctoolrunner_support.prefixed_path(actool_output_plist.path),
        ])

    xcassets = group_files_by_directory(
        asset_files,
        ["xcassets", "xcstickers"],
        attr = "asset_catalogs",
    ).keys()

    args.extend([xctoolrunner_support.prefixed_path(xcasset) for xcasset in xcassets])

    apple_support.run(
        actions = actions,
        arguments = args,
        apple_fragment = platform_prerequisites.apple_fragment,
        executable = xctoolrunner,
        execution_requirements = {"no-sandbox": "1"},
        inputs = asset_files,
        mnemonic = "AssetCatalogCompile",
        outputs = actool_outputs,
        xcode_config = platform_prerequisites.xcode_version_config,
    )

    if alternate_icons:
        apple_support.run(
            actions = actions,
            apple_fragment = platform_prerequisites.apple_fragment,
            arguments = _alticonstool_args(
                actions = actions,
                input_plist = actool_output_plist,
                output_plist = output_plist,
                alticons_files = alternate_icons,
                device_families = platform_prerequisites.device_families,
            ),
            executable = alticonstool,
            inputs = [actool_output_plist] + alternate_icons,
            mnemonic = "AlternateIconsInsert",
            outputs = alticons_outputs,
            xcode_config = platform_prerequisites.xcode_version_config,
        )
