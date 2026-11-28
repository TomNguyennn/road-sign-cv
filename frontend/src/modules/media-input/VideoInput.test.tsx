// @vitest-environment jsdom
import {
  cleanup,
  fireEvent,
  render,
  screen,
  waitFor,
} from "@testing-library/react";
import { afterEach, beforeEach, expect, test, vi } from "vitest";
import { VideoInput } from "./VideoInput";

class FakeEventSource extends EventTarget {
  static current: FakeEventSource | null = null;
  onerror: ((event: Event) => void) | null = null;

  constructor() {
    super();
    FakeEventSource.current = this;
  }

  close() {}

  emit(
    type: string,
    epoch: number,
    stateRevision: number,
    eventSeq: number,
    payload: Record<string, unknown>,
  ) {
    this.dispatchEvent(
      new MessageEvent(type, {
        data: JSON.stringify({
          sessionId: "session-1",
          epoch,
          stateRevision,
          eventSeq,
          type,
          payload,
        }),
      }),
    );
  }
}

const created = {
  sessionId: "session-1",
  csrfToken: "csrf",
  epoch: 1,
  audioGeneration: 1,
  stateRevision: 1,
  eventsUrl: "/api/v1/sessions/session-1/events",
};

let fetchMock: ReturnType<typeof vi.fn<typeof fetch>>;

function requestUrl(input: RequestInfo | URL): string {
  if (typeof input === "string") return input;
  if (input instanceof URL) return input.href;
  return input.url;
}

beforeEach(() => {
  FakeEventSource.current = null;
  vi.stubGlobal("EventSource", FakeEventSource);
  fetchMock = vi.fn<typeof fetch>((input, init) => {
    const url = requestUrl(input);
    if (url === "/api/v1/sessions" && init?.method === "POST")
      return Promise.resolve(Response.json(created, { status: 201 }));
    if (url.endsWith("/media"))
      return Promise.resolve(
        Response.json(
          {
            epoch: 1,
            stateRevision: 3,
            uploadAttemptId: "accepted",
            uploadStatus: "accepted",
            uploadAttemptsUsed: 1,
            maxUploadAttempts: 3,
            canRetryUpload: false,
          },
          { status: 202 },
        ),
      );
    if (url.endsWith("/seek"))
      return Promise.resolve(
        Response.json({
          epoch: 2,
          audioGeneration: 2,
          stateRevision: 5,
        }),
      );
    if (url.endsWith("/heartbeat"))
      return Promise.resolve(new Response(null, { status: 204 }));
    if (init?.method === "DELETE")
      return Promise.resolve(Response.json({ state: "ending" }));
    throw new Error(`Unexpected request: ${url}`);
  });
  vi.stubGlobal("fetch", fetchMock);
});

afterEach(() => {
  cleanup();
  vi.unstubAllGlobals();
});

function emit(
  type: string,
  epoch: number,
  revision: number,
  sequence: number,
  payload: Record<string, unknown>,
) {
  if (!FakeEventSource.current) throw new Error("Event source is unavailable");
  FakeEventSource.current.emit(type, epoch, revision, sequence, payload);
}

test("labels video controls, reports progress, and clears old results before seeking", async () => {
  const onReset = vi.fn();
  const onCancelAudio = vi.fn(() => Promise.resolve());
  render(
    <VideoInput
      onAudioReady={() => undefined}
      onCancelAudio={onCancelAudio}
      onFrame={() => undefined}
      onReset={onReset}
    />,
  );
  expect(screen.getByLabelText("Video file")).toHaveProperty("disabled", true);
  fireEvent.click(screen.getByRole("button", { name: "Start video session" }));
  await waitFor(() => {
    expect(FakeEventSource.current).not.toBeNull();
  });
  emit("session.ready", 1, 2, 1, {
    state: "awaitingInput",
    audioGeneration: 1,
  });
  await waitFor(() => {
    expect(screen.getByLabelText("Video file")).toHaveProperty(
      "disabled",
      false,
    );
  });
  const fileInput = screen.getByLabelText("Video file");
  fireEvent.change(fileInput, {
    target: { files: [new File(["clip"], "clip.mp4", { type: "video/mp4" })] },
  });
  await waitFor(() => {
    expect(screen.getByText("Upload attempts: 1 of 3")).toBeTruthy();
  });
  emit("session.state", 1, 4, 2, {
    state: "processing",
    audioGeneration: 1,
    sourceTimeMs: 1000,
    durationMs: 3000,
  });
  await waitFor(() => {
    expect(screen.getByLabelText("Video progress")).toHaveProperty(
      "value",
      1000,
    );
  });
  fireEvent.change(screen.getByLabelText("Seek position (seconds)"), {
    target: { value: "2" },
  });
  fireEvent.click(
    screen.getByRole("button", { name: "Seek to selected position" }),
  );
  await waitFor(() => {
    expect(onReset).toHaveBeenCalled();
  });
  expect(onCancelAudio).toHaveBeenCalledWith(1);
  await waitFor(() => {
    expect(
      fetchMock.mock.calls.some(([url]) => requestUrl(url).endsWith("/seek")),
    ).toBe(true);
  });
  fireEvent.click(screen.getByRole("button", { name: "Stop video session" }));
  await screen.findByText("Session stopped.");
});

test("rejects unsupported files before transfer", async () => {
  render(
    <VideoInput
      onAudioReady={() => undefined}
      onCancelAudio={() => Promise.resolve()}
      onFrame={() => undefined}
      onReset={() => undefined}
    />,
  );
  fireEvent.click(screen.getByRole("button", { name: "Start video session" }));
  await waitFor(() => {
    expect(FakeEventSource.current).not.toBeNull();
  });
  emit("session.ready", 1, 2, 1, {
    state: "awaitingInput",
    audioGeneration: 1,
  });
  await waitFor(() => {
    expect(screen.getByLabelText("Video file")).toHaveProperty(
      "disabled",
      false,
    );
  });
  fireEvent.change(screen.getByLabelText("Video file"), {
    target: { files: [new File(["text"], "note.txt", { type: "text/plain" })] },
  });
  expect(screen.getByRole("status").textContent).toContain(
    "Choose an MP4 or WebM file",
  );
  expect(
    fetchMock.mock.calls.some(([url]) => requestUrl(url).endsWith("/media")),
  ).toBe(false);
  fireEvent.click(screen.getByRole("button", { name: "Stop video session" }));
  await screen.findByText("Session stopped.");
});
