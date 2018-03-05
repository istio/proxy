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
import chilkat   #  Chilkat v9.5.0.66 or later.

""" This script is used to generate ES256/RS256 public jwk key."""

"""commands to generate public_key_file (Note that private key needs to be generated first):
  ES256: $ openssl ecparam -genkey -name prime256v1 -noout -out private_key.pem
         $ openssl ec -in private_key.pem -pubout -out public_key.pem
  RS256: $ openssl genpkey -algorithm RSA -out private_key.pem -pkeyopt rsa_keygen_bits:2048
         $ openssl rsa -pubout -in private_key.pem -out public_key.pem
"""

def main(args):
  #  Load public key file into memory.
  sbPem = chilkat.CkStringBuilder()
  success = sbPem.LoadFile(args.public_key_file, "utf-8")
  if (success != True):
    print("Failed to load public key.")
    sys.exit()

  #  Load the key file into a public key object.
  pubKey = chilkat.CkPublicKey()
  success = pubKey.LoadFromString(sbPem.getAsString())
  if (success != True):
    print(pubKey.lastErrorText())
    sys.exit()

  #  Get the public key in JWK format:
  jwk = pubKey.getJwk()
  # Convert it to json format.
  json = chilkat.CkJsonObject()
  json.Load(jwk)
  # This line is used to set output format.
  cpt = True
  if (not args.compact) or (args.compact and args.compact == "no"):
    cpt = False
  json.put_EmitCompact(cpt)
  # Additional information can be added like this. change to fit needs.
  if args.alg:
    json.AppendString("alg", args.alg)
  if args.kid:
    json.AppendString("kid", args.kid)
  # Print.
  print("Generated " + args.alg + " public jwk:")
  print(json.emit())


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)

  # positional arguments
  parser.add_argument(
      "alg",
      help="Signing algorithm, e.g., ES256/RS256.")
  parser.add_argument(
      "public_key_file",
      help="The path to the generated ES256/RS256 public key file, e.g., /path/to/public_key.pem.")

  #optional arguments
  parser.add_argument("-c", "--compact", help="If making json output compact, say 'yes' or 'no'.")
  parser.add_argument("-k", "--kid", help="Key id, same as the kid in private key if any.")
  main(parser.parse_args())