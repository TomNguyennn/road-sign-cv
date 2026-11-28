export interface SessionState {
  epoch: number;
  audioGeneration: number;
  stateRevision: number;
  state: string;
  isMuted?: boolean;
  isAudioBlocked?: boolean;
  sourceTimeMs?: number;
  durationMs?: number;
}

export interface UploadState {
  stateRevision: number;
  uploadAttemptId: string | null;
  uploadStatus: string;
  uploadAttemptsUsed: number;
  maxUploadAttempts: number;
  canRetryUpload: boolean;
}

export interface SessionCallbacks {
  onState: (state: SessionState) => void;
  onUpload: (state: UploadState) => void;
  onReset: () => void;
  onCancelAudio: (cutoff: number) => Promise<void>;
  onFrame: (payload: unknown) => void;
  onAudioReady: (payload: unknown) => void;
  onDescription?: (description: {
    descriptionId: string;
    text: string;
  }) => void;
  onError: (message: string) => void;
  onEnd: () => void;
}

interface SessionCreated {
  sessionId: string;
  csrfToken: string;
  epoch: number;
  audioGeneration: number;
  stateRevision: number;
  eventsUrl: string;
}

interface EventEnvelope {
  sessionId: string;
  epoch: number;
  stateRevision: number;
  eventSeq: number;
  type: string;
  payload: Record<string, unknown>;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function createdSession(value: unknown): SessionCreated {
  if (
    !isRecord(value) ||
    typeof value.sessionId !== "string" ||
    typeof value.csrfToken !== "string" ||
    typeof value.epoch !== "number" ||
    typeof value.audioGeneration !== "number" ||
    typeof value.stateRevision !== "number" ||
    typeof value.eventsUrl !== "string" ||
    value.eventsUrl !== `/api/v1/sessions/${value.sessionId}/events`
  ) {
    throw new Error("Invalid session response");
  }
  return {
    sessionId: value.sessionId,
    csrfToken: value.csrfToken,
    epoch: value.epoch,
    audioGeneration: value.audioGeneration,
    stateRevision: value.stateRevision,
    eventsUrl: value.eventsUrl,
  };
}

function eventEnvelope(value: unknown): EventEnvelope | null {
  if (
    !isRecord(value) ||
    !isRecord(value.payload) ||
    typeof value.sessionId !== "string" ||
    typeof value.epoch !== "number" ||
    typeof value.stateRevision !== "number" ||
    typeof value.eventSeq !== "number" ||
    typeof value.type !== "string"
  )
    return null;
  return {
    sessionId: value.sessionId,
    epoch: value.epoch,
    stateRevision: value.stateRevision,
    eventSeq: value.eventSeq,
    type: value.type,
    payload: value.payload,
  };
}

export class SessionClient {
  private session: SessionCreated | null = null;
  private state: SessionState | null = null;
  private upload: UploadState | null = null;
  private events: EventSource | null = null;
  private heartbeatTimer: ReturnType<typeof setInterval> | null = null;
  private lastEventSeq = 0;
  private controlQueue: Promise<void> = Promise.resolve();
  private uncertainAttemptId: string | null = null;
  private latestDescription: {
    descriptionId: string;
    text: string;
    epoch: number;
  } | null = null;
  private pendingMuteIntent: boolean | null = null;
  private audioIntent = 0;
  private uploadAbort: AbortController | null = null;
  private readonly onVisibilityChange = () => {
    if (document.visibilityState === "hidden")
      void this.stop().catch(() => undefined);
  };

  constructor(private readonly callbacks: SessionCallbacks) {}

  get snapshot(): SessionState | null {
    return this.state;
  }
  get uploadSnapshot(): UploadState | null {
    return this.upload;
  }
  get isUploadUncertain(): boolean {
    return this.uncertainAttemptId !== null;
  }
  get latestAcceptedDescription(): {
    descriptionId: string;
    text: string;
  } | null {
    return this.latestDescription;
  }

