# Function for organizing targets into logical groups.
#
# Description:
#   This function add the target to a group of targets. If the group does not exist yet, it will create
#   a custom target. The group acts as a meta-target that does not build anything itself but depends on 
#   other targets.
#
# Parameters:
#   TARGET_TO_ADD (string): The name of the target to be added to the group.
#   GROUP (string): The name of the group target to which the target will be added.
#
# Usage:
#   ADD_TARGET_TO_GROUP(<TARGET_TO_ADD> <GROUP>)
function (ADD_TARGET_TO_GROUP TARGET_TO_ADD GROUP)
  if (NOT TARGET ${GROUP})
    add_custom_target(${GROUP})
  endif ()

  message(STATUS "Adding target ${TARGET_TO_ADD} to ${GROUP} group")
  add_dependencies(${GROUP} ${TARGET_TO_ADD})
endfunction ()

