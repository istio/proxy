/* Copyright 2017 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/envoy/tcp/mixer/filter.h"

#include "common/common/enum_to_int.h"
#include "extensions/filters/network/well_known_names.h"
#include "src/envoy/utils/utils.h"

using ::google::protobuf::util::Status;
using ::istio::mixerclient::CheckResponseInfo;

namespace Envoy {
namespace Tcp {
namespace Mixer {

Filter::Filter(Control &control) : control_(control) {
  ENVOY_LOG(debug, "Called tcp filter: {}", __func__);
}

Filter::~Filter() {
  cancelCheck();
  ENVOY_LOG(debug, "Called tcp filter : {}", __func__);
}

void Filter::initializeReadFilterCallbacks(
    Network::ReadFilterCallbacks &callbacks) {
  ENVOY_LOG(debug, "Called tcp filter: {}", __func__);
  filter_callbacks_ = &callbacks;
  filter_callbacks_->connection().addConnectionCallbacks(*this);
  start_time_ = std::chrono::system_clock::now();
}

void Filter::cancelCheck() {
  if (state_ != State::Calling && handler_) {
    handler_->ResetCancel();
  }
  state_ = State::Closed;
  if (handler_) {
    handler_->CancelCheck();
  }
}

// Makes a Check() call to Mixer.
void Filter::callCheck() {
  state_ = State::Calling;
  filter_callbacks_->connection().readDisable(true);
  calling_check_ = true;
  handler_->Check(
      this, [this](const CheckResponseInfo &info) { completeCheck(info); });
  calling_check_ = false;
}

// TODO(venilnoronha): rewrite this to deep-clone dynamic metadata for all
// filters.
void Filter::cacheFilterMetadata(
    const ::google::protobuf::Map<std::string, ::google::protobuf::Struct>
        &filter_metadata) {
  for (auto &filter_pair : filter_metadata) {
    if (filter_pair.first ==
        Extensions::NetworkFilters::NetworkFilterNames::get().MongoProxy) {
      if (cached_filter_metadata_.count(filter_pair.first) == 0) {
        ProtobufWkt::Struct dynamic_metadata;
        cached_filter_metadata_[filter_pair.first] = dynamic_metadata;
      }

      auto &cached_fields =
          *cached_filter_metadata_[filter_pair.first].mutable_fields();
      for (const auto &message_pair : filter_pair.second.fields()) {
        cached_fields[message_pair.first].mutable_list_value()->CopyFrom(
            message_pair.second.list_value());
      }
    }
  }
}

void Filter::clearCachedFilterMetadata() { cached_filter_metadata_.clear(); }

// Network::ReadFilter
Network::FilterStatus Filter::onData(Buffer::Instance &data, bool) {
  if (state_ == State::NotStarted) {
    // By waiting to invoke the callCheck() at onData(), the call to Mixer
    // will have sufficient SSL information to fill the check Request.
    callCheck();
  }

  ENVOY_CONN_LOG(debug, "Called tcp filter onRead bytes: {}",
                 filter_callbacks_->connection(), data.length());
  received_bytes_ += data.length();

  // Envoy filters like the mongo_proxy filter clear previously set dynamic
  // metadata on each onData call. Since the Mixer filter sends metadata based
  // on a timer event, it's possible that the previously set metadata is cleared
  // off by the time the event is fired. Therefore, we append metadata from each
  // onData call to a local cache and send it all at once when the timer event
  // occurs. The local cache is cleared after reporting it on the timer event.
  cacheFilterMetadata(filter_callbacks_->connection()
                          .streamInfo()
                          .dynamicMetadata()
                          .filter_metadata());

  return (state_ == State::Calling || filter_callbacks_->connection().state() !=
                                          Network::Connection::State::Open)
             ? Network::FilterStatus::StopIteration
             : Network::FilterStatus::Continue;
}

// Network::WriteFilter
Network::FilterStatus Filter::onWrite(Buffer::Instance &data, bool) {
  ENVOY_CONN_LOG(debug, "Called tcp filter onWrite bytes: {}",
                 filter_callbacks_->connection(), data.length());
  send_bytes_ += data.length();
  return Network::FilterStatus::Continue;
}

Network::FilterStatus Filter::onNewConnection() {
  ENVOY_CONN_LOG(debug,
                 "Called tcp filter onNewConnection: remote {}, local {}",
                 filter_callbacks_->connection(),
                 filter_callbacks_->connection().remoteAddress()->asString(),
                 filter_callbacks_->connection().localAddress()->asString());

  handler_ = control_.controller()->CreateRequestHandler();
  handler_->BuildCheckAttributes(this);
  // Wait until onData() is invoked.
  return Network::FilterStatus::Continue;
}

void Filter::completeCheck(const CheckResponseInfo &info) {
  const auto &status = info.status();
  ENVOY_LOG(debug, "Called tcp filter completeCheck: {}", status.ToString());
  handler_->ResetCancel();
  if (state_ == State::Closed) {
    return;
  }
  state_ = State::Completed;

  Utils::CheckResponseInfoToStreamInfo(
      info, filter_callbacks_->connection().streamInfo());

  filter_callbacks_->connection().readDisable(false);

  if (!status.ok()) {
    filter_callbacks_->connection().close(
        Network::ConnectionCloseType::NoFlush);
  } else {
    if (!calling_check_) {
      filter_callbacks_->continueReading();
    }
    handler_->Report(this, ConnectionEvent::OPEN);
    report_timer_ =
        control_.dispatcher().createTimer([this]() { OnReportTimer(); });
    report_timer_->enableTimer(control_.config().report_interval_ms());
  }
}

// Network::ConnectionCallbacks
void Filter::onEvent(Network::ConnectionEvent event) {
  if (filter_callbacks_->upstreamHost()) {
    ENVOY_CONN_LOG(debug, "Called tcp filter onEvent: {} upstream {}",
                   filter_callbacks_->connection(), enumToInt(event),
                   filter_callbacks_->upstreamHost()->address()->asString());
  } else {
    ENVOY_CONN_LOG(debug, "Called tcp filter onEvent: {}",
                   filter_callbacks_->connection(), enumToInt(event));
  }

  if (event == Network::ConnectionEvent::RemoteClose ||
      event == Network::ConnectionEvent::LocalClose) {
    if (state_ != State::Closed && handler_) {
      if (report_timer_) {
        report_timer_->disableTimer();
      }
      handler_->Report(this, ConnectionEvent::CLOSE);
    }
    cancelCheck();
  }
}

bool Filter::GetSourceIpPort(std::string *str_ip, int *port) const {
  return Utils::GetIpPort(filter_callbacks_->connection().remoteAddress()->ip(),
                          str_ip, port);
}

bool Filter::GetPrincipal(bool peer, std::string *user) const {
  return Utils::GetPrincipal(&filter_callbacks_->connection(), peer, user);
}

bool Filter::IsMutualTLS() const {
  return Utils::IsMutualTLS(&filter_callbacks_->connection());
}

bool Filter::GetRequestedServerName(std::string *name) const {
  return Utils::GetRequestedServerName(&filter_callbacks_->connection(), name);
}

bool Filter::GetDestinationIpPort(std::string *str_ip, int *port) const {
  if (filter_callbacks_->upstreamHost() &&
      filter_callbacks_->upstreamHost()->address()) {
    return Utils::GetIpPort(filter_callbacks_->upstreamHost()->address()->ip(),
                            str_ip, port);
  } else {
    return Utils::GetIpPort(
        filter_callbacks_->connection().localAddress()->ip(), str_ip, port);
  }
  return false;
}

bool Filter::GetDestinationUID(std::string *uid) const {
  if (filter_callbacks_->upstreamHost()) {
    return Utils::GetDestinationUID(
        *filter_callbacks_->upstreamHost()->metadata(), uid);
  }
  return false;
}

const ::google::protobuf::Map<std::string, ::google::protobuf::Struct>
    &Filter::GetDynamicFilterState() const {
  return cached_filter_metadata_;
}

void Filter::GetReportInfo(
    ::istio::control::tcp::ReportData::ReportInfo *data) const {
  data->received_bytes = received_bytes_;
  data->send_bytes = send_bytes_;
  data->duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now() - start_time_);
}

std::string Filter::GetConnectionId() const {
  char connection_id_str[32];
  StringUtil::itoa(connection_id_str, 32, filter_callbacks_->connection().id());
  std::string uuid_connection_id = control_.uuid() + "-";
  uuid_connection_id.append(connection_id_str);
  return uuid_connection_id;
}

void Filter::OnReportTimer() {
  handler_->Report(this, ConnectionEvent::CONTINUE);
  clearCachedFilterMetadata();
  report_timer_->enableTimer(control_.config().report_interval_ms());
}

}  // namespace Mixer
}  // namespace Tcp
}  // namespace Envoy
