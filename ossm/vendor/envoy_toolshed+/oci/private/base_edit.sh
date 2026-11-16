#!/usr/bin/env bash
#
# Edit the system files of an OCI base image, as package and user management tools would, without
# running anything in the image.
#
# Reads `/etc/{passwd,group,shadow,gshadow}` and `/var/lib/dpkg/status` from the layers of the
# image, adds the users and groups of the spec, and the dpkg status of the packages, and writes a
# layer with the edited files, and the new home directories.
#
# Usage: base_edit.sh <image layout> <spec json> <output tar> [<deb control tar>...]
#
# Requires `JQ` and `TAR` (bsdtar).

set -euo pipefail
# Fail on errors in command substitutions too, where bash supports it (>= 4.4). Failures that
# matter are also propagated explicitly.
shopt -s inherit_errexit 2> /dev/null || :

LAYOUT="$1"
SPEC="$2"
OUT="$3"
shift 3
CONTROLS=("$@")

# Allocation ranges, as `adduser` uses them for system users and groups.
FIRST_SYSTEM_ID=100
LAST_SYSTEM_ID=999

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ROOT="${WORK}/root"
mkdir -p "$ROOT"

fail () {
    echo "ERROR: $*" >&2
    exit 1
}

blob () {
    local digest="$1"
    echo "${LAYOUT}/blobs/${digest%%:*}/${digest#*:}"
}

# The image manifest. An index must have a single entry (ie a single platform image).
descriptor="$("$JQ" -c '.manifests | if length != 1 then error("expected a single manifest, got \(length)") else .[0] end' "${LAYOUT}/index.json")"
while [[ "$("$JQ" -r '.mediaType' <<< "$descriptor")" == *index* ]]; do
    descriptor="$("$JQ" -c '.manifests | if length != 1 then error("expected a single manifest, got \(length)") else .[0] end' "$(blob "$("$JQ" -r '.digest' <<< "$descriptor")")")"
done
MANIFEST="$(blob "$("$JQ" -r '.digest' <<< "$descriptor")")"

# Extract the files from the layers in order, so that each file is as it is in the image. For each
# layer, first apply its whiteouts (of the file, or of any of its parent directories, or opaque
# markers of its parent directories) to the files of the lower layers, then extract its files.
FILES=(etc/passwd etc/group etc/shadow etc/gshadow var/lib/dpkg/status)
# The paths in the image (for new homes and their parent directories): those of each layer, after
# applying its whiteouts to those of the lower layers.
PATHS="${WORK}/paths"
: > "$PATHS"

update_paths () {
    # update_paths <entries>: apply the whiteouts of the layer entries to the paths, and add the
    # other entries.
    if [[ ! -s "$1" ]]; then
        return
    fi
    awk '
        FNR == NR {
            n = split($0, parts, "/")
            name = parts[n]
            dir = (n > 1) ? substr($0, 1, length($0) - length(name)) : ""
            if (name == ".wh..wh..opq") {
                opaque[dir] = 1
            } else if (substr(name, 1, 4) == ".wh.") {
                deleted[dir substr(name, 5)] = 1
            } else {
                # The entry, and its parent directories (which unpacking it creates).
                path = ""
                for (i = 1; i <= n; i++) {
                    path = (i == 1) ? parts[i] : path "/" parts[i]
                    entries[path] = 1
                }
            }
            next
        }
        {
            for (path in deleted) {
                if ($0 == path || substr($0, 1, length(path) + 1) == path "/") next
            }
            for (dir in opaque) {
                if (dir == "" || substr($0, 1, length(dir)) == dir) next
            }
            print
        }
        END {
            for (entry in entries) print entry
        }' "$1" "$PATHS" | sort -u > "${PATHS}.new"
    mv "${PATHS}.new" "$PATHS"
}

# The symlinks in the image, kept as the paths are (whiteouts of each layer applied, and entries of
# each layer replacing those of the lower layers).
LINKS="${WORK}/links"
: > "$LINKS"

