/*
 * Copyright 2018-2019 The OpenZipkin Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
describe('Zipkin Http Api', () => {
  const Sway = require('sway');
  const read = require('fs').readFileSync;
  const load = require('js-yaml').load;

  function validateSwagger(yaml, validationCallback) {
    const zipkinAPI = read(yaml).toString();
    Sway.create({definition: load(zipkinAPI)}).then(api => {
      validationCallback(api.validate());
    });
  }

  it('/api/v1 yaml should have no swagger syntax errors', done => {
    validateSwagger('./zipkin-api.yaml', result => {
      expect(result.errors).toHaveLength(0);
      done();
    });
  });

  it('/api/v2 yaml should have no swagger syntax errors', done => {
    validateSwagger('./zipkin2-api.yaml', result => {
      expect(result.errors).toHaveLength(0);
      done();
    });
  });
});

describe('Zipkin Protocol Buffers Api', () => {
  const load = require('protobufjs').load;

  function validateProto(proto, validationCallback) {
    load(proto, (err, root) => {
      if (err) throw err;
      validationCallback(root);
    });
  }

  it('should include core data structures', done => {
    validateProto('zipkin.proto', root => {
      expect(root.lookupType("zipkin.proto3.Endpoint")).toBeDefined();
      expect(root.lookupType("zipkin.proto3.Annotation")).toBeDefined();
      expect(root.lookupType("zipkin.proto3.Span")).toBeDefined();
      expect(root.lookupType("zipkin.proto3.ListOfSpans")).toBeDefined();
      done();
    });
  });

  it('should include reporting service', done => {
    validateProto('zipkin.proto', root => {
      // lookup is different for services vs messages
      expect(root.lookup("SpanService")).toBeDefined();
      expect(root.lookupType("zipkin.proto3.ReportResponse")).toBeDefined();
      done();
    });
  });
});
