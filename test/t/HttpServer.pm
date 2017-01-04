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
# A simple Http Server.

package HttpServer;

use strict;
use warnings;

use Data::Dumper;
use File::Basename;
use IO::Select;
use IO::Socket::SSL;
use Socket qw/ CRLF /;

sub new {
    my (undef, $port, $file, $ssl) = @_;

    my $self = {
        _port => $port,
        _file => $file,
        _http => [],
        _http_cb => {},
        _ssl => $ssl,
    };

    bless $self;
    return $self;
}

sub port {
    my ($self) = @_;
    return $self->{_port};
}

sub on {
    my ($self, $method, $url, $response) = @_;
    push @{$self->{_http}}, {
      method   => $method,
      url      => $url,
      response => $response,
    };
}

sub on_sub {
    my ($self, $method, $url, $handler) = @_;
    my $method_url = join($method, $url);
    $self->{_http_cb}{$method_url} = $handler;
}

sub handle_client {
  my ($self, $client, $rh) = @_;

  my $request = '';

  # Read headers.
  while (<$client>) {
    $request .= $_;
    last if (/^\x0d?\x0a?$/);
  }

  # Read the request.
  if ($request =~ /^(\S+)\s+(([^? ]+)(\?[^ ]+)?)\s+HTTP/i) {
    my $verb = $1;
    my $uri = $2;
    my $path = $3;
    my $body = '';
    my %headers;

    print $rh $request;

    my @request_parts = split /\s*\r\n/, $request;
    foreach my $header (@request_parts[1 .. $#request_parts]) {
      my ($key, $value) = split /\s*:\s*/, $header, 2;
      $headers{lc $key} = $value;
    }

    my $content_length = $headers{'content-length'} || 0;

    while ($content_length > 0) {
      my $chunk = "";
      $client->read($chunk, $content_length);
      $content_length -= length $chunk;
      $body .= $chunk;
      print $rh $chunk;
    }

    my $method_uri = join($verb, $uri);
    my $method_path = join($verb, $path);
    if (exists($self->{_http_cb}{$method_uri})) {
      my $handler = $self->{_http_cb}{$method_uri};
      &$handler($request, $body, $client);
    } elsif (exists($self->{_http_cb}{$method_path})) {
      my $handler = $self->{_http_cb}{$method_path};
      &$handler($request, $body, $client);
    } elsif ($uri ne '') {
      if (@{$self->{_http}} &&
          $verb eq $self->{_http}->[0]->{method} &&
          ($uri eq $self->{_http}->[0]->{url} ||
           $path eq $self->{_http}->[0]->{url})) {
        my $http = shift @{$self->{_http}};
        print $client $http->{response};
      } else {
        #print $rh "URL ", $uri, " not found.\n";
        print $client <<EOF;
HTTP/1.1 404 Not Found
Connection: close

ERROR: '$uri' not found.
EOF
      }
    }
  }
}

sub run {
  my ($self) = @_;

  my $listeners = IO::Select->new();
  my $server = IO::Socket::INET->new(
      Proto => 'tcp',
      LocalHost => '127.0.0.1',
      LocalPort => $self->{_port},
      Listen => 5,
      Reuse => 1
  )
  or die "Can't create test server socket: $!\n";
  $server->blocking(0);
  $listeners->add($server);

  # Please avoid taking ports that are not allocated
  if ($self->{_ssl}) {
    my $ssl_server = IO::Socket::INET->new(
        Proto => 'tcp',
        LocalHost => '127.0.0.1',
        LocalPort => $self->{_port} + 443,
        Listen => 5,
        Reuse => 1
    )
    or die "Can't create test SSL server socket: $!\n";
    $ssl_server->blocking(0);
    $listeners->add($ssl_server);
  }

  local $SIG{PIPE} = 'IGNORE';

  open my $rh, '>', $self->{_file} or die "cannot open > " . $self->{_file};
  select $rh; $| = 1; # Enable auto-flush.

  while (1) {
      my @ready = $listeners->can_read;
      foreach(@ready) {
          next unless my $client = $_->accept();
          $client->blocking(1);
          $client->autoflush(1);
          if ($client->sockport() != $self->{_port}) {
              # SSL upgrade client
              my $dir =  dirname($self->{_file});
              IO::Socket::SSL->start_SSL($client,
                  SSL_server => 1,
                  SSL_cert_file => "$dir/test.crt",
                  SSL_key_file => "$dir/test.key",
              ) or die "failed to ssl handshake: $SSL_ERROR";
          }
          $self->handle_client($client, $rh);
          close $client;
      }
  }
  close $rh;
}

1;
