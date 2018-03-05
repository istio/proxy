#!/usr/bin/env python
#
# Copyright (C) Extensible Service Proxy Authors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import argparse
import sys
import jwt  # pip install PyJWT and pip install cryptography.

""" This script is used to generate ES256/RS256-signed jwt token."""

"""commands to generate private_key_file:
  ES256: $ openssl ecparam -genkey -name prime256v1 -noout -out private_key.pem
  RS256: $ openssl genpkey -algorithm RSA -out private_key.pem -pkeyopt rsa_keygen_bits:2048
"""

def main(args):
  # JWT token generation.
  with open(args.private_key_file, 'r') as f:
    try:
      secret = f.read()
    except:
      print("Failed to load private key.")
      sys.exit()

  # Token headers
  hdrs = {'alg': args.alg,
          'typ': 'JWT'}
  if args.kid:
    hdrs['kid'] = args.kid

  # Token claims
  claims = {'iss': args.iss,
            'sub': args.iss,
            'aud': args.aud}
  if args.email:
    claims['email'] = args.email
  if args.azp:
    claims['azp'] = args.azp
  if args.exp:
    claims['exp'] = args.exp

  # Change claim and headers field to fit needs.
  jwt_token = jwt.encode(claims,
                         secret,
                         algorithm=args.alg,
                         headers=hdrs)

  print(args.alg + "-signed jwt:")
  print(jwt_token)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)

  # positional arguments
  parser.add_argument(
      "alg",
      help="Signing algorithm, i.e., ES256/RS256.")
  parser.add_argument(
      "iss",
      help="Token issuer, which is also used for sub claim.")
  parser.add_argument(
      "aud",
      help="Audience. This must match 'audience' in the security configuration"
      " in the swagger spec.")
  parser.add_argument(
      "private_key_file",
      help="The path to the generated ES256/RS256 private key file, e.g., /path/to/private_key.pem.")

  #optional arguments
  parser.add_argument("-e", "--email", help="Preferred e-mail address.")
  parser.add_argument("-a", "--azp", help="Authorized party - the party to which the ID Token was issued.")
  parser.add_argument("-x", "--exp", help="Token expiration claim.")
  parser.add_argument("-k", "--kid", help="Key id.")
  main(parser.parse_args())