update_links () {
    # update_links <entries> <layer links>
    awk '
        FILENAME == ARGV[1] {
            n = split($0, parts, "/")
            name = parts[n]
            dir = (n > 1) ? substr($0, 1, length($0) - length(name)) : ""
            if (name == ".wh..wh..opq") {
                opaque[dir] = 1
            } else if (substr(name, 1, 4) == ".wh.") {
                deleted[dir substr(name, 5)] = 1
            } else {
                replaced[$0] = 1
            }
            next
        }
        FILENAME == ARGV[2] {
            links[$0] = 1
            next
        }
        {
            if ($0 in replaced) next
            for (path in deleted) {
                if ($0 == path || substr($0, 1, length(path) + 1) == path "/") next
            }
            for (dir in opaque) {
                if (dir == "" || substr($0, 1, length(dir)) == dir) next
            }
            print
        }
        END {
            for (link in links) print link
        }' "$1" "$2" "$LINKS" | sort -u > "${LINKS}.new"
    mv "${LINKS}.new" "$LINKS"
}

whited_out () {
    # whited_out <entries> <file>: whether the layer entries delete the file of a lower layer
    local entries="$1" file="$2" path="" part
    local -a parts
    IFS=/ read -ra parts <<< "$file"
    for part in "${parts[@]}"; do
        if awk -v wh="${path}.wh.${part}" -v opq="${path}.wh..wh..opq" \
               '$0 == wh || $0 == opq {found = 1} END {exit !found}' \
               "$entries"; then
            return 0
        fi
        path="${path}${part}/"
    done
    return 1
}

# The state of each tracked path, as `<file> file|dir|deleted`: the last line of a path is its state in
# the image.
STATE="${WORK}/state"
: > "$STATE"

# The mode and owner of each tracked file, as `<file> <mode> <uid> <gid>` (or `<file> -` once
# deleted): the last line of a file is its metadata in the image.
META="${WORK}/meta"
: > "$META"

file_meta () {
    # file_meta <file> <default mode> <default uid> <default gid>: the metadata of the file in the
    # image, or the defaults.
    awk -v file="$1" -v defaults="$2 $3 $4" '
        $1 == file {meta = ($2 == "-") ? "" : $2 " " $3 " " $4}
        END {print (meta == "") ? defaults : meta}' "$META"
}

