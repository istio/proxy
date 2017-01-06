// Copyright (C) Extensible Service Proxy Authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////
//
// A Google Cloud Endpoints example implementation of a simple bookstore API.

"use strict";

var http = require('http');
var server = http.createServer();

var totalReceived = 0;
var totalData = 0;

server.on('request', function(req, res) {
  totalReceived += 1;
  var method = req.method;
  var url = req.url;
  if (method == 'GET' && url == '/version') {
      res.writeHead(200, {"Content-Type": "application/json"});
      res.write('{"version":"${VERSION}"}');
      res.end();
      return;
  }
  req.on('data', function(chunk) {
    totalData += chunk.length;
    res.write(chunk);
  })
  req.on('end', function() {
    res.end();
  });

  var cl = req.headers['content-length'];
  var ct = req.headers['content-type'];

  var headers = {};
  if (cl !== undefined) {
    headers['Content-Length'] = cl;
  }
  if (ct !== undefined) {
    headers['Content-Type'] = ct;
  }

  res.writeHead(200, headers);
  req.resume();
});

var totalConnection = 0;

server.on('connection', function(socket) {
  totalConnection += 1;
});

setInterval(function() {
  console.log("Requests received:", totalReceived, " Data: ", totalData, " Connection: ", totalConnection);
}, 1000);

var port = process.env.PORT || 8080;

server.listen(port, function() {
  console.log('Listening on port', port);
});