  async start(): Promise<void> {
    if (this.session) throw new Error("A session is already active");
    const response = await fetch("/api/v1/sessions", {
      method: "POST",
      credentials: "same-origin",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ sourceKind: "video" }),
    });
    if (!response.ok) throw new Error("Video session is unavailable");
    const body: unknown = await response.json();
    this.session = createdSession(body);
    this.state = {
      epoch: this.session.epoch,
      audioGeneration: this.session.audioGeneration,
      stateRevision: this.session.stateRevision,
      state: "created",
    };
    try {
      await this.attachEvents();
    } catch (error) {
      await this.stop();
      throw error;
    }
    this.heartbeatTimer = setInterval(() => {
      void this.heartbeat();
    }, 5000);
    document.addEventListener("visibilitychange", this.onVisibilityChange);
  }

  private attachEvents(): Promise<void> {
    const session = this.session;
    if (!session) throw new Error("Session is unavailable");
    return new Promise((resolve, reject) => {
      const events = new EventSource(session.eventsUrl, {
        withCredentials: true,
      });
      this.events = events;
      events.addEventListener(
        "session.ready",
        (event) => {
          this.handleEvent(event);
          resolve();
        },
        { once: true },
      );
      for (const type of [
        "session.state",
        "upload.state",
        "frame.result",
        "audio.ready",
        "audio.cancel",
        "description.latest",
        "session.ended",
      ]) {
        events.addEventListener(type, (event) => {
          this.handleEvent(event);
        });
      }
      events.onerror = () => {
        events.close();
        this.events = null;
        void this.stop()
          .catch(() => undefined)
          .finally(() => {
            this.callbacks.onError("Connection lost. Start a new session.");
          });
        reject(new Error("Event stream disconnected"));
      };
    });
  }

  private handleEvent(event: Event): void {
    if (
      !(event instanceof MessageEvent) ||
      !this.session ||
      typeof event.data !== "string"
    )
      return;
    let value: unknown;
    try {
      value = JSON.parse(event.data) as unknown;
    } catch {
      return;
    }
    const envelope = eventEnvelope(value);
    if (
      !envelope ||
      envelope.sessionId !== this.session.sessionId ||
      envelope.eventSeq <= this.lastEventSeq
    )
      return;
    this.lastEventSeq = envelope.eventSeq;
    if (envelope.type === "audio.cancel") {
      void this.acknowledgeCancellation(envelope.payload);
      return;
    }
    if (
      envelope.type === "session.ready" ||
      envelope.type === "session.state"
    ) {
      this.applyState(envelope);
      return;
    }
    if (
      !this.state ||
      envelope.epoch !== this.state.epoch ||
      envelope.stateRevision < this.state.stateRevision
    )
      return;
    if (envelope.type === "upload.state")
      this.applyUpload(envelope.payload, envelope.stateRevision);
    if (
      envelope.type === "description.latest" &&
      typeof envelope.payload.descriptionId === "string" &&
      typeof envelope.payload.text === "string"
    ) {
      this.latestDescription = {
        descriptionId: envelope.payload.descriptionId,
        text: envelope.payload.text,
        epoch: envelope.epoch,
      };
      this.callbacks.onDescription?.(this.latestDescription);
    }
    if (envelope.type === "frame.result")
      this.callbacks.onFrame(envelope.payload);
    if (
      envelope.type === "audio.ready" &&
      envelope.payload.audioGeneration === this.state.audioGeneration
    )
      this.callbacks.onAudioReady(envelope.payload);
    if (envelope.type === "session.ended") {
      void this.stop()
        .catch(() => undefined)
        .finally(() => {
          this.callbacks.onError("Session ended. Start a new session.");
        });
    }
  }

  private applyState(envelope: EventEnvelope): void {
    if (
      !this.state ||
      envelope.epoch < this.state.epoch ||
      envelope.stateRevision < this.state.stateRevision
    )
      return;
    const payload = envelope.payload;
    if (
      typeof payload.state !== "string" ||
      typeof payload.audioGeneration !== "number"
    )
      return;
    if (envelope.epoch > this.state.epoch) {
      void this.callbacks
        .onCancelAudio(this.state.audioGeneration)
        .catch(() => {
          this.callbacks.onError("Audio cancellation could not be confirmed.");
        });
      this.callbacks.onReset();
      this.latestDescription = null;
    }
    this.state = {
      epoch: envelope.epoch,
      stateRevision: envelope.stateRevision,
      audioGeneration: payload.audioGeneration,
      state: payload.state,
      isMuted: this.pendingMuteIntent ?? payload.isMuted === true,
      isAudioBlocked: payload.isAudioBlocked === true,
      ...(typeof payload.sourceTimeMs === "number"
        ? { sourceTimeMs: payload.sourceTimeMs }
        : {}),
      ...(typeof payload.durationMs === "number"
        ? { durationMs: payload.durationMs }
        : {}),
    };
    this.callbacks.onState(this.state);
  }

  private applyUpload(
    payload: Record<string, unknown>,
    revision: number,
  ): void {
    if (this.upload && revision < this.upload.stateRevision) return;
    if (
      typeof payload.uploadStatus !== "string" ||
      typeof payload.uploadAttemptsUsed !== "number" ||
      typeof payload.maxUploadAttempts !== "number" ||
      typeof payload.canRetryUpload !== "boolean" ||
      !(
        typeof payload.uploadAttemptId === "string" ||
        payload.uploadAttemptId === null
      )
    )
      return;
    this.upload = {
      stateRevision: revision,
      uploadAttemptId: payload.uploadAttemptId,
      uploadStatus: payload.uploadStatus,
      uploadAttemptsUsed: payload.uploadAttemptsUsed,
      maxUploadAttempts: payload.maxUploadAttempts,
      canRetryUpload: payload.canRetryUpload,
    };
    if (
      this.uncertainAttemptId === this.upload.uploadAttemptId &&
      ["accepted", "rejected", "aborted"].includes(this.upload.uploadStatus)
    ) {
      this.uncertainAttemptId = null;
    }
    this.callbacks.onUpload(this.upload);
  }

  private async acknowledgeCancellation(
    payload: Record<string, unknown>,
  ): Promise<void> {
    const session = this.session;
    if (
      !session ||
      typeof payload.cancellationId !== "string" ||
      typeof payload.cancelThroughAudioGeneration !== "number"
    )
      return;
    const cutoff = payload.cancelThroughAudioGeneration;
    try {
      await this.callbacks.onCancelAudio(cutoff);
      const response = await fetch(
        `/api/v1/sessions/${session.sessionId}/cancellations/${payload.cancellationId}/ack`,
        {
          method: "POST",
          credentials: "same-origin",
          headers: {
            "Content-Type": "application/json",
            "X-CSRF-Token": session.csrfToken,
          },
          body: JSON.stringify({ cancelThroughAudioGeneration: cutoff }),
        },
      );
      if (!response.ok) throw new Error("Cancellation acknowledgment failed");
    } catch {
      this.callbacks.onError("Audio cancellation could not be confirmed.");
    }
  }

  async uploadVideo(file: File): Promise<void> {
    const session = this.session;
    if (
      !session ||
      !this.state ||
      this.state.state !== "awaitingInput" ||
      this.uncertainAttemptId ||
      this.upload?.canRetryUpload === false
    )
      throw new Error("Video upload is unavailable");
    const attemptId = crypto.randomUUID();
    this.uncertainAttemptId = attemptId;
    const body = new FormData();
    body.set("file", file);
    const abort = new AbortController();
    this.uploadAbort = abort;
    try {
      const response = await fetch(
        `/api/v1/sessions/${session.sessionId}/media`,
        {
          method: "POST",
          credentials: "same-origin",
          body,
          signal: abort.signal,
          headers: {
            "X-CSRF-Token": session.csrfToken,
            "X-Epoch": String(this.state.epoch),
            "X-Upload-Attempt-Id": attemptId,
          },
        },
      );
      const value: unknown = await response.json();
      if (this.session !== session) return;
      if (isRecord(value) && typeof value.stateRevision === "number")
        this.applyUpload(value, value.stateRevision);
      if (!response.ok)
        throw new Error(
          "Video was rejected. Select another file and try again.",
        );
    } catch (error) {
      if (this.session !== session) return;
      await this.reconcileUpload(attemptId);
      if (
        this.upload?.uploadAttemptId === attemptId &&
        this.upload.uploadStatus === "accepted"
      )
        return;
      if (this.uncertainAttemptId === attemptId)
        throw new Error(
          "Upload outcome is uncertain. End this session before trying again.",
          { cause: error },
        );
      throw error;
    } finally {
      if (this.uploadAbort === abort) this.uploadAbort = null;
    }
  }

  private async reconcileUpload(attemptId: string): Promise<void> {
    if (!this.session) return;
    try {
      const response = await fetch(
        `/api/v1/sessions/${this.session.sessionId}/upload-status`,
        {
          credentials: "same-origin",
        },
      );
      if (!response.ok) return;
      const value: unknown = await response.json();
      if (isRecord(value) && typeof value.stateRevision === "number")
        this.applyUpload(value, value.stateRevision);
      if (this.upload?.uploadAttemptId !== attemptId) return;
    } catch {
      this.callbacks.onError(
        "Upload outcome is uncertain. End this session before trying again.",
      );
    }
  }

  seek(positionMs: number): Promise<void> {
    const request = this.controlQueue.then(() => this.seekNow(positionMs));
    this.controlQueue = request.then(
      () => undefined,
      () => undefined,
    );
    return request;
  }

  audio(
    action: "mute" | "unmute" | "enable" | "repeat" | "recover",
    descriptionId?: string,
  ): Promise<void> {
    const intent = ++this.audioIntent;
    if (action === "mute") {
      this.pendingMuteIntent = true;
      if (this.state) {
        void this.callbacks
          .onCancelAudio(this.state.audioGeneration)
          .catch(() => {
            this.callbacks.onError(
              "Audio cancellation could not be confirmed.",
            );
          });
        this.state = { ...this.state, isMuted: true };
        this.callbacks.onState(this.state);
      }
    } else if (action === "unmute") {
      this.pendingMuteIntent = false;
    }
    const request = this.controlQueue.then(() =>
      this.audioNow(action, descriptionId, intent),
    );
    this.controlQueue = request.then(
      () => undefined,
      () => undefined,
    );
    return request;
  }

  private async audioNow(
    action: string,
    descriptionId: string | undefined,
    intent: number,
  ): Promise<void> {
    const session = this.session;
    const state = this.state;
    if (!session || !state) throw new Error("Session is unavailable");
    if (
      action === "recover" &&
      (!descriptionId ||
        descriptionId !== this.latestDescription?.descriptionId)
    )
      throw new Error("Latest result is unavailable");
    const response = await fetch(
      `/api/v1/sessions/${session.sessionId}/audio`,
      {
        method: "POST",
        credentials: "same-origin",
        headers: {
          "Content-Type": "application/json",
          "X-CSRF-Token": session.csrfToken,
        },
        body: JSON.stringify({
          action,
          epoch: state.epoch,
          ...(action === "recover" ? { descriptionId } : {}),
        }),
      },
    );
    if (!response.ok) throw new Error("Audio action is unavailable");
    const result: unknown = await response.json();
    if (
      !isRecord(result) ||
      typeof result.stateRevision !== "number" ||
      typeof result.audioGeneration !== "number" ||
      typeof result.isMuted !== "boolean" ||
      typeof result.isAudioBlocked !== "boolean"
    )
      throw new Error("Invalid audio response");
    if (this.session !== session || !this.state) return;
    if (intent === this.audioIntent) this.pendingMuteIntent = null;
    if (result.stateRevision <= this.state.stateRevision) return;
    this.state = {
      ...this.state,
      stateRevision: result.stateRevision,
      audioGeneration: result.audioGeneration,
      isMuted: this.pendingMuteIntent ?? result.isMuted,
      isAudioBlocked: result.isAudioBlocked,
    };
    this.callbacks.onState(this.state);
  }

  private async seekNow(positionMs: number): Promise<void> {
    const session = this.session;
    if (
      !session ||
      !this.state ||
      !Number.isSafeInteger(positionMs) ||
      positionMs < 0 ||
      this.state.durationMs === undefined ||
      positionMs >= this.state.durationMs
    )
      throw new Error("Seek position is outside the video");
    const durationMs = this.state.durationMs;
    const expectedEpoch = this.state.epoch;
    await this.callbacks.onCancelAudio(this.state.audioGeneration);
    this.callbacks.onReset();
    this.latestDescription = null;
    const response = await fetch(`/api/v1/sessions/${session.sessionId}/seek`, {
      method: "POST",
      credentials: "same-origin",
      headers: {
        "Content-Type": "application/json",
        "X-CSRF-Token": session.csrfToken,
      },
      body: JSON.stringify({ epoch: expectedEpoch, positionMs }),
    });
    if (!response.ok)
      throw new Error("Seek was rejected. Refresh the session state.");
    const result: unknown = await response.json();
    if (
      !isRecord(result) ||
      typeof result.epoch !== "number" ||
      typeof result.audioGeneration !== "number" ||
      typeof result.stateRevision !== "number"
    )
      throw new Error("Invalid seek response");
    const current = this.snapshot;
    if (
      this.session === session &&
      current &&
      result.stateRevision > current.stateRevision
    ) {
      this.state = {
        epoch: result.epoch,
        audioGeneration: result.audioGeneration,
        stateRevision: result.stateRevision,
        state: "processing",
        sourceTimeMs: positionMs,
        durationMs,
      };
      this.callbacks.onState(this.state);
    }
  }

  private async heartbeat(): Promise<void> {
    if (!this.session || !this.state) return;
    try {
      const response = await fetch(
        `/api/v1/sessions/${this.session.sessionId}/heartbeat`,
        {
          method: "POST",
          credentials: "same-origin",
          headers: {
            "Content-Type": "application/json",
            "X-CSRF-Token": this.session.csrfToken,
          },
          body: JSON.stringify({ epoch: this.state.epoch }),
        },
      );
      if (!response.ok) throw new Error("Heartbeat failed");
    } catch {
      this.events?.close();
      await this.stop().catch(() => undefined);
      this.callbacks.onError("Connection lost. Start a new session.");
    }
  }

  async stop(): Promise<void> {
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    this.heartbeatTimer = null;
    document.removeEventListener("visibilitychange", this.onVisibilityChange);
    this.events?.close();
    this.events = null;
    this.uploadAbort?.abort();
    this.uploadAbort = null;
    void this.callbacks.onCancelAudio(Number.MAX_SAFE_INTEGER).catch(() => {
      this.callbacks.onError("Audio cancellation could not be confirmed.");
    });
    const session = this.session;
    this.session = null;
    this.state = null;
    this.upload = null;
    this.uncertainAttemptId = null;
    this.latestDescription = null;
    this.pendingMuteIntent = null;
    this.lastEventSeq = 0;
    this.callbacks.onEnd();
    if (!session) return;
    await fetch(`/api/v1/sessions/${session.sessionId}`, {
      method: "DELETE",
      credentials: "same-origin",
      headers: { "X-CSRF-Token": session.csrfToken },
    });
  }
}
