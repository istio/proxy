# -*- rpm-spec -*-

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

${SUBRPMS}

${CHANGELOG}
