# Aggregate Runtime Telemetry Contract (C1)

Runtime telemetry is process-wide operational data, separate from session entities and offline
evaluation. C++ records fixed counters through `backend/infrastructure/RuntimeMetrics.cpp` and
writes a bounded flat JSONL stream through `backend/infrastructure/RuntimeMetricsWriter.cpp`.
No public metrics endpoint, runtime Python or Azure SDK is added to the backend.
`RUNTIME_METRICS_ENABLED` defaults to false for the local profile and is true in the Azure Compose
profile. It controls only this aggregate exporter; evaluation tooling remains explicit and separate.

## Schema and measurement

Emit one snapshot every 60 seconds, at most 2 KiB per UTF-8 line. Schema v1 has only these fields;
reject unknown keys and nonfinite/negative numeric values before writing. Fields are never built
from user text, filenames, URLs, headers or arbitrary label maps.

| Fields | Type and meaning |
|--------|------------------|
| TimeGenerated | UTC ISO-8601 sample timestamp; timestamp clock is not used to measure inference duration |
| SchemaVersion | Integer 1 |
| ProcessInstanceId | Random UUID generated at process startup, unrelated to sessions/users |
| SampleSequence | Nonnegative integer, increasing per process; increments for every sampling tick, including dropped writes |
| FramesAcceptedTotal | Nonnegative cumulative integer; validated camera/image/decoded-video frames admitted to the vision queue |
| FramesProcessedTotal | Nonnegative cumulative integer; frames whose inference completed successfully, including zero-detection frames |
| FramesDroppedTotal | Nonnegative cumulative integer; accepted pending frames displaced before inference |
| InferenceErrorsTotal | Nonnegative cumulative integer; failed inference calls |
| InferenceCountTotal, InferenceDurationMsTotal | Nonnegative cumulative count and duration sum of completed successful inference calls; durations measured with a monotonic clock |
| InferenceLe25MsTotal, InferenceLe50MsTotal, InferenceLe100MsTotal, InferenceLe250MsTotal, InferenceLe500MsTotal, InferenceLe1000MsTotal | Nonnegative cumulative counts at fixed inclusive latency boundaries; InferenceCountTotal supplies the final infinity bucket |
| ActiveFrames, PendingFrames, PendingAnnouncements, OfferedAudio | Instantaneous queue gauges bounded respectively by 1, 1, 3 and 1 |
| RssBytes | Nonnegative current backend resident-memory gauge |
| TelemetryDroppedSamplesTotal | Nonnegative cumulative count of snapshots dropped due to queue, serialization, file or capacity failures |

All counters reset only on process restart; use the new ProcessInstanceId to separate sequences.
Store counters as checked nonnegative 64-bit integers (Log Analytics long), durations as finite
real numbers and TimeGenerated as datetime. The writer uses a single pending snapshot; if a write
cannot keep pace, replace/drop telemetry and increment TelemetryDroppedSamplesTotal. Never block
capture, inference, audio or session cleanup on metrics I/O, and never buffer a replay backlog.

Snapshots contain no session/request/track/description/announcement IDs, recognised categories/text,
media, audio, free-form messages or per-user labels. Instance/sequence identify process samples only.
Manual repeat/recovery does not enter capture-to-speech statistics. These backend histograms measure
inference latency, not browser end-to-end or acoustic latency; SC-007 still uses the separate explicit
evaluation procedure and exact samples in quickstart.md.

## Bounded local output and Azure ingestion

Only the dedicated host directory `/var/log/road-sign/runtime/` is mounted writable into the
otherwise read-only backend. It stores sanitized aggregate telemetry, never request spools.
Append to UTC daily `runtime-YYYY-MM-DD.jsonl` files; retain current plus three previous days,
at most 4 MiB per file and 16 MiB total. Preserve at least two days for collection. Do not truncate
or rename a collected file into another matching filename; if a capacity/write bound is reached,
drop new telemetry and keep serving recognition. Readiness does not depend on the collector.

Azure Monitor Agent on the VM reads only that exact JSONL pattern using managed identity. Terraform
creates `RoadSignRuntime_CL`, then a Linux DCR with JSON log-file input, explicit typed stream schema,
an allowlist projection and output `Custom-RoadSignRuntime_CL`, and associates the DCR with the VM.
Exclude RawData, FilePath and all unexpected fields from the output. Use the existing authenticated
public Monitor endpoint; no separate ingestion secret or DCE is needed for this chosen source.
Private Link would require a separate network design.
[JSON collection requirements](https://learn.microsoft.com/en-us/azure/azure-monitor/vm/data-collection-log-json),
[Terraform DCR](https://registry.terraform.io/providers/hashicorp/azurerm/latest/docs/resources/monitor_data_collection_rule),
[agent identity](https://learn.microsoft.com/en-us/azure/azure-monitor/agents/azure-monitor-agent-requirements),
[DCE requirements](https://learn.microsoft.com/en-us/azure/azure-monitor/data-collection/data-collection-endpoint-overview).

The agent's own cache is separate from the 16-MiB application files. Configure its Linux disk quota
to 4,000 MB and reserve that space on the VM OS disk; report agent disk/memory usage separately
from backend container limits. Retain the custom table for 30 days with no extended archive or raw
Storage/Event Hub export. Delivery can drop data during prolonged outages and is not exactly once.
[Agent quota](https://learn.microsoft.com/en-us/azure/azure-monitor/agents/agent-settings),
[table retention](https://learn.microsoft.com/en-us/azure/azure-monitor/logs/data-retention-configure).

Workbook and alert queries deduplicate by ProcessInstanceId + SampleSequence before computing
deltas/rates. Compare consecutive cumulative samples only within the same process; exclude a
negative/reset delta, show sample gaps and treat absent telemetry as unknown, never zero traffic.
Compute histogram quantiles from bucket deltas, label them approximate and show overflow above
1,000 ms; never average per-window p95 values. Queue gauges are sampled values, not guaranteed
between-sample peaks. Display freshness and alert after three missing 60-second samples, alongside
the existing readiness/resource/security alerts.

## Acceptance

Native tests use a fake clock and fixed frame completions to verify counter/bucket boundaries,
queue gauges, reset identity, 60-second cadence and the 2-KiB ceiling. Inject write failures, a full
directory and a stalled collector; prove bounded memory/disk and uninterrupted session/audio work.
Validate rotation without truncation, secret/text canary rejection and strict schema projection.
Live Azure tests inject known aggregate samples, then query RoadSignRuntime_CL and the Workbook:
verify values, duplicate deduplication, gap/reset handling, missing-telemetry alerts and retention.
Do not claim local fixtures prove that ingestion or operational alerts work in Azure.
