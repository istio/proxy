#include "test/integration/controlled_upstream.h"

namespace Mixer {
    namespace ControlledUpstream {
        void DumpHeaders(const std::string& message, const Envoy::Http::HeaderMap& headers) {
          // Slurp into memory to avoid interleaved log messages.
          std::ostringstream buffer;
          buffer << message.c_str() << std::endl << headers;
          fputs(buffer.str().c_str(), stderr);
        }

        static TransactionHandler DefaultTransactionHandler = [](Envoy::FakeStream &stream,
                                                                 const std::string &upstream_name) {
            std::cerr << "HANDLE DEFAULT REQUEST FOR " << upstream_name << ":\n" << stream.headers() << std::endl;

            Envoy::Http::TestHeaderMapImpl response_headers{{":status", "200"}};

            bool end_stream = true;
            stream.encodeHeaders(response_headers, end_stream);
        };

        class HttpConnection : public Envoy::FakeHttpConnection {
        public:
            HttpConnection(Envoy::SharedConnectionWrapper &shared_connection, Envoy::Stats::Store &store,
                           Type type, Upstream &upstream)
                : FakeHttpConnection(shared_connection, store, type, upstream.timeSystem()),
                  upstream_(upstream) {}

            virtual ~HttpConnection() {}

            Envoy::Http::StreamDecoder &newStream(Envoy::Http::StreamEncoder &response_encoder) override;

            Upstream &upstream() {
              return upstream_;
            }

        private:
            Upstream &upstream_;
            // TODO not sure this is useful, try remvoing it.
            std::vector<Envoy::FakeStreamPtr> streams_;
        };

        typedef std::unique_ptr<HttpConnection> HttpConnectionPtr;

        class Stream : public Envoy::FakeStream {
        public:
            Stream(HttpConnection &connection, Envoy::Http::StreamEncoder &encoder, Upstream &upstream)
                : FakeStream(connection, encoder, upstream.timeSystem()), connection_(connection) {}

            virtual ~Stream() {
              RELEASE_ASSERT(complete(), "Stream dtor called before completion");
            }

            void setEndStream(bool set) override;

        private:
            // TODO: this could be eliminated if the base class made it protected.
            HttpConnection &connection_;
        };

        // handle request completion
        void Stream::setEndStream(bool end_stream) {
          FakeStream::setEndStream(end_stream);

          if (!end_stream) {
            return;
          }

          TransactionHandler handler = connection_.upstream().handler();
          handler(*this, connection_.upstream().name());
        }

        Envoy::Http::StreamDecoder &HttpConnection::newStream(Envoy::Http::StreamEncoder &response_encoder) {
          auto stream = new Stream(*this, response_encoder, upstream_);
          streams_.push_back(Envoy::FakeStreamPtr{stream});
          return *(stream);
        }

        Upstream::Upstream(const char *name, uint32_t port, Envoy::FakeHttpConnection::Type type,
                           Envoy::Network::Address::IpVersion version, Envoy::Event::TestTimeSystem &time_system)
            : FakeUpstream(port, type, version, time_system), name_(name) {
          default_handler_ = DefaultTransactionHandler;
          handler_ = DefaultTransactionHandler;
        }

        Upstream::~Upstream() {
          // TODO Make sure the dispatcher is stopped before the connections are destroyed.
          cleanUp();
          http_connections_.clear();
        }

        bool Upstream::createNetworkFilterChain(Envoy::Network::Connection &connection,
                                                const std::vector<Envoy::Network::FilterFactoryCb> &) {
          shared_connections_.emplace_back(new Envoy::SharedConnectionWrapper(connection, true));
          HttpConnectionPtr http_connection(new HttpConnection(*shared_connections_.back(), stats_store_,
                                                               http_type_, *this));
          testing::AssertionResult result = http_connection->initialize();
          RELEASE_ASSERT(result, result.message());
          http_connections_.push_back(std::move(http_connection));
          return true;
        }

        bool Upstream::createListenerFilterChain(Envoy::Network::ListenerFilterManager &) {
          return true;
        }

    }
}