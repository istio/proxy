import datetime
import getpass
import json


class SBOMWriter:
    def __init__(self, tool, out):
        self.out = out
        self.tool = tool

    def write_header(self, package):
        header = [
            'SPDXVersion: SPDX-2.2',
            'DataLicense: CC0-1.0',
            'SPDXID: SPDXRef-DOCUMENT',
            'DocumentName: %s' % package,
            # TBD
            # 'DocumentNamespace: https://swinslow.net/spdx-examples/example1/hello-v3
            'Creator: Person: %s' % getpass.getuser(),
            'Creator: Tool: %s' % self.tool,
            datetime.datetime.utcnow().strftime('Created: %Y-%m-%d-%H:%M:%SZ'),
            '',
            '##### Package: %s' % package,
        ]
        self.out.write('\n'.join(header))
    
    def write_packages(self, packages):
        for p in packages:
            name = p.get('package_name') or '<unknown>'
            self.out.write('\n')
            self.out.write('SPDXID: "%s"\n' % name)
            self.out.write('  name: "%s"\n' % name)

            if p.get('package_version'):
                self.out.write('  versionInfo: "%s"\n' % p['package_version'])
            
            # IGNORE_COPYRIGHT: Not a copyright notice. It is a variable holding one.
            cn = p.get('copyright_notice')
            if cn:
                self.out.write('  copyrightText: "%s"\n' % cn)
            
            kinds = p.get('license_kinds')
            if kinds:
                self.out.write('  licenseDeclared: "%s"\n' %
                    ','.join([k['name'] for k in kinds]))
            
            url = p.get('package_url')
            if url:
                self.out.write('  downloadLocation: %s\n' % url)

            purl = p.get('purl')
            if purl:
                self.out.write('  externalRef: PACKAGE-MANAGER purl %s\n' % purl)