# Read lists into files first (not process substitution), so that failures stop the action.
"$JQ" -r '.layers[].digest' "$MANIFEST" > "${WORK}/layers"
[[ -s "${WORK}/layers" ]] || fail "No layers in the image"
while read -r digest; do
    layer="$(blob "$digest")"
    "$TAR" -tf "$layer" > "${WORK}/names"
    sed -e 's#^\./##' -e 's#/$##' "${WORK}/names" > "${WORK}/entries"
    update_paths "${WORK}/entries"
    # The symlinks of the layer.
    "$TAR" -cf - --format=mtree --options '!all,type,!use-set' "@${layer}" \
        | awk '/^#/ || NF == 0 {next} $2 == "type=link" {name = $1; sub(/^\.\//, "", name); sub(/\/$/, "", name); print name}' \
        > "${WORK}/layer_links"
    if [[ -s "${WORK}/entries" ]]; then
        update_links "${WORK}/entries" "${WORK}/layer_links"
    fi
    # Only the files in this layer, by their names in it: bsdtar fails on a pattern that matches
    # nothing.
    INCLUDES=()
    for file in "${FILES[@]}"; do
        if whited_out "${WORK}/entries" "$file"; then
            rm -f "${ROOT}/${file}"
            echo "${file} -" >> "$META"
            echo "${file} deleted" >> "$STATE"
        fi
        # A directory at the path (`<file>/`, or only implied by entries under it) replaces the file
        # of a lower layer.
        if awk -v file="$file" '$0 == file || $0 == "./" file {found = 1} END {exit !found}' "${WORK}/names"; then
            echo "${file} file" >> "$STATE"
        elif awk -v file="$file" 'index($0 "/", file "/") == 1 {found = 1} END {exit !found}' "${WORK}/entries"; then
            rm -rf "${ROOT:?}/${file}"
            echo "${file} dir" >> "$STATE"
        fi
        awk -v file="$file" '$0 == file || $0 == "./" file' "${WORK}/names" > "${WORK}/matches"
        while read -r name; do
            INCLUDES+=(--include "$name")
        done < "${WORK}/matches"
    done
    if [[ "${#INCLUDES[@]}" -gt 0 ]]; then
        "$TAR" -xf "$layer" -C "$ROOT" "${INCLUDES[@]}"
        # Their numeric owners and octal modes, from the layer as mtree.
        "$TAR" -cf - --format=mtree --options '!all,type,uid,gid,mode,!use-set' \
                "${INCLUDES[@]}" "@${layer}" \
            | awk '
                /^#/ || NF == 0 {next}
                {
                    name = $1; sub(/^\.\//, "", name)
                    mode = ""; uid = ""; gid = ""
                    for (i = 2; i <= NF; i++) {
                        split($i, kv, "=")
                        if (kv[1] == "mode") mode = kv[2]
                        if (kv[1] == "uid") uid = kv[2]
                        if (kv[1] == "gid") gid = kv[2]
                    }
                    if (mode != "" && uid != "" && gid != "") print name, "0" mode, uid, gid
                }' >> "$META"
    fi
done < "${WORK}/layers"

for file in "${FILES[@]}"; do
    # A file under a symlinked directory is not in the layers at its path, and would be read as
    # absent (and replaced with only the new entries): reject these layouts.
    path=""
    IFS=/ read -ra parts <<< "$file"
    for part in "${parts[@]}"; do
        path="${path:+${path}/}${part}"
        if awk -v path="$path" '$0 == path {found = 1} END {exit !found}' "$LINKS"; then
            fail "${path} is a symlink in the image (${file} can not be edited)"
        fi
    done
    if [[ "$(awk -v file="$file" '$1 == file {state = $2} END {print state}' "$STATE")" == "dir" ]]; then
        fail "${file} is a directory in the image"
    fi
    # The files are read and written: only regular files (never follow a link out of the image root).
    if [[ -L "${ROOT}/${file}" ]] || [[ -e "${ROOT}/${file}" && ! -f "${ROOT}/${file}" ]]; then
        fail "${file} is not a regular file in the image"
    fi
    mkdir -p "$(dirname "${ROOT}/${file}")"
    touch "${ROOT}/${file}"
    chmod u+rw "${ROOT}/${file}"
    # Entries are appended: make sure the last entry of the image is terminated.
    if [[ -s "${ROOT}/${file}" && "$(tail -c 1 "${ROOT}/${file}" | wc -l)" -eq 0 ]]; then
        echo >> "${ROOT}/${file}"
    fi
done

free_id () {
    # free_id <file>: the first system id that is not used (3rd field) in the passwd or group file
    awk -F: -v first="$FIRST_SYSTEM_ID" -v last="$LAST_SYSTEM_ID" '
        # Ids are compared as numbers (eg `0100` in the image is 100).
        $3 ~ /^[0-9]+$/ {used[$3 + 0] = 1}
        END {
            for (id = first; id <= last; id++) {
                if (!(id in used)) {print id; exit 0}
            }
            exit 1
        }' "$1" || fail "No free system id in ${1#"${ROOT}/"}"
}

has_entry () {
    # has_entry <file> <name>: whether the passwd or group file has an entry of the name
    awk -F: -v name="$2" '$1 == name {found = 1} END {exit !found}' "$1"
}

id_used () {
    # id_used <file> <id>: whether the passwd or group file has an entry with the id
    awk -F: -v id="$2" '$3 ~ /^[0-9]+$/ && $3 + 0 == id + 0 {found = 1} END {exit !found}' "$1"
}

# Days since the epoch for new shadow entries. Use those of `root`, so that the output only depends
# on the image.
LASTCHG="$(awk -F: '$1 == "root" {print $3}' "${ROOT}/etc/shadow")"

add_group () {
    # add_group <name> [<gid>]
    local name="$1" gid="${2:-}"
    if has_entry "${ROOT}/etc/group" "$name"; then
        fail "Group ${name} exists in the image"
    fi
    if [[ -z "$gid" ]]; then
        gid="$(free_id "${ROOT}/etc/group")" || return 1
    elif id_used "${ROOT}/etc/group" "$gid"; then
        fail "Group id ${gid} (${name}) is used in the image"
    fi
    echo "${name}:x:${gid}:" >> "${ROOT}/etc/group"
    echo "${name}:!::" >> "${ROOT}/etc/gshadow"
    echo "$gid"
}

group_id () {
    # group_id <name>
    awk -F: -v name="$1" '$1 == name {print $3}' "${ROOT}/etc/group"
}

valid () {
    # valid <what> <value> <regex>
    if [[ ! "$2" =~ $3 ]]; then
        fail "Invalid ${1}: ${2}"
    fi
}

NAME_RE='^[a-z_][-a-z0-9_]*$'
# Canonical decimal ids (no leading zeros), as they are compared and written.
ID_RE='^(0|[1-9][0-9]*)$'
PATH_RE='^/[A-Za-z0-9._/+-]*[A-Za-z0-9._+-]$'

# Groups
"$JQ" -c '.groups[]' "$SPEC" > "${WORK}/groups"
while read -r group; do
    valid "group name" "$("$JQ" -r '.name' <<< "$group")" "$NAME_RE"
    if [[ -n "$("$JQ" -r '.gid // empty' <<< "$group")" ]]; then
        valid "gid" "$("$JQ" -r '.gid' <<< "$group")" "$ID_RE"
    fi
    add_group "$("$JQ" -r '.name' <<< "$group")" "$("$JQ" -r '.gid // empty' <<< "$group")" > /dev/null
done < "${WORK}/groups"

# Users. A user with `"group": true` gets a group of the same name, with the same id if it is free
# (as `adduser --system --group`).
HOMES=()
"$JQ" -c '.users[]' "$SPEC" > "${WORK}/users"
while read -r user; do
    name="$("$JQ" -r '.name' <<< "$user")"
    uid="$("$JQ" -r '.uid // empty' <<< "$user")"
    home="$("$JQ" -r '.home // "/home/\(.name)"' <<< "$user")"
    shell="$("$JQ" -r '.shell // "/usr/sbin/nologin"' <<< "$user")"
    gecos="$("$JQ" -r '.gecos // ""' <<< "$user")"
    valid "user name" "$name" "$NAME_RE"
    valid "home of ${name} (an absolute path, other than /)" "$home" "$PATH_RE"
    valid "shell of ${name}" "$shell" "$PATH_RE"
    for path in "$home" "$shell"; do
        # Canonical paths only: no repeated separators, or `.` / `..` components.
        if [[ "$path" == *//* || "/${path}/" == */./* || "/${path}/" == */../* ]]; then
            fail "Invalid path of ${name} (not canonical): ${path}"
        fi
    done
    if [[ "$gecos" == *:* || "$gecos" == *$'\n'* ]]; then
        fail "Invalid gecos of ${name}: ${gecos}"
    fi
    for field in uid gid; do
        if [[ -n "$("$JQ" -r ".${field} // empty" <<< "$user")" ]]; then
            valid "${field} of ${name}" "$("$JQ" -r ".${field}" <<< "$user")" "$ID_RE"
        fi
    done
    if [[ -n "$("$JQ" -r '.home_mode // empty' <<< "$user")" ]]; then
        valid "home_mode of ${name}" "$("$JQ" -r '.home_mode' <<< "$user")" '^0?[0-7]{3}$'
    fi
    if has_entry "${ROOT}/etc/passwd" "$name"; then
        fail "User ${name} exists in the image"
    fi
    # A home through a symlinked directory would be created (or changed) through the link.
    path=""
    IFS=/ read -ra parts <<< "${home#/}"
    for part in "${parts[@]}"; do
        path="${path:+${path}/}${part}"
        if awk -v path="$path" '$0 == path {found = 1} END {exit !found}' "$LINKS"; then
            fail "Home of ${name} (${home}) is through a symlink in the image: /${path}"
        fi
    done
    if [[ -z "$uid" ]]; then
        uid="$(free_id "${ROOT}/etc/passwd")" || exit 1
    elif id_used "${ROOT}/etc/passwd" "$uid"; then
        fail "User id ${uid} (${name}) is used in the image"
    fi
    if [[ "$("$JQ" -r '.group // false' <<< "$user")" == "true" ]]; then
        if id_used "${ROOT}/etc/group" "$uid"; then
            gid="$(add_group "$name")" || exit 1
        else
            gid="$(add_group "$name" "$uid")" || exit 1
        fi
    else
        gid="$("$JQ" -r '.gid // empty' <<< "$user")"
        if [[ -z "$gid" ]]; then
            gid="$(group_id "$("$JQ" -r '.primary_group // "nogroup"' <<< "$user")")"
        fi
        [[ -n "$gid" ]] || fail "No group for user ${name}"
    fi
    echo "${name}:x:${uid}:${gid}:${gecos}:${home}:${shell}" >> "${ROOT}/etc/passwd"
    echo "${name}:*:${LASTCHG}:0:99999:7:::" >> "${ROOT}/etc/shadow"
    if [[ "$("$JQ" -r 'if has("create_home") then .create_home else true end' <<< "$user")" == "true" ]]; then
        HOMES+=("${home}:${uid}:${gid}:$("$JQ" -r '.home_mode // "0750"' <<< "$user")")
    fi
done < "${WORK}/users"

# dpkg status: add (or replace) the entry of each package, from its control file, as installed.
STATUS=0
for control in "${CONTROLS[@]+"${CONTROLS[@]}"}"; do
    STATUS=1
    mkdir -p "${WORK}/control"
    name="$("$TAR" -tf "$control" | awk '($0 == "control" || $0 == "./control") && !found {print; found = 1}')"
    [[ -n "$name" ]] || fail "No control file in ${control}"
    "$TAR" -xf "$control" -C "${WORK}/control" --include "$name"
    if [[ -L "${WORK}/control/control" || ! -f "${WORK}/control/control" ]]; then
        fail "The control file of ${control} is not a regular file"
    fi
    entry="$(awk '{print} /^Package:/ {print "Status: install ok installed"}' "${WORK}/control/control" | sed -e '/^$/d')"
    package="$(awk '/^Package:/ {print $2}' <<< "$entry")"
    arch="$(awk '/^Architecture:/ {print $2}' <<< "$entry")"
    awk -v RS= -v ORS='\n\n' -v package="$package" -v arch="$arch" '
        {
            p = ""; a = ""
            n = split($0, lines, "\n")
            for (i = 1; i <= n; i++) {
                if (lines[i] ~ /^Package: /) p = substr(lines[i], 10)
                if (lines[i] ~ /^Architecture: /) a = substr(lines[i], 15)
            }
            if (p == package && (a == arch || a == "all" || arch == "all")) next
            print
        }' "${ROOT}/var/lib/dpkg/status" > "${WORK}/status"
    printf '%s\n\n' "$entry" >> "${WORK}/status"
    mv "${WORK}/status" "${ROOT}/var/lib/dpkg/status"
    rm -rf "${WORK}/control"
done

# The layer: the edited files, with their mode and owner in the image (or the defaults), the new
# homes, and any of their parent directories that are not in the image. Directories that are in the
# image are left as they are.
SHADOW_GID="$(group_id shadow)"
SHADOW_GID="${SHADOW_GID:-0}"
MTREE="${WORK}/mtree"

in_image () {
    # in_image <path>: whether the (absolute) path is in the image
    awk -v path="${1#/}" '$0 == path {found = 1} END {exit !found}' "$PATHS"
}

new_dirs () {
    # new_dirs <path>: the parent directories of the path that are not in the image, as root dirs
    local parent
    parent="$(dirname "$1")"
    while [[ "$parent" != "/" ]]; do
        if ! in_image "$parent"; then
            echo ".${parent} type=dir uid=0 gid=0 mode=0755 time=0"
        fi
        parent="$(dirname "$parent")"
    done
}

layer_file () {
    # layer_file <file> <default mode> <default uid> <default gid>
    local mode uid gid
    read -r mode uid gid <<< "$(file_meta "$@")"
    new_dirs "/$1"
    echo "./$1 type=file uid=${uid} gid=${gid} mode=${mode} time=0 contents=${ROOT}/$1"
}

{
    echo "#mtree"
    if [[ "$("$JQ" '(.users | length) + (.groups | length)' "$SPEC")" != "0" ]]; then
        layer_file etc/passwd 0644 0 0
        layer_file etc/group 0644 0 0
        layer_file etc/shadow 0640 0 "$SHADOW_GID"
        layer_file etc/gshadow 0640 0 "$SHADOW_GID"
    fi
    if [[ "$STATUS" == 1 ]]; then
        layer_file var/lib/dpkg/status 0644 0 0
    fi
    # New homes (those in the image are left as they are, as `adduser` does), and their parents
    # that are not in the image or another home.
    HOME_DIRS="$(printf '%s\n' "${HOMES[@]+"${HOMES[@]}"}" | cut -d: -f1)"
    for entry in "${HOMES[@]+"${HOMES[@]}"}"; do
        IFS=: read -r home uid gid mode <<< "$entry"
        if ! in_image "$home"; then
            echo ".${home} type=dir uid=${uid} gid=${gid} mode=${mode} time=0"
        fi
        new_dirs "$home" | while read -r dir rest; do
            if ! awk -v path="${dir#.}" '$0 == path {found = 1} END {exit !found}' <<< "$HOME_DIRS"; then
                echo "${dir} ${rest}"
            fi
        done
    done
} | sort -u -k1,1 > "$MTREE"

"$TAR" --create --file "$OUT" --format=pax "@${MTREE}"
