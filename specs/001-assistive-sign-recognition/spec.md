# Feature Specification: Accessible Sign Recognition Web Prototype

**Feature Branch**: `001-assistive-sign-recognition`

**Created**: 2026-11-23

**Status**: Draft

**Input**: User description: "Create the specification from the agreed plan for an audio-first
web prototype helping blind and partially sighted people understand UK pedestrian-related signs,
using live camera input and uploaded images/videos, sign detection, tracking, text extraction,
and spoken announcements. Include custom model training and research evaluation; exclude
embedded systems."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Hear Sign Information (Priority: P1)

A blind or partially sighted user starts a camera session or uploads an image and hears supported
sign meanings and readable text without needing to inspect visual results.

**Why this priority**: Spoken environmental information is the prototype's primary user benefit.

**Independent Test**: Present a labelled example of each supported category through an image and
camera session, then compare the spoken information with the reference meaning and text.

**Acceptance Scenarios**:

1. **Given** a user has granted camera access, **When** they start recognition and present a
   supported sign, **Then** the system provides a spoken description of the accepted recognition
   result and any accepted readable text.
2. **Given** a supported uploaded image containing a street name, **When** recognition finishes,
   **Then** the user hears the extracted street name without needing to enable visual results.
3. **Given** a sign with a supported symbol and unreadable text, **When** the symbol is recognised,
   **Then** the system can announce the sign meaning without inventing the missing text.
4. **Given** unsupported or uncertain visual information, **When** recognition runs, **Then** the
   system does not fabricate a supported sign meaning; an image with no accepted result receives
   an accessible no-result status.
5. **Given** two supported sign categories in one scene, **When** recognition results are accepted,
   **Then** their meanings and text remain associated with the correct signs.

---

### User Story 2 - Follow Signs Across Changing Scenes (Priority: P1)

A user watches a live or uploaded video and hears relevant changes rather than the same sign
repeatedly. Pedestrian warnings and restrictions take priority over general information.

**Why this priority**: Repetition and obsolete announcements reduce the usefulness of continuous
recognition and can obscure more relevant information.

**Independent Test**: Replay annotated sequences with stationary signs, brief occlusions,
multiple signs, and changed text; compare announcement events and tracked identities with the
expected sequence. Test announcement policy separately with fixed recognition observations.

**Acceptance Scenarios**:

1. **Given** a sign has been announced, **When** it remains visible and unchanged across frames,
   **Then** it retains its identity and produces no additional automatic announcement.
2. **Given** a sign is briefly occluded, **When** it is matched again within the documented
   continuity window, **Then** identity continuity preserves its announcement history.
3. **Given** two distinct signs with identical wording, **When** both are observed, **Then** they
   retain separate identities; suppression is not based solely on matching text.
4. **Given** accepted text for an existing sign changes materially, **When** the updated result is
   consolidated, **Then** the change becomes eligible for announcement rather than being suppressed
   as an unchanged observation. Whitespace-only changes do not count as new information.
5. **Given** warnings, restrictions, directions, and general information await speech, **When** the
   next announcement is selected, **Then** warnings and restrictions precede directions, which
   precede general information; equal-priority items follow acceptance order.
6. **Given** a queued result becomes obsolete or its source is replaced, **When** playback advances,
   **Then** it does not speak the obsolete result. Separate announcements never overlap.

---

### User Story 3 - Control Recognition Without Visual Interaction (Priority: P1)

A user controls input, recognition, and audio through labelled controls usable with a keyboard or
screen reader, including on a phone browser.

**Why this priority**: Recognition cannot serve the intended audience if core controls require sight.

**Independent Test**: Complete source selection, start, mute/unmute, repeat, and stop workflows
using keyboard-only interaction and separately using a screen reader, with visual results hidden.

**Acceptance Scenarios**:

1. **Given** visual results are hidden, **When** the user navigates the controls, **Then** each control
   has an understandable accessible name and state, a logical focus order, and a visible focus
   indicator for users who can see it.
2. **Given** a session is active, **When** the user mutes audio, **Then** current application speech
   stops and automatic announcements remain silent until unmuted; unmuting does not replay a stale
   backlog.
3. **Given** an announcement exists and audio is unmuted, **When** the user selects repeat,
   **Then** the latest announcement is spoken again as an intentional replay. While muted, repeat
   does not bypass mute and its unavailable state is accessible.
