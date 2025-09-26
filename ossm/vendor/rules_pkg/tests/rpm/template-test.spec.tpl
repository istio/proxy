# -*- rpm-spec -*-

################################################################################
# Test customizations

# Force MD5 file digesting to preserve compatibility with (very old) versions of
# rpm.
#
# For valid values to set here, see:
# https://github.com/rpm-software-management/rpm/blob/8d628a138ee4c3d1b77b993a3c5b71345ce052e8/macros.in#L393-L405
#
# At some point, we might want to consider bumping this up to use SHA-1 (2), but
# that would be best reserved for when we don't know of anyone using rpm < 4.6.
%define _source_filedigest_algorithm 1
%define _binary_filedigest_algorithm 1

# Do not try to use magic to determine file types
%define __spec_install_post %{nil}

################################################################################

# Hey!
#
# Keep the below in sync with pkg/rpm/template.spec.tpl!

################################################################################

# This comprises the entirety of the preamble
%include %build_rpm_options

%description
%include %build_rpm_description

%install
%include %build_rpm_install

%files -f %build_rpm_files

${PRE_SCRIPTLET}

${POST_SCRIPTLET}

${PREUN_SCRIPTLET}

${POSTUN_SCRIPTLET}

${POSTTRANS_SCRIPTLET}
