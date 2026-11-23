# Accessible Browser Interaction Contract

## Page and controls

One recognition page contains a heading, an explanation that media is processed by the demo host
without persistent session storage, source selection, controls, status, and optional results.
No login, onboarding wizard, automatic camera access, or hidden keyboard shortcuts.

Use native labelled controls in this order: input mode; file picker when applicable; Start;
Stop; Mute/Unmute; Repeat latest; Enable audio when activation failed; video seek when applicable;
Show visual results. All must expose accessible name, role, enabled state, keyboard operation,
logical focus order, and visible focus. Disabled controls have an adjacent explanation when their
reason is not obvious. File constraints are available before choosing a file.

Start requests camera permission only for camera mode and activates AudioContext from the user
interaction. Failure of either step must not leave invisible capture running. Default camera
facing preference is environment (not a hard device constraint); no microphone permission is
requested. Preview video is muted and inline. Pause capture on hidden page by ending the session;
returning to the page requires explicit Start and an accessible explanation.

A single audio source plays server-generated WAV. The browser does not synthesize speech. An
Enable audio control recovers from browser restrictions without silently replaying a stale queue.
Repeat plays only the latest started description when unmuted. Stop, mute, and disconnect take
local effect immediately, irrespective of HTTP completion.

## Status versus recognition content

Use one polite status region for source changes, busy, camera/connection state and no-result
image completion. Recoverable errors include a next action. Do not move focus automatically on
every result. Processing status changes are coalesced rather than announcing each frame.

Recognition transcript/history and optional visual results are non-live. They remain navigable
by a screen reader, but do not duplicate generated speech automatically. Muting application audio
does not silence screen-reader status. The browser cannot control another assistive tool's voice;
manual reading may overlap, and the app must not claim to prevent external audio overlap.

When visual results are hidden, accessible controls, status and review text remain available.
Camera overlays use the matched sampled frame; uploaded video overlays align to source timestamp
or display the corresponding sampled frame when synchronisation is unavailable. Do not silently
paint old boxes on unrelated live pixels. History shows last 200 entries and a truncation message;
it is cleared with session end.

## Acceptance matrix

Automated Playwright tests cover Chromium/Firefox/WebKit engines with deterministic media fixtures,
keyboard flow, focus and axe checks. They do not substitute for real device/camera/audio tests.
Manual release checks cover NVDA with desktop Chrome/Firefox, VoiceOver with macOS Safari and
actual iPhone Safari, and TalkBack with Android Chrome. Record exact OS/browser/assistive versions.
Check trusted HTTPS, grant/deny/revoke permission, activation rejection, all controls without
visual results, seek while audio is pending, loss of connection and competing session rejection.

The five-participant study uses authorised staged scenes and records task completion plus 1–5
clarity, relevance and timing ratings. Do not represent automated accessibility scans or developer
ratings as evidence from blind/partially sighted participants.
