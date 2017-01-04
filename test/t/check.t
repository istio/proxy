# Copyright 2016 Google Inc. All Rights Reserved.
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
#
################################################################################
#
use strict;
use warnings;

################################################################################

use test::t::ApiManager;   # Must be first (sets up import path to the Nginx test module)
use test::t::HttpServer;
use Test::Nginx;  # Imports Nginx's test module
use Test::More;   # And the test framework

################################################################################

# Port assignments
my $NginxPort = ApiManager::pick_port();
my $BackendPort = ApiManager::pick_port();
my $ServiceControlPort = ApiManager::pick_port();

my $t = Test::Nginx->new()->has(qw/http proxy/)->plan(10);

# Save service name in the service configuration protocol buffer file.

$t->write_file('service.pb.txt', ApiManager::get_bookstore_service_config . <<"EOF");
control {
  environment: "http://127.0.0.1:${ServiceControlPort}"
}
EOF

ApiManager::write_file_expand($t, 'nginx.conf', <<"EOF");
%%TEST_GLOBALS%%
daemon off;
events {
  worker_connections 32;
}
http {
  %%TEST_GLOBALS_HTTP%%
  server_tokens off;
  server {
    listen 127.0.0.1:${NginxPort};
    server_name localhost;
    location / {
      endpoints {
        api service.pb.txt;
        %%TEST_CONFIG%%
        on;
      }
      proxy_pass http://127.0.0.1:${BackendPort};
    }
  }
}
EOF

$t->run_daemon(\&bookstore, $t, $BackendPort, 'bookstore.log');
$t->run_daemon(\&servicecontrol, $t, $ServiceControlPort, 'servicecontrol.log');
is($t->waitforsocket("127.0.0.1:${BackendPort}"), 1, 'Bookstore socket ready.');
is($t->waitforsocket("127.0.0.1:${ServiceControlPort}"), 1, 'Service control socket ready.');
$t->run_daemon(\&ApiManager::envoy, $t, 'service.pb.txt', $NginxPort, $BackendPort, $ServiceControlPort);
is($t->waitforsocket("127.0.0.1:${NginxPort}"), 1, 'Envoy socket ready.');

<>;
################################################################################

my $response = ApiManager::http_get($NginxPort,'/shelves?key=this-is-an-api-key');

$t->stop_daemons();

my ($response_headers, $response_body) = split /\r\n\r\n/, $response, 2;

like($response_headers, qr/HTTP\/1\.1 200 OK/, 'Returned HTTP 200.');
is($response_body, <<'EOF', 'Shelves returned in the response body.');
{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF

my @requests = ApiManager::read_http_stream($t, 'bookstore.log');
is(scalar @requests, 1, 'Backend received one request');

my $r = shift @requests;
is($r->{verb}, 'GET', 'Backend request was a get');
is($r->{uri}, '/shelves?key=this-is-an-api-key', 'Backend uri was /shelves');
#is($r->{headers}->{host}, "127.0.0.1:${BackendPort}", 'Host header was set');

@requests = ApiManager::read_http_stream($t, 'servicecontrol.log');
is(scalar @requests, 1, 'Service control received one request');

$r = shift @requests;
is($r->{verb}, 'POST', ':check verb was post');
is($r->{uri}, '/v1/services/endpoints-test.cloudendpointsapis.com:check', ':check was called');
#is($r->{headers}->{host}, "127.0.0.1:${ServiceControlPort}", 'Host header was set');
is($r->{headers}->{'content-type'}, 'application/x-protobuf', ':check Content-Type was protocol buffer');

################################################################################

sub bookstore {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';

  $server->on('GET', '/shelves?key=this-is-an-api-key', <<'EOF');
HTTP/1.1 200 OK
Content-Length: 118
Connection: close

{ "shelves": [
    { "name": "shelves/1", "theme": "Fiction" },
    { "name": "shelves/2", "theme": "Fantasy" }
  ]
}
EOF
  $server->run();
}

sub servicecontrol {
  my ($t, $port, $file) = @_;
  my $server = HttpServer->new($port, $t->testdir() . '/' . $file)
    or die "Can't create test server socket: $!\n";
  local $SIG{PIPE} = 'IGNORE';
  $server->on('POST', '/v1/services/endpoints-test.cloudendpointsapis.com:check', <<'EOF');
HTTP/1.1 200 OK
Connection: close

EOF
  $server->run();
}

################################################################################
