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
#
################################################################################
#
# Skylark macros for nginx tests.

load("@io_bazel_rules_perl//perl:perl.bzl", "perl_test")

def nginx_test(name, nginx, data=None, env=None, config=None, **kwargs):
  if nginx == None or len(nginx) == 0:
    fail("'nginx' parameter must be a non-empty string (target).")

  if data == None:
    data = []
  data += [ nginx ]

  if env == None:
    env = {}

  if config != None:
    data += [ config ]
    c = Label(config)
    env["TEST_CONFIG"] = "server_config " + "${TEST_SRCDIR}/" + c.package + "/" + c.name + ";"
    name = name + '_' + c.name.split("/")[-1].split(".")[0]

  # Count existing rules in the BUILD file and assign base port using
  # Each rule can use 10 ports in its range
  # Rules generated from config_list get separate ranges
  port = 9000 + len(native.existing_rules().values()) * 10

  l = Label(nginx)
  env["TEST_PORT"] = "%s" % port

  env_files = {
      "TEST_NGINX_BINARY": "../__main__/" + l.package + "/" + l.name
  }
  perl_test(name=name, data=data, env=env, env_files=env_files, **kwargs)

def nginx_suite(tests, deps, nginx, size="small", config_list=[], data=None, tags=[],
                timeout="short", env=None):
  for test in tests:
    if not config_list:
      nginx_test(
          name = test.split(".")[0],
          size = size,
          timeout = timeout,
          srcs = [test],
          deps = deps,
          data = data,
          nginx = nginx,
          config = None,
          tags = tags,
          env = env,
      )
    else:
      for config in config_list:
        nginx_test(
            name = test.split(".")[0],
            size = size,
            timeout = timeout,
            srcs = [test],
            deps = deps,
            data = data,
            nginx = nginx,
            config = config,
            tags = tags,
            env = env,
        )
