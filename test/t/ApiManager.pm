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
# A shared module for ESP end-to-end tests.
# Sets up a TEST_NGINX_BINARY environment variable for Nginx test framework
# to find ESP build of Nginx.
# Adds Nginx test library (nginx-tests/lib) to the module search path.

use strict;
use warnings;

package ApiManager;

use FindBin;
use JSON::PP;
use Data::Dumper;
use MIME::Base64;
use Test::More;

sub repo_root {
  my $testdir = $FindBin::Bin;
  my @path = split('/', $testdir);
  return (join('/', @path[0 .. $#path - 3]), $testdir);
}

BEGIN {
  our ($Root, $TestDir) = repo_root();
  our $TestLib = $Root . "/nginx_tests_git/lib";

  if (!defined $ENV{TEST_SRCDIR}) {
    $ENV{TEST_SRCDIR} = $Root;
  }
}

use lib $ApiManager::TestLib;

select STDERR; $| = 1;   # flush stderr immediately
select STDOUT; $| = 1;   # flush stdout immediately

sub write_binary_file {
  my ($name, $content) = @_;
  open F, '>>', $name or die "Can't create $name: $!";
  binmode F;
  print F $content;
  close F;
}

sub compare {
  my ($x, $y, $path, $ignore_keys) = @_;

  my $refx = ref $x;
  my $refy = ref $y;
  if(!$refx && !$refy) { # both are scalars
    unless ($x eq $y) {
      print "$path value doesn't match $x != $y.\n";
      return 0;
    }
  }
  elsif ($refx ne $refy) { # not the same type
    print "$path type doesn't match $refx != $refy.\n";
    return 0;
  }
  elsif ($refx eq 'SCALAR' || $refx eq 'REF') {
    return compare(${$x}, ${$y}, $path, $ignore_keys);
  }
  elsif ($refx eq 'ARRAY') {
    if ($#{$x} == $#{$y}) { # same length
      my $i = -1;
      for (@$x) {
        $i++;
        return 0 unless compare(
          $x->[$i], $y->[$i], "$path:[$i]", $ignore_keys);
      }
    }
    else {
      print "$path array size doesn't match: $#{$x} != $#{$y}.\n";
      return 0;
    }
  }
  elsif ($refx eq 'HASH') {
    my @diff = grep { !exists $ignore_keys->{$_} && !exists $y->{$_} } keys %$x;
    if (@diff) {
      print "$path has following extra keys:\n";
      for (@diff) {
        print "$_: $x->{$_}\n";
      }
      return 0;
    }
    for (keys %$y) {
      unless(exists($x->{$_})) {
        print "$path key $_ doesn't exist.\n";
        return 0;
      }
      return 0 unless compare($x->{$_}, $y->{$_}, "$path:$_", $ignore_keys);
    }
  } else {
    print "$path: Not supported type: $refx\n";
    return 0;
  }
  return 1;
}

sub compare_json {
  my ($json, $expected, $random_metrics) = @_;
  my $json_obj = decode_json($json);

  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  return compare($json_obj, $expected, "", {});
}

sub compare_json_with_random_metrics {
  my ($json, $expected, @random_metrics) = @_;
  my $json_obj = decode_json($json);

  # A list of metrics with non-deterministic values.
  my %random_metric_map = map { $_ => 1 } @random_metrics;

  # Check and remove the above random metrics before making the comparison.
  my $matched_random_metric_count = 0;
  if (not exists $json_obj->{operations}) {
    return 0;
  }

  my $operation = $json_obj->{operations}->[0];
  if (not exists $operation->{metricValueSets}) {
    return 0;
  }

  my @metric_value_sets;
  foreach my $metric (@{$operation->{metricValueSets}}) {
    if (exists($random_metric_map{$metric->{metricName}})) {
      $matched_random_metric_count += 1;
    } else {
      push @metric_value_sets, $metric;
    }
  }

  if ($matched_random_metric_count != scalar @random_metrics) {
    return 0;
  }
  $operation->{metricValueSets} = \@metric_value_sets;

  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  my %ignore_keys = map { $_ => "1" } qw(
    startTime endTime timestamp operationId);
  return compare($json_obj, $expected, "", \%ignore_keys);
}

sub compare_user_info {
  my ($user_info, $expected) = @_;
  my $json_obj = decode_json(decode_base64($user_info));
  print Dumper $json_obj if $ENV{TEST_NGINX_VERBOSE};
  return compare($json_obj, $expected, "", {});
}

sub read_file_using_full_path {
  my ($full_path) = @_;
  local $/;
  open F, '<', $full_path or die "Can't open $full_path $!";
  my $content = <F>;
  close F;
  return $content;
}

sub read_test_file {
  my ($name) = @_;
  return read_file_using_full_path($ApiManager::TestDir . '/' . $name);
}

sub write_file_expand {
  if (!defined $ENV{TEST_CONFIG}) {
    $ENV{TEST_CONFIG} = "";
  }
  my ($t, $name, $content) = @_;
  $content =~ s/%%TEST_CONFIG%%/$ENV{TEST_CONFIG}/gmse;
  $t->write_file_expand($name,  $content);
}

sub get_bookstore_service_config {
  return read_test_file("testdata/bookstore.pb.txt");
}

sub get_bookstore_service_config_allow_all_http_requests {
    return read_test_file('testdata/bookstore_allow_all_http_requests.pb.txt');
}

sub get_bookstore_service_config_allow_unregistered {
  return get_bookstore_service_config .
         read_test_file("testdata/usage_fragment.pb.txt");
}

sub get_bookstore_service_config_allow_some_unregistered {
  return get_bookstore_service_config .
      read_test_file("testdata/usage_frag.pb.txt");
}

sub get_echo_service_config {
  return read_test_file("testdata/echo_service.pb.txt");
}

sub get_grpc_test_service_config {
  my ($GrpcBackendPort) = @_;
  return read_test_file("testdata/grpc_echo_service.pb.txt") . <<EOF
backend {
  rules {
    selector: "test.grpc.Test.Echo"
    address: "127.0.0.1:$GrpcBackendPort"
  }
  rules {
    selector: "test.grpc.Test.EchoStream"
    address: "127.0.0.1:$GrpcBackendPort"
  }
}
EOF
}

sub get_grpc_interop_service_config {
  return read_test_file("testdata/grpc_interop_service.pb.txt");
}

sub get_transcoding_test_service_config {
  my ($host_name, $service_control_address) = @_;
  my $path = './test/transcoding/service.pb.txt';
  my $service_config = read_file_using_full_path($path);
  # Replace the host name
  $service_config =~ s/<YOUR_PROJECT_ID>.appspot.com/$host_name/;
  # Replace the project id
  $service_config =~ s/<YOUR_PROJECT_ID>/endpoints-transcoding-test/;
  # Replace the service control address
  $service_config =~ s/servicecontrol.googleapis.com/$service_control_address/;
  return $service_config;
}

sub get_grpc_echo_test_service_config {
  my ($host_name, $service_control_address) = @_;
  my $path = './test/grpc/local/service.json';
  my $service_config = read_file_using_full_path($path);
  # Replace the host name
  $service_config =~ s/echo-dot-esp-grpc-load-test.appspot.com/$host_name/;
  # Replace the service control address
  $service_config =~ s/servicecontrol.googleapis.com/$service_control_address/;
  return $service_config;
}

sub get_large_report_request {
  my ($t, $size) = @_;
  my $testdir = $t->testdir();
  my $cmd = './src/tools/service_control_json_gen';
  system "$cmd --report_request_size=$size --json > $testdir/large_data.json";
  return $t->read_file('large_data.json');
}

sub get_metadata_response_body {
  return <<EOF;
{
  "instance": {
    "attributes": {
      "gae_app_container": "app",
      "gae_app_fullname": "esp-test-app_20150921t180445-387321214075436208",
      "gae_backend_instance": "0",
      "gae_backend_minor_version": "387321214075436208",
      "gae_backend_name": "default",
      "gae_backend_version": "20150921t180445",
      "gae_project": "esp-test-app",
      "gae_vm_runtime": "custom",
      "gcm-pool": "gae-default-20150921t180445",
      "gcm-replica": "gae-default-20150921t180445-inqp"
    },
    "cpuPlatform": "Intel Ivy Bridge",
    "description": "GAE managed VM for module: default, version: 20150921t180445",
    "hostname": "gae-default-20150921t180445-inqp.c.esp-test-app.internal",
    "id": 3296474103533342935,
    "image": "",
    "machineType": "projects/345623948572/machineTypes/g1-small",
    "maintenanceEvent": "NONE",
    "zone": "projects/345623948572/zones/us-west1-d"
  },
  "project": {
    "numericProjectId": 345623948572,
    "projectId": "esp-test-app"
  }
}
EOF
}

sub disable_service_control_cache {
  return <<EOF;
service_control_config {
  check_aggregator_config {
  cache_entries: 0
  }
  report_aggregator_config {
  cache_entries: 0
  }
}
EOF
}

sub envoy {
  my ($t, $ServiceConfig, $Port, $BackendPort, $ServiceControlPort) = @_;
  my $testdir = $t->testdir();

  $t->write_file_expand('envoy.json', <<"  EOF");
  {
    "listeners": [
      {
        "port": $Port,
        "filters": [
          {
            "type": "read",
            "name": "http_connection_manager",
            "config": {
              "codec_type": "auto",
              "stat_prefix": "ingress_http",
              "route_config": {
                "virtual_hosts": [
                  {
                    "name": "backend",
                    "domains": ["*"],
                    "routes": [
                      {
                        "timeout_ms": 0,
                        "prefix": "/",
                        "cluster": "service1"
                      }
                    ]
                  }
                ]
              },
              "access_log": [
                {
                  "path": "/tmp/access.envoy"
                }
              ],
              "filters": [
                {
                  "type": "both",
                  "name": "esp",
                  "config": {
                    "service_config": "$ServiceConfig"
                  }
                },
                {
                  "type": "decoder",
                  "name": "router",
                  "config": {}
                }
              ]
            }
          }
        ]
      }
    ],
    "admin": {
      "access_log_path": "$testdir/access.envoy",
      "port": 1080
    },
    "cluster_manager": {
      "clusters": [
        {
          "name": "service1",
          "connect_timeout_ms": 5000,
          "type": "strict_dns",
          "lb_type": "round_robin",
          "hosts": [
            {
              "url": "tcp://localhost:$BackendPort"
            }
          ]
        },
        {
          "name": "api_manager",
          "connect_timeout_ms": 5000,
          "type": "strict_dns",
          "lb_type": "round_robin",
          "hosts": [
            {
              "url": "tcp://localhost:$ServiceControlPort"
            }
          ]
        }
      ]
    },
    "tracing_enabled": "true"
  }
  EOF

  chdir $testdir;
  my $server = $ENV{TEST_NGINX_BINARY};
  exec $server, "-c", "envoy.json", "-l", "debug";
}

sub grpc_test_server {
  my ($t, @args) = @_;
  my $server = './test/grpc/grpc-test-server';
  exec $server, @args;
}

sub grpc_interop_server {
  my ($t, $port) = @_;
  my $server = './test/grpc/interop-server';
  exec $server, "--port", $port;
}

sub transcoding_test_server {
  my ($t, @args) = @_;
  my $server = './test/transcoding/bookstore-server';
  exec $server, @args;
}

# Runs the gRPC server for testing transcoding and redirects the output to a
# file.
sub run_transcoding_test_server {
  my ($t, $output_file, @args) = @_;
  my $redirect_file = $t->{_testdir}.'/'.$output_file;

  # redirect, fork & run, restore
  open ORIGINAL, ">&", \*STDOUT;
  open STDOUT, ">", $redirect_file;
  $t->run_daemon(\&transcoding_test_server, $t, @args);
  open STDOUT, ">&", \*ORIGINAL;
}

sub call_bookstore_client {
  my ($t, @args) = @_;
  my $client = './test/transcoding/bookstore-client';
  my $output_file = $t->{_testdir} . '/bookstore-client.log';

  my $rc = system "$client " . join(' ', @args) . " > $output_file";

  return ($rc, read_file_using_full_path($output_file))
}

sub run_grpc_test {
  my ($t, $plans) = @_;
  $t->write_file('test_plans.txt', $plans);
  my $testdir = $t->testdir();
  my $client = './test/grpc/grpc-test-client';
  system "$client < $testdir/test_plans.txt > $testdir/test_results.txt";
  return $t->read_file('test_results.txt');
}

sub run_grpc_interop_test {
  my ($t, $port, $test_case, @args) = @_;
  my $testdir = $t->testdir();
  my $client = './test/grpc/interop-client';
  return system "$client --server_port $port --test_case $test_case " . join(' ', @args)
}

sub run_nginx_with_stderr_redirect {
  my $t = shift;
  my $redirect_file = $t->{_testdir}.'/stderr.log';

  # redirect, fork & run, restore
  open ORIGINAL, ">&", \*STDERR;
  open STDERR, ">", $redirect_file;
  $t->run();
  open STDERR, ">&", \*ORIGINAL;
}

# Runs an HTTP server that returns "404 Not Found" for every request.
sub not_found_server {
  my ($t, $port) = @_;

  my $server = HttpServer->new($port, $t->testdir() . '/nop.log')
    or die "Can't create test server socket: $!\n";

  $server->run();
}

# Reads a file which contains a stream of HTTP requests,
# parses out individual requests and returns them in an array.
sub read_http_stream {
  my ($t, $file) = @_;

  my $http = $t->read_file($file);

  # Parse out individual HTTP requests.

  my @requests;

  while ($http ne '') {
    my ($request_headers, $rest) = split /\r\n\r\n/, $http, 2;
    my @header_lines = split /\r\n/, $request_headers;

    my %headers;
    my $verb = '';
    my $uri = '';
    my $path = '';
    my $body = '';

    # Process request line.
    my $request_line = $header_lines[0];
    if ($request_line =~ /^(\S+)\s+(([^? ]+)(\?[^ ]+)?)\s+HTTP/i) {
      $verb = $1;
      $uri = $2;
      $path = $3;
    }

    # Process headers
    foreach my $header (@header_lines[1 .. $#header_lines]) {
      my ($key, $value) = split /\s*:\s*/, $header, 2;
      $headers{lc $key} = $value;
    }

    my $content_length = $headers{'content-length'} || 0;
    if ($content_length > 0) {
      $body = substr $rest, 0, $content_length;
      $rest = substr $rest, $content_length;
    }

    push @requests, {
      'verb' => $verb,
      'path' => $path,
      'uri' => $uri,
      'headers' => \%headers,
      'body' => $body
    };

    $http = $rest;
  }

  return @requests;
}

# Checks that response Content-Type header is application/json and matches the
# response body with the expected JSON.
sub verify_http_json_response {
  my ($response, $expected_body) = @_;

  # Parse out the body
  my ($headers, $actual_body) = split /\r\n\r\n/, $response, 2;

  if ($headers !~ qr/HTTP\/1.1 200 OK/i) {
    Test::More::diag("Status code doesn't match\n");
    Test::More::diag("Expected: 200 OK\n");
    Test::More::diag("Actual headers: ${headers}\n");
    return 0;
  }

  if ($headers !~ qr/content-type:(\s)*application\/json/i) {
    Test::More::diag("Content-Type doesn't match\n");
    Test::More::diag("Expected: application/json\n");
    Test::More::diag("Actual headers: ${headers}\n");
    return 0;
  }

  if (!compare_json($actual_body, $expected_body)) {
    Test::More::diag("Response body doesn't match\n");
    Test::More::diag("Expected: " . encode_json(${expected_body}) . "\n");
    Test::More::diag("Actual: ${actual_body}\n");
    return 0;
  }

  return 1;
}

# Initial port is 8080 or $TEST_PORT env variable. A test is allowed to use 10
# subsequent ports.
sub available_port_range {
  my %port_range;
  if (!defined $ENV{TEST_PORT}) {
    %port_range = (8080, 8090);
  } else {
    %port_range = ($ENV{TEST_PORT}, $ENV{TEST_PORT} + 10);
  }

  printf("Available port range: [%d, %d)\n", %port_range);
  return %port_range;
}

my ($next_port, $max_port) = available_port_range();

# Select an open port
sub pick_port {
  for (my $port = $next_port; $port < $max_port; $port++) {
    my $server = IO::Socket::INET->new(
        Proto => 'tcp',
        LocalHost => '127.0.0.1',
        LocalPort => $port,
    )
    or next;
    close $server;
    $next_port = $port + 1;
    print "Pick port: $port\n";
    return $port;
  }
  die "Could not find an available port for testing\n"
}

#
# These routines are copied from Nginx.pm to support custom ports
#

sub log_core {
  my ($prefix, $msg) = @_;
  ($prefix, $msg) = ('', $prefix) unless defined $msg;
  $prefix .= ' ' if length($prefix) > 0;

  if (length($msg) > 2048) {
    $msg = substr($msg, 0, 2048)
      . "(...logged only 2048 of " . length($msg)
      . " bytes)";
  }

  $msg =~ s/^/# $prefix/gm;
  $msg =~ s/([^\x20-\x7e])/sprintf('\\x%02x', ord($1)) . (($1 eq "\n") ? "\n" : '')/gmxe;
  $msg .= "\n" unless $msg =~ /\n\Z/;
  print $msg;
}

sub log_out {
  log_core('>>', @_);
}

sub log_in {
  log_core('<<', @_);
}

sub http_get($;$;%) {
  my ($port, $url, %extra) = @_;
  return http($port, <<EOF, %extra);
GET $url HTTP/1.1
Host: localhost
Connection: close

EOF
}

sub http($;$;%) {
  my ($port, $request, %extra) = @_;

  my $s = http_start($port, $request, %extra);

  return $s if $extra{start} or !defined $s;
  return http_end($s);
}

sub http_start($;$;%) {
  my ($port, $request, %extra) = @_;
  my $s;

  eval {
    local $SIG{ALRM} = sub { die "timeout\n" };
    local $SIG{PIPE} = sub { die "sigpipe\n" };
    alarm(8);

    $s = $extra{socket} || IO::Socket::INET->new(
      Proto => 'tcp',
      PeerAddr => "127.0.0.1:$port"
    )
      or die "Can't connect to nginx: $!\n";

    log_out($request);
    $s->print($request);

    select undef, undef, undef, $extra{sleep} if $extra{sleep};
    return '' if $extra{aborted};

    if ($extra{body}) {
      log_out($extra{body});
      $s->print($extra{body});
    }

    alarm(0);
  };
  alarm(0);
  if ($@) {
    log_in("died: $@");
    return undef;
  }

  return $s;
}

sub http_end($;%) {
  my ($s) = @_;
  my $reply;

  eval {
    local $SIG{ALRM} = sub { die "timeout\n" };
    local $SIG{PIPE} = sub { die "sigpipe\n" };
    alarm(8);

    local $/;
    $reply = $s->getline();

    alarm(0);
  };
  alarm(0);
  if ($@) {
    log_in("died: $@");
    return undef;
  }

  log_in($reply);
  return $reply;
}

1;
