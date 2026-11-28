export interface AudioReady {
  sessionId: string;
  epoch: number;
  audioGeneration: number;
  announcementId: string;
  audioUrl: string;
  validForMs: number;
  isReplay: boolean;
  origin: string;
  trackId: string | null;
  revision: number | null;
  frameId: number | null;
}

export interface Cancellation {
  sessionId: string;
  epoch: number;
  cancellationId: string;
  cancelThroughAudioGeneration: number;
  audioGeneration: number;
}

interface LatestDescription {
  descriptionId: string;
  text: string;
  epoch: number;
}

export interface AudioPlaybackDependencies {
  fetchAudio: (
    url: string,
    signal: AbortSignal,
  ) => Promise<{ bytes: ArrayBuffer; validForMs: number }>;
  play: (bytes: ArrayBuffer) => Promise<void>;
  stop: () => void;
  acknowledgeCancellation: (id: string, cutoff: number) => Promise<void>;
  reportPlayback: (
    id: string,
    status: "started" | "ended" | "failed" | "blocked",
  ) => Promise<void>;
  requestRecovery: (descriptionId: string) => Promise<void>;
  now: () => number;
}

export class AudioPlaybackService {
  private sessionId: string | null = null;
  private epoch = 0;
  private generation = 0;
  private muted = false;
  private blocked = false;
  private barrier = false;
  private serial = 0;
  private pendingAnnouncement: string | null = null;
  private fetchAbort: AbortController | null = null;
  private activeAnnouncement: string | null = null;
  private latest: LatestDescription | null = null;
  private latestStarted: string | null = null;
  private pendingRecovery: string | null = null;

  constructor(private readonly dependencies: AudioPlaybackDependencies) {}

  startSession(sessionId: string, epoch: number, generation: number): void {
    this.stopSession();
    this.sessionId = sessionId;
    this.epoch = epoch;
    this.generation = generation;
    this.muted = false;
  }

  isActive(): boolean {
    return this.sessionId !== null;
  }
  isMuted(): boolean {
    return this.muted;
  }
  canRepeat(): boolean {
    return !!this.latestStarted && !this.muted;
  }
  canReadLatest(): boolean {
    return !!this.latest && !this.muted;
  }

  setLatestDescription(description: LatestDescription): void {
    if (description.epoch === this.epoch) this.latest = description;
  }

  readLatestResult(
    descriptionId?: string,
  ): { action: "recover"; descriptionId: string } | null {
    if (
      !this.canReadLatest() ||
      !this.latest ||
      (descriptionId && descriptionId !== this.latest.descriptionId)
    )
      return null;
    const id = this.latest.descriptionId;
    if (this.pendingRecovery !== id) {
      this.pendingRecovery = id;
      this.blocked = false;
      void this.dependencies
        .requestRecovery(id)
        .catch(() => undefined)
        .finally(() => {
          if (this.pendingRecovery === id) this.pendingRecovery = null;
        });
    }
    return { action: "recover", descriptionId: id };
  }

  markBlocked(): void {
    this.stopLocal();
    this.blocked = true;
  }

  mute(controlReply: Promise<void>): void {
    void controlReply.catch(() => undefined);
    this.muted = true;
    this.stopLocal();
  }

  unmute(controlReply: Promise<void>): void {
    void controlReply.catch(() => undefined);
    this.muted = false;
  }

  stopSession(): void {
    this.stopLocal();
    this.sessionId = null;
    this.latest = null;
    this.latestStarted = null;
    this.pendingRecovery = null;
    this.blocked = false;
    this.barrier = false;
  }

  changeEpoch(epoch: number): void {
    if (epoch === this.epoch) return;
    this.stopLocal();
    this.epoch = epoch;
    this.latest = null;
    this.latestStarted = null;
    this.pendingRecovery = null;
  }

  changeGeneration(generation: number): void {
    if (generation <= this.generation) return;
    this.stopLocal();
    this.generation = generation;
  }

  private stopLocal(): void {
    ++this.serial;
    this.fetchAbort?.abort();
    this.fetchAbort = null;
    if (this.activeAnnouncement) this.dependencies.stop();
    this.activeAnnouncement = null;
    this.pendingAnnouncement = null;
  }

  async offer(ready: AudioReady): Promise<void> {
    if (
      !["automatic", "repeat", "recovery"].includes(ready.origin) ||
      (ready.origin === "automatic" &&
        (ready.isReplay ||
          ready.trackId === null ||
          ready.revision === null ||
          ready.frameId === null)) ||
      (ready.origin !== "automatic" &&
        (!ready.isReplay ||
          ready.trackId !== null ||
          ready.revision !== null ||
          ready.frameId !== null))
    )
      return;
    if (
      !this.sessionId ||
      ready.sessionId !== this.sessionId ||
      ready.epoch !== this.epoch ||
      ready.audioGeneration !== this.generation ||
      this.muted ||
      this.blocked ||
      this.barrier ||
      this.activeAnnouncement ||
      this.pendingAnnouncement ||
      !Number.isFinite(ready.validForMs) ||
      ready.validForMs <= 0
    )
      return;
    const serial = ++this.serial;
    this.pendingAnnouncement = ready.announcementId;
    const startedAt = this.dependencies.now();
    const abort = new AbortController();
    this.fetchAbort = abort;
    let audio: { bytes: ArrayBuffer; validForMs: number };
    try {
      audio = await this.dependencies.fetchAudio(ready.audioUrl, abort.signal);
    } catch {
      if (serial === this.serial)
        await this.dependencies.reportPlayback(ready.announcementId, "failed");
      return;
    } finally {
      if (this.fetchAbort === abort) this.fetchAbort = null;
    }
    if (this.pendingAnnouncement === ready.announcementId)
      this.pendingAnnouncement = null;
    const { bytes, validForMs } = audio;
    if (
      serial !== this.serial ||
      ready.epoch !== this.epoch ||
      ready.audioGeneration !== this.generation ||
      !Number.isFinite(validForMs) ||
      validForMs - (this.dependencies.now() - startedAt) <= 0
    )
      return;
    this.activeAnnouncement = ready.announcementId;
    try {
      await this.dependencies.play(bytes);
      if (serial !== this.serial) return;
      this.latestStarted = ready.announcementId;
      await this.dependencies.reportPlayback(ready.announcementId, "started");
    } catch {
      if (serial !== this.serial) return;
      this.blocked = true;
      this.activeAnnouncement = null;
      await this.dependencies.reportPlayback(ready.announcementId, "blocked");
    }
  }

  async cancel(receipt: Cancellation): Promise<void> {
    if (receipt.sessionId !== this.sessionId) return;
    this.barrier = true;
    if (receipt.cancelThroughAudioGeneration >= this.generation)
      this.stopLocal();
    this.generation = Math.max(this.generation, receipt.audioGeneration);
    await this.dependencies.acknowledgeCancellation(
      receipt.cancellationId,
      receipt.cancelThroughAudioGeneration,
    );
    this.barrier = false;
  }

  async ended(announcementId: string): Promise<void> {
    if (announcementId !== this.activeAnnouncement) return;
    this.activeAnnouncement = null;
    await this.dependencies.reportPlayback(announcementId, "ended");
  }
}
