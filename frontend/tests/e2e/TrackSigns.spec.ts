import { expect, test, type Page } from "@playwright/test";

test.skip(
  process.env.VIDEO_COMPONENT_E2E !== "1",
  "Run with playwright.video.config.ts",
);

declare global {
  interface Window {
    mockEvents: {
      connected(): boolean;
      emit(
        type: string,
        epoch: number,
        revision: number,
        sequence: number,
        payload: Record<string, unknown>,
      ): void;
    };
    videoHarness: {
      events: string[];
      cancelCutoffs: number[];
      audioReadyCount: number;
      audioFetchCompleted: boolean;
    };
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

async function installEvents(page: Page): Promise<void> {
  await page.addInitScript(() => {
    const state: { source: EventTarget | null } = { source: null };
    class MockEventSource extends EventTarget {
      onerror: ((event: Event) => void) | null = null;
      constructor() {
        super();
        state.source = this;
      }
      close() {
        state.source = null;
      }
    }
    Object.defineProperty(window, "EventSource", { value: MockEventSource });
    window.mockEvents = {
      connected() {
        return state.source !== null;
      },
      emit(type, epoch, revision, sequence, payload) {
        state.source?.dispatchEvent(
          new MessageEvent(type, {
            data: JSON.stringify({
              sessionId: "session-1",
              epoch,
              stateRevision: revision,
              eventSeq: sequence,
              type,
              payload,
            }),
          }),
        );
      },
    };
  });
}

async function emit(
  page: Page,
  type: string,
  epoch: number,
  revision: number,
  sequence: number,
  payload: Record<string, unknown>,
): Promise<void> {
  await page.evaluate(
    ({ type, epoch, revision, sequence, payload }) => {
      window.mockEvents.emit(type, epoch, revision, sequence, payload);
    },
    { type, epoch, revision, sequence, payload },
  );
}

async function beginVideo(page: Page): Promise<void> {
  await page.goto("/tests/e2e/video-harness.html");
  await page.getByRole("button", { name: "Start video session" }).click();
  await expect
    .poll(() => page.evaluate(() => window.mockEvents.connected()))
    .toBe(true);
  await emit(page, "session.ready", 1, 2, 1, {
    state: "awaitingInput",
    audioGeneration: 1,
  });
  await expect(page.getByLabel("Video file")).toBeEnabled();
  await page.getByLabel("Video file").setInputFiles({
    name: "clip.mp4",
    mimeType: "video/mp4",
    buffer: Buffer.from("fixture"),
  });
  await expect(page.getByText("Upload attempts: 1 of 3")).toBeVisible();
}

test("video upload reaches timestamped EOF review with a labelled seek", async ({
  page,
}) => {
  await installEvents(page);
  await page.route("**/api/v1/**", async (route) => {
    const path = new URL(route.request().url()).pathname;
    if (path === "/api/v1/sessions" && route.request().method() === "POST") {
      await route.fulfill({ status: 201, json: created });
      return;
    }
    if (path.endsWith("/media")) {
      await route.fulfill({
        status: 202,
        json: {
          epoch: 1,
          stateRevision: 3,
          uploadAttemptId: "accepted",
          uploadStatus: "accepted",
          uploadAttemptsUsed: 1,
          maxUploadAttempts: 3,
          canRetryUpload: false,
        },
      });
      return;
    }
    await route.fulfill({ status: 204, body: "" });
  });
  await beginVideo(page);
  await emit(page, "session.state", 1, 4, 2, {
    state: "processing",
    audioGeneration: 1,
    sourceTimeMs: 1200,
    durationMs: 3000,
  });
  await expect(page.getByText("00:01 / 00:03")).toBeVisible();
  await emit(page, "session.state", 1, 5, 3, {
    state: "reviewing",
    audioGeneration: 1,
    sourceTimeMs: 3000,
    durationMs: 3000,
  });
  await expect(
    page.getByText("Video complete. Results are ready to review."),
  ).toBeVisible();
  await expect(
    page.getByRole("button", { name: "Seek to selected position" }),
  ).toBeEnabled();
});

test("seek fences an in-flight audio fetch and ignores old-epoch results", async ({
  page,
}) => {
  await installEvents(page);
  let releaseSeek: () => void = () => {
    throw new Error("Seek not requested");
  };
  const seekGate = new Promise<void>((resolve) => {
    releaseSeek = resolve;
  });
  let isSeekRequested = false;
  let isAudioFetchStarted = false;
  let isCancelAcknowledged = false;
  await page.route("**/api/v1/**", async (route) => {
    const path = new URL(route.request().url()).pathname;
    if (path === "/api/v1/sessions" && route.request().method() === "POST") {
      await route.fulfill({ status: 201, json: created });
      return;
    }
    if (path.endsWith("/media")) {
      await route.fulfill({
        status: 202,
        json: {
          epoch: 1,
          stateRevision: 3,
          uploadAttemptId: "accepted",
          uploadStatus: "accepted",
          uploadAttemptsUsed: 1,
          maxUploadAttempts: 3,
          canRetryUpload: false,
        },
      });
      return;
    }
    if (path.endsWith("/audio/old")) {
      isAudioFetchStarted = true;
      await new Promise((resolve) => setTimeout(resolve, 500));
      await route
        .fulfill({ status: 200, body: "audio" })
        .catch(() => undefined);
      return;
    }
    if (path.endsWith("/seek")) {
      isSeekRequested = true;
      await seekGate;
      await route.fulfill({
        status: 200,
        json: {
          epoch: 2,
          audioGeneration: 2,
          stateRevision: 5,
        },
      });
      return;
    }
    if (path.endsWith("/cancellations/cancel-1/ack")) {
      isCancelAcknowledged = true;
      await route.fulfill({ status: 204, body: "" });
      return;
    }
    await route.fulfill({ status: 204, body: "" });
  });
  await beginVideo(page);
  await emit(page, "session.state", 1, 4, 2, {
    state: "processing",
    audioGeneration: 1,
    sourceTimeMs: 0,
    durationMs: 3000,
  });
  await emit(page, "audio.ready", 1, 4, 3, {
    audioGeneration: 1,
    audioUrl: "/api/v1/sessions/session-1/audio/old",
  });
  await expect.poll(() => isAudioFetchStarted).toBe(true);
  await page.getByLabel("Seek position (seconds)").focus();
  await page.getByLabel("Seek position (seconds)").press("ArrowRight");
  await page.getByRole("button", { name: "Seek to selected position" }).click();
  await expect.poll(() => isSeekRequested).toBe(true);
  await emit(page, "session.state", 2, 6, 4, {
    state: "processing",
    audioGeneration: 2,
    sourceTimeMs: 1000,
    durationMs: 3000,
  });
  await emit(page, "audio.cancel", 1, 5, 5, {
    cancellationId: "cancel-1",
    cancelThroughAudioGeneration: 1,
  });
  await emit(page, "frame.result", 1, 5, 6, { frameId: 10 });
  await emit(page, "audio.ready", 1, 5, 7, {
    audioGeneration: 1,
    audioUrl: "/api/v1/sessions/session-1/audio/old",
  });
  releaseSeek();
  await expect.poll(() => isCancelAcknowledged).toBe(true);
  await expect
    .poll(() =>
      page.evaluate(() => window.videoHarness.cancelCutoffs.includes(1)),
    )
    .toBe(true);
  expect(await page.evaluate(() => window.videoHarness.audioReadyCount)).toBe(
    1,
  );
  expect(
    await page.evaluate(() => window.videoHarness.audioFetchCompleted),
  ).toBe(false);
  expect(
    await page.evaluate(() => window.videoHarness.events.includes("frame")),
  ).toBe(false);
  await expect(page.getByText("00:01 / 00:03")).toBeVisible();
});
