// @vitest-environment jsdom
import { afterEach, beforeEach, expect, test, vi } from "vitest";
import { SessionClient, type SessionCallbacks } from "./SessionClient";

class FakeEventSource extends EventTarget {
  static current: FakeEventSource | null = null;
  onerror: ((event: Event) => void) | null = null;
  closed = false;

  constructor() {
    super();
    FakeEventSource.current = this;
  }

  close() {
    this.closed = true;
  }

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

let callbacks: SessionCallbacks;

function baseFetch(input: string, init?: RequestInit): Promise<Response> {
  if (input === "/api/v1/sessions" && init?.method === "POST")
    return Promise.resolve(Response.json(created, { status: 201 }));
  if (input.endsWith("/heartbeat"))
    return Promise.resolve(new Response(null, { status: 204 }));
  if (init?.method === "DELETE")
    return Promise.resolve(Response.json({ state: "ending" }, { status: 202 }));
  throw new Error(`Unexpected request: ${input}`);
}

const fetchMock = vi.fn(baseFetch);

beforeEach(() => {
  FakeEventSource.current = null;
  vi.stubGlobal("EventSource", FakeEventSource);
  fetchMock.mockReset().mockImplementation(baseFetch);
  vi.stubGlobal("fetch", fetchMock);
  callbacks = {
    onState: vi.fn(),
    onUpload: vi.fn(),
    onReset: vi.fn(),
    onCancelAudio: vi.fn(() => Promise.resolve()),
    onFrame: vi.fn(),
    onAudioReady: vi.fn(),
    onError: vi.fn(),
    onEnd: vi.fn(),
  };
});

afterEach(() => {
  vi.unstubAllGlobals();
});

async function startClient() {
  const client = new SessionClient(callbacks);
  const started = client.start();
  await vi.waitFor(() => {
    expect(FakeEventSource.current).not.toBeNull();
  });
  emit("session.ready", 1, 2, 1, {
    state: "awaitingInput",
    audioGeneration: 1,
  });
  await started;
  return client;
}

function emit(
  type: string,
  epoch: number,
  stateRevision: number,
  eventSeq: number,
  payload: Record<string, unknown>,
) {
  if (!FakeEventSource.current) throw new Error("Event source is unavailable");
  FakeEventSource.current.emit(type, epoch, stateRevision, eventSeq, payload);
}

test("accepts early new-epoch state and ignores older seek reply and audio", async () => {
  let reply: (value: Response) => void = () => {
    throw new Error("No seek request");
  };
  fetchMock.mockImplementation((input: string, init?: RequestInit) => {
    if (input.endsWith("/seek"))
      return new Promise<Response>((resolve) => {
        reply = resolve;
      });
    return baseFetch(input, init);
  });
  const client = await startClient();
  emit("session.state", 1, 3, 2, {
    state: "processing",
    audioGeneration: 1,
    sourceTimeMs: 0,
    durationMs: 3000,
  });
  const seek = client.seek(1000);
  await vi.waitFor(() => {
    expect(fetchMock).toHaveBeenCalledWith(
      "/api/v1/sessions/session-1/seek",
      expect.anything(),
    );
  });
  emit("session.state", 2, 5, 3, {
    state: "processing",
    audioGeneration: 2,
    sourceTimeMs: 1000,
    durationMs: 3000,
  });
  emit("audio.ready", 1, 4, 4, { audioGeneration: 1, announcementId: "old" });
  reply(Response.json({ epoch: 2, audioGeneration: 2, stateRevision: 4 }));
  await seek;
  expect(client.snapshot?.epoch).toBe(2);
  expect(client.snapshot?.stateRevision).toBe(5);
  expect(callbacks.onReset).toHaveBeenCalled();
  expect(callbacks.onAudioReady).not.toHaveBeenCalled();
  await client.stop();
});

test("awaits local cancellation before acknowledging an old-epoch barrier", async () => {
  let finishCancellation: () => void = () => {
    throw new Error("No cancellation");
  };
  callbacks.onCancelAudio = vi.fn(
    () =>
      new Promise<void>((resolve) => {
        finishCancellation = resolve;
      }),
  );
  fetchMock.mockImplementation((input: string, init?: RequestInit) => {
    if (input.includes("/cancellations/"))
      return Promise.resolve(new Response(null, { status: 204 }));
    return baseFetch(input, init);
  });
  const client = await startClient();
  emit("session.state", 2, 3, 2, {
    state: "processing",
    audioGeneration: 2,
    durationMs: 3000,
  });
  emit("audio.cancel", 1, 2, 3, {
    cancellationId: "cancel-1",
    cancelThroughAudioGeneration: 1,
  });
  expect(
    fetchMock.mock.calls.some(([url]) => url.includes("/cancellations/")),
  ).toBe(false);
  finishCancellation();
  await vi.waitFor(() => {
    expect(
      fetchMock.mock.calls.some(([url]) =>
        url.includes("/cancellations/cancel-1/ack"),
      ),
    ).toBe(true);
  });
  callbacks.onCancelAudio = vi.fn(() => Promise.resolve());
  await client.stop();
});

test("reconciles a failed upload before allowing an explicit new attempt", async () => {
  let uploads = 0;
  fetchMock.mockImplementation((input: string, init?: RequestInit) => {
    if (input.endsWith("/media")) {
      uploads += 1;
      if (uploads === 1)
        return Promise.reject(new Error("Network interrupted"));
      return Promise.resolve(
        Response.json(
          {
            epoch: 1,
            stateRevision: 5,
            uploadAttemptId: "second",
            uploadStatus: "accepted",
            uploadAttemptsUsed: 2,
            maxUploadAttempts: 3,
            canRetryUpload: false,
          },
          { status: 202 },
        ),
      );
    }
    if (input.endsWith("/upload-status"))
      return Promise.resolve(
        Response.json({
          epoch: 1,
          stateRevision: 4,
          uploadAttemptId: firstAttemptId,
          uploadStatus: "rejected",
          uploadAttemptsUsed: 1,
          maxUploadAttempts: 3,
          canRetryUpload: true,
        }),
      );
    return baseFetch(input, init);
  });
  const client = await startClient();
  const file = new File(["video"], "clip.mp4", { type: "video/mp4" });
  let firstAttemptId = "";
  const upload = client.uploadVideo(file);
  await vi.waitFor(() => {
    const mediaCall = fetchMock.mock.calls.find(([url]) =>
      url.endsWith("/media"),
    );
    expect(mediaCall).toBeDefined();
    firstAttemptId =
      ((mediaCall?.[1] as RequestInit).headers &&
        ((mediaCall?.[1] as RequestInit).headers as Record<string, string>)[
          "X-Upload-Attempt-Id"
        ]) ||
      "";
  });
  await expect(upload).rejects.toThrow("Network interrupted");
  expect(client.uploadSnapshot?.canRetryUpload).toBe(true);
  await client.uploadVideo(file);
  expect(uploads).toBe(2);
  await client.stop();
});
