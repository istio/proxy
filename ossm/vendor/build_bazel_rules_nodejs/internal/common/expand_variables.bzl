# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Helper functions to expand "make" variables of form $(VAR)
"""

def expand_variables(ctx, s, outs = [], output_dir = False, attribute_name = "args"):
    """This function is the same as ctx.expand_make_variables with the additional
    genrule-like substitutions of:

      - $@: The output file if it is a single file. Else triggers a build error.
      - $(@D): The output directory. If there is only one file name in outs,
               this expands to the directory containing that file. If there are multiple files,
               this instead expands to the package's root directory in the bin tree,
               even if all generated files belong to the same subdirectory!
      - $(RULEDIR): The output directory of the rule, that is, the directory
        corresponding to the name of the package containing the rule under the bin tree.

    See https://docs.bazel.build/versions/main/be/general.html#genrule.cmd and
    https://docs.bazel.build/versions/main/be/make-variables.html#predefined_genrule_variables
    for more information of how these special variables are expanded.
    """
    rule_dir = [f for f in [
        ctx.bin_dir.path,
        ctx.label.workspace_root,
        ctx.label.package,
    ] if f]
    additional_substitutions = {}

    if output_dir:
        if s.find("$@") != -1 or s.find("$(@)") != -1:
            fail("""$@ substitution may only be used with output_dir=False.
            Upgrading rules_nodejs? Maybe you need to switch from $@ to $(@D)
            See https://github.com/bazelbuild/rules_nodejs/releases/tag/0.42.0""")

        # We'll write into a newly created directory named after the rule
        output_dir = [f for f in [
            ctx.bin_dir.path,
            ctx.label.workspace_root,
            ctx.label.package,
            ctx.label.name,
        ] if f]
    else:
        if s.find("$@") != -1 or s.find("$(@)") != -1:
            if len(outs) > 1:
                fail("""$@ substitution may only be used with a single out
                Upgrading rules_nodejs? Maybe you need to switch from $@ to $(RULEDIR)
                See https://github.com/bazelbuild/rules_nodejs/releases/tag/0.42.0""")
        if len(outs) == 1:
            additional_substitutions["@"] = outs[0].path
            output_dir = outs[0].dirname.split("/")
        else:
            output_dir = rule_dir[:]

    # The list comprehension removes empty segments like if we are in the root package
    additional_substitutions["@D"] = "/".join([o for o in output_dir if o])
    additional_substitutions["RULEDIR"] = "/".join([o for o in rule_dir if o])

    return ctx.expand_make_variables(attribute_name, s, additional_substitutions)
