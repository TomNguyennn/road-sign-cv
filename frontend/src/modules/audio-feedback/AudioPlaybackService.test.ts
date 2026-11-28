import { beforeEach, describe, expect, test, vi } from "vitest";
import { AudioPlaybackService } from "./AudioPlaybackService";

const ready = (overrides: Record<string, unknown> = {}) => ({
  sessionId: "session-1",
  epoch: 1,
  audioGeneration: 1,
  announcementId: "announcement-1",
  audioUrl: "/sessions/session-1/audio/announcement-1",
  validForMs: 5_000,
  isReplay: false,
  origin: "automatic",
  trackId: "track-1",
  revision: 1,
  frameId: 1,
  ...overrides,
});

function deferred<T>() {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((done) => {
    resolve = done;
  });
  return { promise, resolve };
}

function fixture(now = () => performance.now()) {
  const fetchAudio = vi.fn((url: string, signal: AbortSignal) => {
    if (!url || signal.aborted)
      return Promise.reject(new Error("fetch aborted"));
    return Promise.resolve({
      bytes: new ArrayBuffer(8),
      validForMs: 5_000,
    });
  });
  const play = vi.fn(() => Promise.resolve());
  const stop = vi.fn();
  const acknowledgeCancellation = vi.fn(() => Promise.resolve());
  const reportPlayback = vi.fn(() => Promise.resolve());
  const requestRecovery = vi.fn(() => Promise.resolve());
  const service = new AudioPlaybackService({
    fetchAudio,
    play,
    stop,
    acknowledgeCancellation,
    reportPlayback,
    requestRecovery,
    now,
  });
  service.startSession("session-1", 1, 1);
  return {
    service,
    fetchAudio,
    play,
    stop,
    acknowledgeCancellation,
    reportPlayback,
    requestRecovery,
  };
}

beforeEach(() => vi.useRealTimers());

