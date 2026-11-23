# Azure, Terraform and GitHub Actions Delivery Design

This is an implementation contract, not evidence of provisioned resources or working workflows.
It extends [plan.md](plan.md) for the requested cloud deployment. The application remains one C++
modular monolith plus a static TypeScript client, packaged with Docker build and Docker Compose.
Python remains offline training tooling. No Azure OCR or speech service replaces native inference.

## Deployment boundary and service inventory

Start in UK South, subject to subscription quota and SKU availability. Use one environment at a
time for this controlled, single-session prototype; separate resource groups and state keys when
both development and demonstration environments are needed. Internet reachability does not make
this a public multi-user service: block clients outside the configured tester CIDRs at the gateway.
Update that list for phone networks before testing; do not add a public bypass to solve access issues.

| Required service/resource | Purpose and initial configuration |
|---------------------------|-----------------------------------|
| Resource groups and tags | Separate state/bootstrap from environment resources; owner, environment, project and expiry tags |
| Virtual Network and NSGs | Dedicated gateway subnet and private VM subnet; only gateway-to-VM HTTPS ingress; no public SSH |
| Application Gateway WAF_v2 and Standard public IP | Single HTTPS entry point, health probes, WAF managed rules, bot rules and route-specific rate limits |
| Azure DDoS IP Protection | Explicitly protect the gateway public IP; do not rely only on baseline platform mitigation |
| NAT Gateway and outbound public IP | Explicit egress for the private VM to pull images, obtain certificates and install updates |
| Azure VM and managed OS disk | Ubuntu 24.04, Standard_D4s_v5 (4 vCPU, 16 GiB), Docker Engine/Compose; no public VM IP |
| Azure Container Registry Basic | Private backend/web images and versioned model OCI artifacts; admin login disabled; classic registry RBAC mode |
| Azure Key Vault Standard | TLS certificate and any required deployment secrets; RBAC, soft delete and purge protection |
| Microsoft Entra workload identities and Azure managed identities | GitHub OIDC federation, gateway certificate access, VM image/artifact pull and certificate retrieval |
| Azure Storage account and private Blob containers | Terraform state and short-lived saved plans in separate containers; Entra authentication, locking/recovery and scoped access; no demo media |
| Azure Monitor and Log Analytics | Resource metrics, sanitized application/security logs, VM agent data collection and KQL analysis |
| Azure Monitor Workbooks, alerts and Action Group | Latency/resource/error/WAF/DDoS dashboards and operational alerts to configured maintainers |
| Azure Cost Management budget | Environment cost visibility and notifications; owner-supplied budget before apply |
| DNS record and public TLS certificate | Existing owned domain; use Azure DNS if Azure manages its zone, otherwise update the existing DNS provider |