4. **Given** capture, processing, or speech is active, **When** the user selects stop, **Then** capture
   and new processing cease, current speech and queued announcements are cancelled, and results
   arriving later do not restart playback.
5. **Given** camera permission is denied or an input fails, **When** the failure is detected,
   **Then** the user receives an accessible explanation and a recovery action without a silent
   or indefinite processing state.
6. **Given** audio playback cannot start, **When** a result is available, **Then** accessible status
   explains how to enable playback and the result remains available to the screen reader.

---

### User Story 4 - Review and Evaluate the Prototype (Priority: P2)

A developer or evaluator optionally views sign boxes, text, and session announcement history,
then evaluates recognition, speech usefulness, accessibility, and performance on labelled data.

**Why this priority**: Repeatable evidence is needed to understand the prototype's strengths and
limitations without claiming unsupported quality levels.

**Independent Test**: Compare a reference session with the optional visual display, and produce
an evaluation report from a fixed labelled evaluation set and recorded test conditions.

**Acceptance Scenarios**:

1. **Given** visual results are enabled and a matching local frame is available, **When** a sign is detected, **Then** its box, category, and
   available text refer to the same observation; hiding results does not disable recognition.
2. **Given** announcements occur, **When** the user reviews session history, **Then** chronological
   descriptions are accessible and intentional repeats are distinguishable from automatic speech.
3. **Given** a documented evaluation set, **When** evaluation completes, **Then** the report contains
   every metric and breakdown defined in the success criteria, with sample counts and limitations.
4. **Given** a custom detector is evaluated, **When** the evaluator checks its training record,
   **Then** dataset provenance, permitted use, labels, model identity, and source-separated held-out
   evaluation membership are documented.
5. **Given** the session ends, **When** another session starts, **Then** previous media, sign identities,
   recognised text, and announcement history are not available in the new session.
6. **Given** an uploaded video is supported by the backend but the browser cannot decode it locally
   or synchronize its displayed frame with detection timestamps, **When** visual results are requested,
   **Then** the overlay is disabled with an accessible explanation while backend recognition,
   text review and spoken feedback continue. The application does not draw stale boxes on another
   frame or request a server-generated preview image. This fallback is supported across the
   documented input/browser test matrix and does not count as failure of text/audio recognition.

### Edge Cases

- Denied/revoked permission, missing camera, disconnected camera, or a camera already in use:
  stop the affected source, announce accessible status, and allow another input selection.
- Unsupported, corrupt, oversized, or empty uploads: reject with an accessible reason and the
  applicable supported formats or limits. Do not leave a session stuck in processing.
- No signs, low confidence, unreadable text, unsupported signs, glare, darkness, blur, oblique
  angles, and partial occlusion: preserve uncertainty and avoid inventing results. Continuous
  no-result frames must not create repeated no-result speech.
- Similar adjacent signs, track loss, camera motion, and returning to a previously viewed sign:
  use the documented continuity policy; a sign returning after track expiry is a new encounter.
- Recognition faster than speech: retain relevant consolidated information, discard obsolete
  pending items, and prevent an ever-growing announcement backlog.
- Source switching, video seeking/restarting, connection failure, and stop during recognition:
  invalidate old pending results; a new sequence must not inherit stale identities or speech.
- A second user attempts recognition during an active demo: show an accessible busy message and
  do not expose or interrupt the first user's results.
- Screen-reader feedback and application speech: avoid automatically reading the same recognition
  result through both channels; keep result text available for deliberate screen-reader review.
- A supported uploaded video cannot be decoded or synchronized locally: disable its overlay,
  explain the limitation accessibly, and retain backend recognition plus text/audio review.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The prototype MUST support live browser-camera input and image/video uploads, with one
  selected input per session and an accessible indication of the active input and session state.
- **FR-002**: It MUST offer recognition for eight catalogue categories: pedestrian-crossing warnings,
  children warnings, roadworks warnings, no-pedestrian signs, street names, pedestrian directions,
  bus-stop signs, and temporary pedestrian notices. The evaluation catalogue MUST document included
  variants, examples, and exclusions; these categories do not imply coverage of every UK sign.
- **FR-003**: It MUST extract readable text from detected signs where relevant, associate text with
  the correct sign, and distinguish observed text from the recognised sign meaning. Unknown or
  unreadable content MUST NOT be replaced with invented wording.
