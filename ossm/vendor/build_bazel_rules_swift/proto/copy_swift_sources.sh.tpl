#!/bin/sh

# Recursively copy the Swift sources from the temporary directory to the permanent one:
cp -RL {temporary_output_directory_path}/** {permanent_output_directory_path}/

# Touch all of the declared Swift sources to create an empty file if the plugin didn't generate it:
touch {swift_source_file_paths}
