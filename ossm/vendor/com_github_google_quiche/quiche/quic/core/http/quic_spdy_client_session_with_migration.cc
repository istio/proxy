// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_spdy_client_session_with_migration.h"

#include "quiche/quic/core/quic_force_blockable_packet_writer.h"

namespace quic {

QuicSpdyClientSessionWithMigration::QuicSpdyClientSessionWithMigration(
    QuicConnection* connection, QuicForceBlockablePacketWriter* writer,
    QuicSession::Visitor* visitor, const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    QuicNetworkHandle default_network, QuicNetworkHandle current_network,
    std::unique_ptr<QuicPathContextFactory> path_context_factory,
    QuicConnectionMigrationConfig migration_config,
    QuicPriorityType priority_type)
    : QuicSpdyClientSessionBase(connection, visitor, config, supported_versions,
                                priority_type),
      path_context_factory_(std::move(path_context_factory)),
      migration_manager_(this, connection->clock(), default_network,
                         current_network, path_context_factory_.get(),
                         migration_config),
      writer_(writer),
      most_recent_stream_close_time_(connection->clock()->ApproximateNow()) {
  QUICHE_DCHECK(writer_ == nullptr || writer_ == connection->writer())
      << "Writer is should be either null or the connection writer";
  if (migration_config.migrate_session_on_network_change ||
      migration_config.allow_port_migration ||
      migration_config.allow_server_preferred_address) {
    QUICHE_DCHECK_EQ(writer_, connection->writer())
        << "Writer is not the connection writer";
  }
}

QuicSpdyClientSessionWithMigration::~QuicSpdyClientSessionWithMigration() =
    default;

void QuicSpdyClientSessionWithMigration::OnPathDegrading() {
  QuicSpdyClientSessionBase::OnPathDegrading();
  migration_manager_.OnPathDegrading();
}

void QuicSpdyClientSessionWithMigration::OnTlsHandshakeComplete() {
  QuicSpdyClientSessionBase::OnTlsHandshakeComplete();
  migration_manager_.OnHandshakeCompleted(*config());
}

void QuicSpdyClientSessionWithMigration::SetDefaultEncryptionLevel(
    EncryptionLevel level) {
  QuicSpdyClientSessionBase::SetDefaultEncryptionLevel(level);
  if (level == ENCRYPTION_FORWARD_SECURE) {
    migration_manager_.OnHandshakeCompleted(*config());
  }
}

bool QuicSpdyClientSessionWithMigration::MigrateToNewPath(
    std::unique_ptr<QuicClientPathValidationContext> path_context) {
  if (!PrepareForMigrationToPath(*path_context)) {
    QUIC_CLIENT_HISTOGRAM_BOOL("QuicSession.PrepareForMigrationToPath", false,
                               "");
    return false;
  }
  const bool success = MigratePath(
      path_context->self_address(), path_context->peer_address(),
      path_context->WriterToUse(), path_context->ShouldConnectionOwnWriter());

  if (!success) {
    migration_manager_.OnMigrationFailure(
        QuicConnectionMigrationStatus::MIGRATION_STATUS_NO_UNUSED_CONNECTION_ID,
        "No unused server connection ID");
    QUIC_DVLOG(1) << "MigratePath fails as there is no CID available";
  }
  writer_ = path_context->ForceBlockableWriterToUse();
  QUICHE_DCHECK_EQ(writer_, connection()->writer());
  OnMigrationToPathDone(std::move(path_context), success);
  return success;
}

void QuicSpdyClientSessionWithMigration::OnServerPreferredAddressAvailable(
    const QuicSocketAddress& server_preferred_address) {
  QUICHE_DCHECK(version().HasIetfQuicFrames());
  QuicSpdyClientSessionBase::OnServerPreferredAddressAvailable(
      server_preferred_address);
  migration_manager_.MaybeStartMigrateSessionToServerPreferredAddress(
      server_preferred_address);
}

void QuicSpdyClientSessionWithMigration::SetMigrationDebugVisitor(
    QuicConnectionMigrationDebugVisitor* visitor) {
  migration_manager_.set_debug_visitor(visitor);
}

const QuicConnectionMigrationConfig&
QuicSpdyClientSessionWithMigration::GetConnectionMigrationConfig() const {
  return migration_manager_.config();
}

void QuicSpdyClientSessionWithMigration::OnStreamClosed(
    QuicStreamId stream_id) {
  most_recent_stream_close_time_ = connection()->clock()->ApproximateNow();
  QuicSpdyClientSessionBase::OnStreamClosed(stream_id);
}

QuicTimeDelta QuicSpdyClientSessionWithMigration::TimeSinceLastStreamClose() {
  return connection()->clock()->ApproximateNow() -
         most_recent_stream_close_time_;
}

}  // namespace quic