- **FR-004**: It MUST validate and consolidate observations before speech using a documented
  acceptance policy. Low-confidence or conflicting observations MUST NOT be presented as settled
  facts. Thresholds and consolidation settings MUST be fixed and recorded for each evaluation run.
- **FR-005**: It MUST maintain separate sign identities across video observations and suppress
  unchanged automatic announcements within each track's lifetime, including reacquisition within
  a documented continuity window. Distinct physical signs MUST NOT be merged solely by their text.
- **FR-006**: It MUST make materially changed accepted information eligible for speech and ignore
  formatting-only text changes. A track-expiry policy MUST define when a returning sign becomes
  a new encounter; changes to that policy MUST be recorded in evaluation results.
- **FR-007**: It MUST announce accepted information in English, prioritise warnings/restrictions over
  directions over general information, serialize speech, and discard superseded or expired pending
  results. Queue expiry and limits MUST be documented and tested before evaluation.
- **FR-008**: Users MUST be able to start, stop, mute/unmute, and repeat the latest announcement.
  Mute cancels current application speech; repeat respects mute; unmute MUST NOT replay obsolete
  queued results. Repeat with no prior announcement MUST expose an accessible unavailable state.
- **FR-009**: All primary workflows MUST support keyboard and screen-reader operation, labelled
  controls, announced state/error changes, and optional visual results. Automatic recognition
  output MUST NOT be duplicated through simultaneous application speech and live screen-reader
  announcements. Users MUST retain access to the recognised text.
- **FR-010**: It MUST expose optional, associated sign boxes/category/text and a chronological
  session announcement history that distinguishes intentional replays from automatic speech.
  Overlays MUST refer to matching locally available frames. For uploaded video without local
  decoding or timestamp synchronization, the supported fallback MUST disable the overlay with an
  accessible explanation while preserving recognition, text review and audio feedback. No server
  preview-image transport is required; unmatched or stale overlays MUST NOT be shown.
- **FR-011**: It MUST handle the listed input, connection, and playback failures with accessible
  status and recovery actions. Supported upload formats and limits MUST be visible before upload
  and consistently enforced; they are to be fixed in technical planning.
- **FR-012**: Stop, source replacement, and video seeking/restarting MUST cancel obsolete pending
  recognition and speech. Stop MUST release camera capture. Connection loss MUST stop accepting
  frames and clear pending speech; recovery requires an explicit restart rather than silently
  resuming capture. New sessions MUST have fresh identities and history.
- **FR-013**: The demo MUST support one active recognition session on the developer-hosted system,
  including a phone browser. Additional session requests MUST receive an accessible busy status
  without access to the active user's media or results. No user accounts are required.
- **FR-014**: Camera access MUST require user permission. User media, extracted text, and announcement
  history MUST be session-only, cleared on session end, and excluded from persistent logs and
  training datasets. The prototype MUST explain that media is sent to the demo host for processing.
- **FR-015**: The project MUST train and evaluate a custom detector for the supported catalogue and
  document dataset provenance, usage permissions, labels, split membership, and model identity.
  Evaluation data MUST be held out from training and tuning; near-duplicate frames from the same
  source sequence MUST not be split between training and evaluation.
- **FR-016**: Evaluation MUST measure all outcomes below using documented ground truth, metric
  definitions, configuration, and test conditions. Separate evaluation media MUST be authorised
  for that purpose; ephemeral user sessions MUST NOT be silently retained as evaluation data.
- **FR-017**: Descriptions MUST concern observed signs and text. The prototype MUST NOT issue
  route guidance, obstacle-avoidance instructions, or decisions that a road is safe to cross.

### Key Entities *(include if feature involves data)*

- **Recognition Session**: One active user's source, lifecycle state, audio state, and temporary
  recognition/announcement history; ends on explicit stop, disconnect, or replacement. Finishing an image/video leaves results
  available for review until the session ends.
- **Sign Catalogue Entry**: Supported category, user-facing meaning, included variants, example
  annotations, text relevance, and exclusions.
- **Sign Observation**: A timestamped sign location, proposed category, confidence, and associated
  readable text from a source image or frame.
- **Tracked Sign**: A session-scoped physical-sign identity, associated observations, consolidated
  information, continuity state, and announcement history.
