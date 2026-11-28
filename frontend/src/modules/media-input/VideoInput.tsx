import { useEffect, useRef, useState } from "react";
import {
  SessionClient,
  type SessionState,
  type UploadState,
} from "../recognition-session/SessionClient";

interface VideoInputProps {
  onAudioReady: (payload: unknown) => void;
  onCancelAudio: (cutoff: number) => Promise<void>;
  onFrame: (payload: unknown) => void;
  onReset: () => void;
}

const MAX_VIDEO_BYTES = 100 * 1024 * 1024;

export function VideoInput({
  onAudioReady,
  onCancelAudio,
  onFrame,
  onReset,
}: VideoInputProps) {
  const client = useRef<SessionClient | null>(null);
  const callbacks = useRef({ onAudioReady, onCancelAudio, onFrame, onReset });
  callbacks.current = { onAudioReady, onCancelAudio, onFrame, onReset };
  const [state, setState] = useState<SessionState | null>(null);
  const [upload, setUpload] = useState<UploadState | null>(null);
  const [status, setStatus] = useState(
    "Start a video session to upload a file.",
  );
  const [seekPosition, setSeekPosition] = useState(0);
  const [isBusy, setIsBusy] = useState(false);

  useEffect(() => {
    const session = new SessionClient({
      onState(next) {
        setState(next);
        if (next.state === "reviewing")
          setStatus("Video complete. Results are ready to review.");
      },
      onUpload(next) {
        setUpload(next);
        if (next.uploadStatus === "accepted")
          setStatus("Video accepted. Processing has started.");
        if (next.uploadStatus === "rejected")
          setStatus("Video rejected. Select a supported file to try again.");
      },
      onReset() {
        setSeekPosition(0);
        callbacks.current.onReset();
      },
      onCancelAudio: async (cutoff) => {
        await callbacks.current.onCancelAudio(cutoff);
      },
      onFrame: (payload) => {
        callbacks.current.onFrame(payload);
      },
      onAudioReady: (payload) => {
        callbacks.current.onAudioReady(payload);
      },
      onError(message) {
        setStatus(message);
      },
      onEnd() {
        setState(null);
        setUpload(null);
      },
    });
    client.current = session;
    return () => {
      void session.stop().catch(() => undefined);
      client.current = null;
    };
  }, []);

  async function start() {
    setIsBusy(true);
    try {
      await client.current?.start();
      setStatus("Video session ready. Choose an MP4 or WebM file.");
    } catch (error) {
      setStatus(
        error instanceof Error
          ? error.message
          : "Could not start video session.",
      );
    } finally {
      setIsBusy(false);
    }
  }

  async function uploadFile(file: File | undefined) {
    if (!file) return;
    if (
      file.size === 0 ||
      file.size > MAX_VIDEO_BYTES ||
      !["video/mp4", "video/webm"].includes(file.type)
    ) {
      setStatus("Choose an MP4 or WebM file up to 100 MiB.");
      return;
    }
    setIsBusy(true);
    try {
      await client.current?.uploadVideo(file);
    } catch (error) {
      setStatus(
        error instanceof Error ? error.message : "Video upload failed.",
      );
    } finally {
      setIsBusy(false);
    }
  }

  async function seek() {
    setIsBusy(true);
    try {
      await client.current?.seek(seekPosition);
      setStatus("Seeking. Previous results and audio were cleared.");
    } catch (error) {
      setStatus(error instanceof Error ? error.message : "Seek failed.");
    } finally {
      setIsBusy(false);
    }
  }

  async function stop() {
    setIsBusy(true);
    try {
      await client.current?.stop();
    } finally {
      setState(null);
      setUpload(null);
      setSeekPosition(0);
      setIsBusy(false);
      setStatus("Session stopped.");
    }
  }

  const durationMs = state?.durationMs ?? 0;
  const canSeek =
    !isBusy &&
    durationMs > 0 &&
    (state?.state === "processing" || state?.state === "reviewing");
  const canUpload =
    !isBusy &&
    state?.state === "awaitingInput" &&
    upload?.uploadStatus !== "accepted" &&
    upload?.canRetryUpload !== false &&
    !client.current?.isUploadUncertain;

  return (
    <section aria-label="Video input">
      <p>
        Supported video: MP4/H.264 or WebM/VP8/VP9, up to 100 MiB and five
        minutes.
      </p>
      <button
        type="button"
        onClick={() => {
          void start();
        }}
        disabled={isBusy || state !== null}
      >
        Start video session
      </button>
      <button
        type="button"
        onClick={() => {
          void stop();
        }}
        disabled={state === null}
      >
        Stop video session
      </button>
      <label htmlFor="video-file">Video file</label>
      <input
        id="video-file"
        type="file"
        accept="video/mp4,video/webm"
        disabled={!canUpload}
        onChange={(event) => {
          void uploadFile(event.currentTarget.files?.[0]);
        }}
      />
      {upload && (
        <p>
          Upload attempts: {upload.uploadAttemptsUsed} of{" "}
          {upload.maxUploadAttempts === 0
            ? "unlimited"
            : upload.maxUploadAttempts}
        </p>
      )}
      <label htmlFor="video-progress">Video progress</label>
      <progress
        id="video-progress"
        value={state?.sourceTimeMs ?? 0}
        max={durationMs || 1}
      />
      <label htmlFor="video-seek">Seek position (seconds)</label>
      <input
        id="video-seek"
        type="range"
        min={0}
        max={Math.max(0, Math.ceil(durationMs / 1000) - 1)}
        value={seekPosition / 1000}
        disabled={!canSeek}
        onChange={(event) => {
          setSeekPosition(Number(event.currentTarget.value) * 1000);
        }}
      />
      <button
        type="button"
        onClick={() => {
          void seek();
        }}
        disabled={!canSeek}
      >
        Seek to selected position
      </button>
      <p role="status" aria-live="polite">
        {status}
      </p>
    </section>
  );
}