describe("AudioPlaybackService", () => {
  test("mute and stop silence locally before network control completes", async () => {
    const f = fixture();
    await f.service.offer(ready());
    const pending = deferred<undefined>();
    f.service.mute(pending.promise);
    expect(f.stop).toHaveBeenCalledOnce();
    expect(f.service.isMuted()).toBe(true);
    await f.service.offer(ready({ announcementId: "late" }));
    expect(f.play).toHaveBeenCalledOnce();
    f.service.stopSession();
    expect(f.service.isActive()).toBe(false);
    pending.resolve(undefined);
  });

  test("later mute intent survives an earlier unmute reply", async () => {
    const f = fixture();
    const unmute = deferred<undefined>();
    f.service.unmute(unmute.promise);
    f.service.mute(Promise.resolve());
    unmute.resolve(undefined);
    await unmute.promise;
    expect(f.service.isMuted()).toBe(true);
    await f.service.offer(ready());
    expect(f.play).not.toHaveBeenCalled();
  });

  test("expired GET validity and superseded fetch never start playback", async () => {
    const f = fixture();
    f.fetchAudio.mockResolvedValueOnce({
      bytes: new ArrayBuffer(8),
      validForMs: 0,
    });
    await f.service.offer(ready());
    expect(f.play).not.toHaveBeenCalled();

    const oldFetch = deferred<{ bytes: ArrayBuffer; validForMs: number }>();
    f.fetchAudio.mockImplementationOnce(() => oldFetch.promise);
    const pending = f.service.offer(ready({ announcementId: "old" }));
    f.service.changeGeneration(2);
    oldFetch.resolve({ bytes: new ArrayBuffer(8), validForMs: 5_000 });
    await pending;
    expect(f.play).not.toHaveBeenCalled();
  });

  test("cancellation acknowledgment follows stopped playback and unlocks the slot", async () => {
    const f = fixture();
    await f.service.offer(ready());
    const ack = deferred<undefined>();
    f.acknowledgeCancellation.mockImplementationOnce(() => ack.promise);
    const cancellation = f.service.cancel({
      sessionId: "session-1",
      epoch: 0,
      cancellationId: "cancel-1",
      cancelThroughAudioGeneration: 1,
      audioGeneration: 2,
    });
    expect(f.stop).toHaveBeenCalledOnce();
    expect(f.acknowledgeCancellation).toHaveBeenCalledWith("cancel-1", 1);
    await f.service.offer(
      ready({ audioGeneration: 2, announcementId: "next" }),
    );
    expect(f.play).toHaveBeenCalledOnce();
    ack.resolve(undefined);
    await cancellation;
    await f.service.offer(
      ready({ audioGeneration: 2, announcementId: "next" }),
    );
    expect(f.play).toHaveBeenCalledTimes(2);
  });

  test("first blocked image retains latest text for explicit recovery", () => {
    const f = fixture();
    f.service.setLatestDescription({
      descriptionId: "description-1",
      text: "Crossing ahead",
      epoch: 1,
    });
    f.service.markBlocked();
    expect(f.service.canRepeat()).toBe(false);
    expect(f.service.canReadLatest()).toBe(true);
    expect(f.service.readLatestResult()).toEqual({
      action: "recover",
      descriptionId: "description-1",
    });
    expect(f.service.readLatestResult("old-description")).toBeNull();
    f.service.changeEpoch(2);
    expect(f.service.canReadLatest()).toBe(false);
  });

  test("repeated recovery clicks coalesce and manual speech has no source timing", async () => {
    const f = fixture();
    f.service.setLatestDescription({
      descriptionId: "description-1",
      text: "Crossing ahead",
      epoch: 1,
    });
    f.service.readLatestResult();
    f.service.readLatestResult();
    expect(f.requestRecovery).toHaveBeenCalledOnce();
    expect(f.requestRecovery).toHaveBeenCalledWith("description-1");
    await f.service.offer(
      ready({
        isReplay: true,
        origin: "recovery",
        trackId: null,
        revision: null,
        frameId: null,
        announcementId: "manual-1",
      }),
    );
    expect(f.play).toHaveBeenCalledOnce();
    expect(f.service.canReadLatest()).toBe(true);
  });

  test("manual offer expires instead of renewing its validity on repeated receipt", async () => {
    let clock = 0;
    const f = fixture(() => clock);
    const oldFetch = deferred<{ bytes: ArrayBuffer; validForMs: number }>();
    f.fetchAudio.mockImplementationOnce(() => oldFetch.promise);
    const pending = f.service.offer(
      ready({
        isReplay: true,
        origin: "repeat",
        trackId: null,
        revision: null,
        frameId: null,
      }),
    );
    clock = 6_000;
    const duplicate = f.service.offer(
      ready({
        isReplay: true,
        origin: "repeat",
        trackId: null,
        revision: null,
        frameId: null,
      }),
    );
    oldFetch.resolve({ bytes: new ArrayBuffer(8), validForMs: 5_000 });
    await Promise.all([pending, duplicate]);
    expect(f.play).not.toHaveBeenCalled();
  });

  test("generation change aborts fetch and failed acknowledgment keeps the barrier", async () => {
    const f = fixture();
    const oldFetch = deferred<{ bytes: ArrayBuffer; validForMs: number }>();
    f.fetchAudio.mockImplementationOnce(() => oldFetch.promise);
    const pending = f.service.offer(ready());
    const signal = f.fetchAudio.mock.calls[0]?.[1];
    f.service.changeGeneration(2);
    expect(signal?.aborted).toBe(true);
    oldFetch.resolve({ bytes: new ArrayBuffer(8), validForMs: 5_000 });
    await pending;
    f.acknowledgeCancellation.mockRejectedValueOnce(new Error("offline"));
    await expect(
      f.service.cancel({
        sessionId: "session-1",
        epoch: 1,
        cancellationId: "cancel-2",
        cancelThroughAudioGeneration: 2,
        audioGeneration: 3,
      }),
    ).rejects.toThrow("offline");
    await f.service.offer(
      ready({ announcementId: "next", audioGeneration: 3 }),
    );
    expect(f.play).not.toHaveBeenCalled();
  });

  test("late play completion cannot acknowledge speech after mute", async () => {
    const f = fixture();
    const started = deferred<undefined>();
    f.play.mockImplementationOnce(() => started.promise);
    const pending = f.service.offer(ready());
    await vi.waitFor(() => {
      expect(f.play).toHaveBeenCalledOnce();
    });
    f.service.mute(Promise.resolve());
    started.resolve(undefined);
    await pending;
    expect(f.reportPlayback).not.toHaveBeenCalledWith(
      "announcement-1",
      "started",
    );
  });
});
