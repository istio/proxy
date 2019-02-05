#pragma once

#include "test/integration/fake_upstream.h"

namespace Mixer {
    namespace ControlledUpstream {

        extern void DumpHeaders(const std::string& message, const Envoy::Http::HeaderMap& headers);

        using TransactionHandler = std::function<void(Envoy::FakeStream &stream, const std::string &upstream_name)>;

        class Upstream : public Envoy::FakeUpstream {
        public:
            static const char TRANSACTION_ID_HEADER_NAME[];

            Upstream(const char *name, uint32_t port, Envoy::FakeHttpConnection::Type type,
                     Envoy::Network::Address::IpVersion version, Envoy::Event::TestTimeSystem &time_system);

            virtual ~Upstream();

            bool createNetworkFilterChain(Envoy::Network::Connection &connection,
                                          const std::vector<Envoy::Network::FilterFactoryCb> &filter_factories) override;

            bool createListenerFilterChain(Envoy::Network::ListenerFilterManager &listener) override;

            TransactionHandler handler() {
              Envoy::Thread::LockGuard lock(handler_lock_);
              return handler_;
            }

            void setHandler(TransactionHandler handler) {
              Envoy::Thread::LockGuard lock(handler_lock_);
              handler_ = handler;
            }

            void unsetHandler() {
              Envoy::Thread::LockGuard lock(handler_lock_);
              handler_ = default_handler_;
            }

            const std::string &name() const {
              return name_;
            }

        private:

            TransactionHandler default_handler_;
            TransactionHandler handler_;
            Envoy::Thread::MutexBasicLockable handler_lock_;

            // TODO not sure either of these are useful, try remvoing them.
            std::vector<Envoy::FakeHttpConnectionPtr> http_connections_;
            std::vector<Envoy::SharedConnectionWrapperPtr> shared_connections_;
            std::string name_;
        };

    } // namespace ControlledUpstream
} // namespace Mixer

