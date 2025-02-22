// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <mutex>

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"

#include "opentelemetry/exporters/otlp/otlp_grpc_client.h"
#include "opentelemetry/exporters/otlp/otlp_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"
#include "opentelemetry/sdk_config.h"

#include <grpcpp/grpcpp.h>

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{
// -------------------------------- Constructors --------------------------------

OtlpGrpcExporter::OtlpGrpcExporter() : OtlpGrpcExporter(OtlpGrpcExporterOptions()) {}

OtlpGrpcExporter::OtlpGrpcExporter(const OtlpGrpcExporterOptions &options)
    : options_(options),
      trace_service_stub_(
          OtlpGrpcClient::MakeServiceStub<proto::collector::trace::v1::TraceService>(options))
{}

OtlpGrpcExporter::OtlpGrpcExporter(
    std::unique_ptr<proto::collector::trace::v1::TraceService::StubInterface> stub)
    : options_(OtlpGrpcExporterOptions()), trace_service_stub_(std::move(stub))
{}

// ----------------------------- Exporter methods ------------------------------

std::unique_ptr<sdk::trace::Recordable> OtlpGrpcExporter::MakeRecordable() noexcept
{
  return std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);
}

sdk::common::ExportResult OtlpGrpcExporter::Export(
    const nostd::span<std::unique_ptr<sdk::trace::Recordable>> &spans) noexcept
{
  if (isShutdown())
  {
    OTEL_INTERNAL_LOG_ERROR("[OTLP gRPC] Exporting " << spans.size()
                                                     << " span(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }
  if (spans.empty())
  {
    return sdk::common::ExportResult::kSuccess;
  }

  proto::collector::trace::v1::ExportTraceServiceRequest request;
  OtlpRecordableUtils::PopulateRequest(spans, &request);

  auto context = OtlpGrpcClient::MakeClientContext(options_);
  proto::collector::trace::v1::ExportTraceServiceResponse response;

  grpc::Status status = trace_service_stub_->Export(context.get(), request, &response);

  if (!status.ok())
  {

    OTEL_INTERNAL_LOG_ERROR(
        "[OTLP TRACE GRPC Exporter] Export() failed: " << status.error_message());
    return sdk::common::ExportResult::kFailure;
  }
  return sdk::common::ExportResult::kSuccess;
}

bool OtlpGrpcExporter::Shutdown(std::chrono::microseconds /* timeout */) noexcept
{
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  is_shutdown_ = true;
  return true;
}

bool OtlpGrpcExporter::isShutdown() const noexcept
{
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  return is_shutdown_;
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