- **Announcement**: Spoken content, source sign, priority, creation time, relevance state, and
  whether it is automatic or a user-requested replay.
- **Evaluation Dataset**: Authorised labelled media with source grouping, sign/text/identity labels,
  condition tags, and documented training/tuning/evaluation split membership.
- **Evaluation Run**: Dataset and model versions, settings, host/device conditions, metric definitions,
  observations, aggregate results, and limitations needed to repeat the assessment.

## Success Criteria *(mandatory)*

### Measurable Outcomes

Functional acceptance and research evaluation serve different purposes. Functional scenarios must
pass; measured model accuracy and performance are reported without a predetermined minimum score.
A report is not evidence that unexecuted functional tests have passed.

- **SC-001**: All acceptance scenarios in Stories 1–4 pass on documented reference fixtures. The
  acceptance report records expected and observed outcomes separately from held-out model scores.
- **SC-002**: All primary tasks (select input, start, mute/unmute, repeat, stop, recover from an error)
  are completed without visual assistance in both keyboard-only and screen-reader tests, including
  a phone-browser screen-reader session. Record browser/device combinations and task failures.
- **SC-003**: Announcement-policy fixtures produce zero unwanted repeats for unchanged tracked signs,
  zero overlapping announcements, and zero post-stop announcements. Intentional replay is counted
  separately. End-to-end recognition sequences additionally report unwanted repeats per sign
  encounter and per minute, including track-loss cases.
- **SC-004**: Report precision, recall, mAP at overlap 0.5, and mAP averaged over overlaps 0.5–0.95
  for each supported category and overall, with sample counts and the confidence operating point.
  Report false positives on scenes without supported signs as well as missed signs.
- **SC-005**: Report character and word error rates against transcribed sign text, specifying text
  normalisation and matching rules. Include missed text regions in end-to-end scoring and report
  recognition on correctly located text separately so detection misses are not hidden.
- **SC-006**: Report identity switches, track fragmentation, and an identity-consistency score with
  its definition, alongside duplicate-announcement frequency, on annotated video sequences.
- **SC-007**: Report median and 95th-percentile capture-to-result and capture-to-speech-onset latency,
  processed frames per second, and peak memory consumption over a documented ten-minute camera
  session. Include frame counts, host/browser/device details, input resolution, connection
  conditions, and any backlog or resource growth.
- **SC-008**: Evaluate speech clarity, relevance, and timing with at least five consenting blind or
  partially sighted participants using documented tasks and a consistent 1–5 rating scale.
  Report participant counts, task completion rates, ratings, and qualitative problems without
  claiming a required satisfaction score. If recruitment is unavailable, mark this outcome
  incomplete; developer ratings MUST NOT substitute for intended-user evaluation.
- **SC-009**: Report recognition and OCR results across lighting, camera movement, viewing angle,
  and image-quality conditions, with category/condition sample counts. Missing coverage MUST be
  visible in the report rather than treated as successful robustness validation.
- **SC-010**: Lifecycle/privacy tests confirm that ending a session leaves no retained user media,
  recognised text, or announcement history, and that a second session cannot receive prior results.

## Assumptions

- This is a controlled web research prototype for one active user, not a public multi-user service.
  The developer provides a host reachable from the test phone/computer and a browser environment
  supporting camera access and audio playback. Offline operation is not promised.
- Recognition and speech initially target English. Multilingual support, navigation, crossing
  decisions, general obstacle avoidance, embedded systems, and wearable deployment are excluded.
- The eight categories bound coverage. Dataset selection and catalogue variant annotation are
  preparation tasks; a suitable labelled dataset or trained model is not assumed to exist already.
- Custom training and evaluation are in scope. Architecture and technology constraints remain
  governed by the project constitution; implementation choices belong in technical planning.
- The continuity window, confidence policy, queue limits, upload limits, and supported browser
  matrix will be fixed and documented in technical planning before implementation tests are
  defined. Research evaluation records these settings and does not silently tune on held-out data.
- Authorised research datasets are distinct from ephemeral demo input. Anonymous aggregate research
  metrics and consented study findings may be retained without storing ordinary users' camera media.
- The five-participant usability study and ten-minute performance run are explicit evaluation
  defaults. They establish reproducible evidence collection, not claims of statistical coverage
  or readiness for unsupervised outdoor navigation.