The VM size exceeds the local 4-core/8-GiB minimum; backend limits remain 4 GiB with a 256-MiB
tmpfs. Private VMs need explicit outbound connectivity. DDoS IP Protection covers network-layer
attacks; WAF covers HTTP-layer threats. These are complementary controls.
[VM sizes](https://learn.microsoft.com/en-us/azure/virtual-machines/sizes/general-purpose/dsv5-series),
[outbound networking](https://learn.microsoft.com/en-us/azure/virtual-network/ip-services/default-outbound-access),
[DDoS tiers](https://learn.microsoft.com/en-us/azure/ddos-protection/ddos-protection-overview).

Optional, with a concrete trigger: Bastion/VPN for interactive administration; private endpoints
and a private runner for stronger service-network isolation; Defender for Cloud for expanded
posture/vulnerability management; Sentinel for an operated SIEM; Azure Machine Learning/GPU VMs
and separate Blob storage for authorised offline research at scale; DDoS Network Protection for
a larger protected estate. Application Insights is optional if application tracing becomes useful;
Monitor/Log Analytics already provide the requested logging and analytics. AKS, a database, Redis,
Service Bus, API Management and Front Door are not needed by this single-VM design.

## Network, HTTPS and media compatibility

Traffic flows from browser HTTPS to Application Gateway, then HTTPS to Caddy on the VM private IP,
then to the C++ service on the Compose internal network. Caddy serves UI and API on the same origin.
The gateway forwards the configured application hostname for backend Host/SNI and verifies its TLS
certificate, with TLS 1.2 or later on both hops. Both gateway and VM use an enabled, exportable
software-backed Key Vault PFX certificate issued for that hostname; local
development's Caddy internal CA is not used for Azure. Fetch the VM certificate using managed
identity into a root-owned tmpfs location, mount it read-only into Caddy and renew/reload atomically.
Never put PFX bytes/passwords or private keys into Terraform inputs, state, cloud-init or build logs.
Terraform references the versionless Key Vault secret URI; certificate issuance/import and renewal
are separately documented operational steps. Alert on expiry and test rotation before release.
[Gateway Key Vault integration](https://learn.microsoft.com/en-us/azure/application-gateway/key-vault-certs).

The VM NSG permits port 443 only from the gateway subnet. Preserve documented gateway management
and Azure Load Balancer traffic on the gateway subnet; a blanket deny must not break the gateway.
No application port or private upload-admission route is publicly exposed. Trust forwarded headers
only from the gateway through Caddy, reject an unexpected Host/Origin, and retain the cookie/CSRF
and ownership rules in [contracts/api.md](contracts/api.md).

Disable gateway response buffering (`global.response_buffering_enabled=false`), use a 120-second
backend timeout, flush SSE immediately in Caddy and retain five-second heartbeats. Test an actual
ten-minute stream; HTTP readiness alone cannot validate streaming. Keep API/audio/SSE responses
uncached and preserve no-store headers.
[SSE guidance](https://learn.microsoft.com/en-us/azure/application-gateway/use-server-sent-events),
[Terraform gateway settings](https://registry.terraform.io/providers/hashicorp/azurerm/latest/docs/resources/application_gateway).

Use WAF CRS 3.2 or a validated successor with enforced 2-MiB ordinary request and 110-MB file-upload
ceilings. The existing 1-MiB raw JPEG camera request fits the former. Uploaded images/videos must
use multipart file parts with a filename; other content types use the ordinary request limit.
Caddy/backend retain the stricter 101-MiB multipart, 100-MiB video and 10-MiB image limits. Test the
maximum accepted sizes and the first rejected sizes through the real gateway. Configure and report
the inspection depth separately: permitting a video upload does not mean WAF inspects its entire
contents. Native decoder validation, duration/pixel limits and sandboxing remain required.
[Request and upload limits](https://learn.microsoft.com/en-us/azure/web-application-firewall/ag/application-gateway-waf-request-size-limits).

WAF can buffer requests even with request buffering disabled. Pre-body admission protects
application-controlled spools, not resources already consumed at Azure's edge. Disable application
swap/core dumps and persistent request buffering; verify after VM reboot and Docker restart,
including Azure agent/cloud-init swap settings. The no-durable-media guarantee covers our process,
containers and VM. Do not promise RAM-only handling inside managed Azure infrastructure or claim
that WAF log scrubbing removes Microsoft's internal security telemetry. Disclose cloud processing
before the user starts capture/upload.
[Proxy buffers](https://learn.microsoft.com/en-us/azure/application-gateway/proxy-buffers),
[Linux agent configuration](https://learn.microsoft.com/en-us/azure/virtual-machines/extensions/agent-linux).

## Required security and observability controls

These IDs trace the additional user requirements to implementation tasks without changing the
recognition feature's FR/SC identifiers.

| ID | Required behavior and acceptance evidence |
|----|-------------------------------------------|
| AZ-001 WAF | Prevention mode, OWASP CRS 3.2 or validated successor; benign recognition traffic succeeds and safe known-malicious test requests are blocked; narrowly justified exclusions only |
| AZ-002 Rate limiting | Separate ClientAddr rules by method/path: initial session creation 10/min, uploads 10/min, camera frames 600/min, remaining API controls 120/min; sustained excess is blocked while 5-FPS camera and controls work |
| AZ-003 Bot protection | Microsoft_BotManagerRuleSet 1.1 or validated successor; block known bad bots, measure false positives and preserve screen-reader/browser use; no CAPTCHA or JavaScript challenge on API/SSE |
| AZ-004 Secrets and identity | OIDC for CI/CD, managed identities at runtime, narrowly scoped RBAC, no registry admin credentials or long-lived cloud client secrets; certificate rotation succeeds |
| AZ-005 Logs and analytics | Sanitized resource/application logs, Monitor metrics, KQL Workbooks and alerts; planted synthetic secrets/text/session IDs never appear in retained logs |
| AZ-006 DDoS | Gateway Standard public IP has explicit IP Protection enabled and alert wiring verified; configuration validation, not an unauthorised attack simulation |
| AZ-007 Terraform | Reproducible locked providers, separate remote state, reviewed plans, state locking/recovery and no secret material in state/artifacts |
| AZ-008 GitHub Actions | Required PR checks, tested immutable releases, protected OIDC apply/deploy, serialized deployments, verified health and rollback evidence |

The rate values are editable starting policies, not exact admission quotas. Edge limits aggregate
clients behind the same NAT and are approximate; backend session locking and per-session bounds
remain authoritative. Use the actual gateway client address, not caller-supplied X-Forwarded-For.
Use a negated CIDR **Block** rule for the tester boundary; an **Allow** rule must not short-circuit
managed inspection or size limits. Frontend errors must handle non-JSON gateway denials and 403/429
without automatic upload retry or a repeated-audio backlog.
[Rate limiting](https://learn.microsoft.com/en-us/azure/web-application-firewall/ag/rate-limiting-overview),
[bot rules](https://learn.microsoft.com/en-us/azure/web-application-firewall/ag/bot-protection-overview).

Send resource-specific AGWAccessLogs and AGWFirewallLogs to Log Analytics with ingestion
transformations. Enable WAF scrubbing for cookies, authorization/CSRF headers, request arguments
and client IP. Before retention, strip query strings, session/audio/description identifiers in URI
paths, raw matched data and content-bearing diagnostic fields; retain normalized route, rule ID,
action, status, duration and aggregate counts. Do not also export unsanitized logs to Storage/Event
Hub. VM agent collection must select only sanitized application logs and necessary host counters,
not all syslog, HTTP bodies or Docker output indiscriminately. Set 30-day analytics retention and
restricted operator access. No browser session replay, transcripts, media or audio telemetry.
[WAF scrubbing](https://learn.microsoft.com/en-us/azure/web-application-firewall/ag/waf-sensitive-data-protection-configure),
[access log transformations](https://learn.microsoft.com/en-us/azure/azure-monitor/reference/tables/agwaccesslogs),
[firewall log transformations](https://learn.microsoft.com/en-us/azure/azure-monitor/reference/tables/agwfirewalllogs).

Workbooks show readiness/5xx, latency, processed/dropped frames, queue depth, VM memory/CPU,
WAF/rate/bot blocks and DDoS signals. Alert on unhealthy backend, sustained errors, memory pressure,
certificate expiry, DDoS events and budget thresholds. Security analytics must remain useful after
redaction; synthetic canaries verify absence of sensitive content. Cost alerts are notifications,
not a spending cap. WAF gateway, DDoS protection and NAT have costs while the app is idle; VM
deallocation alone does not stop those charges. Record an estimate and teardown owner before apply.
[Budget behavior](https://learn.microsoft.com/en-us/azure/cost-management-billing/costs/tutorial-acm-create-budgets).

## Terraform ownership and state

Planned files (created during implementation):

```text
infra/terraform/
  bootstrap/                 # state storage and federated identities; separate lifecycle
  stack/                     # one environment root; networking, compute, edge, identity, monitoring
  environments/              # non-secret dev.tfvars and demo.tfvars; separate backend keys
infra/cloud-init/            # minimal host provisioning; no secrets or application media
infra/tests/                 # plan assertions and synthetic redaction fixtures
scripts/deploy/             # release validation, Compose switch and rollback
.github/workflows/
  ci.yml
  terraform-plan.yml
  publish.yml
  deploy.yml                 # protected apply, deploy and explicit rollback modes
```

Use Terraform and AzureRM, pin tested CLI/provider versions and commit `.terraform.lock.hcl` for
each root. Keep one stack with clearly named files; do not create generic modules for every Azure
resource. Share the stack between environments through non-secret inputs. Validate region, tester
CIDRs, DNS hostname, certificate URI, VM size, retention, budgets and limits before planning.

Bootstrap state storage, precreated VM/gateway user-assigned identities and federated identities
with an authorised operator identity, then migrate
bootstrap state to its own remote key; record and securely remove the local state copy after
verification. Use the azurerm backend with `use_azuread_auth=true` and CI `use_oidc=true`, a private
container, encryption, shared-key authentication disabled, Blob locking/versioning and 30-day
soft-delete recovery. Scope Storage Blob Data Contributor to the state container. Default to the
authenticated public TLS endpoint for hosted runners; private endpoints require a networked runner
and are not implied by a private Blob container. State and plan files are sensitive even when no
application secrets are intentionally stored in them.
[Terraform Azure backend](https://developer.hashicorp.com/terraform/language/backend/azurerm).

Separate federated identities for plan, image publish, infrastructure apply and application deploy.
Plan receives resource read permissions and state locking access; publish receives ACR push only;
application deploy receives narrowly scoped VM Run Command/release permissions; infrastructure
apply receives environment-scoped resource permissions. Bootstrap owns federation and privileged
role delegation. Constrain apply's role-assignment permission to the named runtime identities and
explicit AcrPull/Key Vault roles within the environment scope; it cannot grant itself privileges
or assign arbitrary roles. Routine apply is not subscription Owner. The VM identity
receives AcrPull and access only to its required Key Vault certificate/secret. The gateway identity
receives its certificate access. No workflow downloads Key Vault secret values into Terraform.
[Constrained role delegation](https://learn.microsoft.com/en-us/azure/role-based-access-control/delegate-role-assignments-overview).

## GitHub Actions pipeline and release contract

All actions are pinned to reviewed commit SHAs, tools/dependencies are locked, and jobs use minimum
permissions (`contents: read` by default; `id-token: write` only in Azure-authenticated jobs).
Protect the main branch with required checks and review, and require review of workflow/infra files.
Never execute untrusted pull-request code under `pull_request_target` with Azure credentials.
Fork PRs receive no Azure identity, state access or privileged artifacts. Bind federation to the
actual repository/environment OIDC subject and audience; verify the repository's current subject
format rather than assuming a legacy name-only subject.
[GitHub Azure OIDC](https://docs.github.com/en/actions/how-tos/secure-your-work/security-harden-deployments/oidc-in-azure).

| Workflow | Trigger and gates | Outputs and privilege boundary |
|----------|-------------------|--------------------------------|
| ci.yml | PR and main push: native unit/contracts, ASan/UBSan, separate TSan, Python tests/types, strict TS/lint/Vitest, Playwright/axe, both Docker build targets and Compose smoke; Terraform fmt/validate and plan-policy fixtures | Test summaries and synthetic-fixture reports; no cloud credentials; absent required code/model checks are reported as pending, never silently green |
| terraform-plan.yml | Main push/manual trusted ref and weekly read-only drift: authenticated refresh/plan after static CI; PRs receive offline validation only | Reviewable plan summary and encrypted saved plan in restricted Azure Blob storage; exact commit, provider lock hash, backend key, checksum and expiry recorded; no apply permission |
| publish.yml | Main push after CI succeeds for the same SHA; verify custom bundle/parity/license evidence before publishing a runnable release | Push backend/web images and model artifact to ACR, capture immutable digests, dependency/SBOM and scan results; no infra/deploy permission |
| deploy.yml | Manual dispatch, main-only reviewed refs: separate protected infra-apply mode with plan ID and app-deploy mode with release ID | Apply the unexpired saved plan for the exact reviewed SHA; application deploy uses digests, OIDC and VM Run Command, tests readiness/recognition/SSE and records result; serialize per environment |
| deploy.yml rollback mode | Explicit previous successful release ID under the same protected environment | Verify retained image/model digests and compatibility, stop current session, restore Compose release and verify health; never overwrite Terraform state |

Configure GitHub Environment required reviewers and branch restrictions before enabling deploy;
verify the repository/account supports them. If unavailable, leave deployment disabled until an
equivalent enforced approval gate exists. A stale plan, changed commit/provider lock, changed state,
failed prerequisite or expired artifact requires a new plan/review. Retain saved plans for at most
24 hours in a private Blob container with separately scoped access; do not post raw plan JSON or
secrets in PR comments or public GitHub artifacts. Expose only a sanitized human-readable summary.
Use GitHub deployment concurrency with cancellation disabled for an in-progress apply/deploy.

The non-secret release manifest contains `releaseId`, `gitSha`, `backendImageDigest`,
`webImageDigest`, `modelArtifactDigest`, `modelManifestSha256`, `policySha256`,
`composeSha256`, `terraformCommit`, and evidence references. Validate required fields, digest
syntax, matching model/policy contract and tested commit before deployment. Keep the active and
previous successful manifests; protect their referenced ACR artifacts from retention deletion.
Model artifacts contain only authorised weights/catalogue/manifests, never ordinary session media.
The VM pulls using managed identity, checks hashes, stages the release, then runs Compose with
`--no-build`; the pipeline does not compile on the production VM.

Deployment is a controlled maintenance interruption: stop admission, terminate the current session
and audio, switch release, then reopen only after health/synthetic smoke passes. On failure restore
the previous compatible release; if rollback fails, keep admission closed and report failure.
Never restore sessions or replay an audio backlog. First deployment has no previous release and
must remain unavailable on failure. Application rollback is separate from Terraform rollback:
infrastructure changes require a new reviewed plan, never a copied old state file. Weekly read-only
drift plans report differences without automatic repair.

CI cannot replace the physical-phone/screen-reader checks, participant study or native-host
performance evidence in [quickstart.md](quickstart.md). Real-model release validation uses the
authorised custom bundle in a trusted job; synthetic CI fixtures must never become a production
recognition fallback. Publishing and deployment remain blocked when required evidence is absent.

## Cloud acceptance and operations

Before first apply, supply subscription/tenant, region/quota, repository/environments, operator
RBAC, domain/certificate, tester CIDRs, budget and alert recipients. These are deployment inputs,
not reasons to invent credentials or provision resources during planning. Apply bootstrap first,
then use protected infra-apply for the stack with `is_gateway_enabled=false` to create ACR, vault
and private compute. Import/issue the certificate outside Terraform, then review/apply a new plan
with `is_gateway_enabled=true` and its validated certificate URI. The closed gateway flag is only
for initial provisioning; acceptance requires it enabled with every protection. This allows first
infrastructure creation before any release manifest exists. Publish the first verified release
only after ACR is available, then use app-deploy. Preserve bootstrap state during environment teardown.

Record evidence for AZ-001–AZ-008: valid/max/oversized uploads and retry after failure, 5-FPS frames,
ten-minute SSE, cookie/CSRF rejection, blocked first speech recovery, cancellation receipts, phone
HTTPS, tester boundary/origin isolation, safe WAF/bot/rate checks, log-canary absence, certificate
rotation, host reboot without swap, alert delivery, state lock/recovery and failed-deploy rollback.
Measure end-to-end latency through Azure separately from the native inference benchmark. Do not
simulate volumetric DDoS traffic as an ordinary acceptance test.

Runbooks must cover bootstrap, renewal, secret rotation, patching, deployment interruption,
rollback, drift, redacted log queries, incident access, cost review and environment teardown.
Destroy requires its own reviewed plan and retains protected state/recovery artifacts; purge
protection and retained backups can outlive application resources and must be included in costs.
