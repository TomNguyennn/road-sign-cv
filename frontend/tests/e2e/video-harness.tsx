import { createRoot } from "react-dom/client";
import { VideoInput } from "../../src/modules/media-input/VideoInput";

interface VideoHarness {
  events: string[];
  cancelCutoffs: number[];
  audioReadyCount: number;
  audioFetchCompleted: boolean;
}

declare global {
  interface Window {
    videoHarness: VideoHarness;
  }
}

const harness: VideoHarness = {
  events: [],
  cancelCutoffs: [],
  audioReadyCount: 0,
  audioFetchCompleted: false,
};
window.videoHarness = harness;
let audioFetch: AbortController | null = null;

const root = document.getElementById("root");
if (!root) throw new Error("Video harness root is missing");
createRoot(root).render(
  <VideoInput
    onAudioReady={(payload) => {
      harness.audioReadyCount += 1;
      if (
        typeof payload !== "object" ||
        payload === null ||
        !("audioUrl" in payload) ||
        typeof payload.audioUrl !== "string"
      )
        return;
      audioFetch = new AbortController();
      void fetch(payload.audioUrl, { signal: audioFetch.signal })
        .then(() => {
          harness.audioFetchCompleted = true;
        })
        .catch(() => undefined);
    }}
    onCancelAudio={(cutoff) => {
      harness.events.push("cancel");
      harness.cancelCutoffs.push(cutoff);
      audioFetch?.abort();
      audioFetch = null;
      return Promise.resolve();
    }}
    onFrame={() => {
      harness.events.push("frame");
    }}
    onReset={() => {
      harness.events.push("reset");
    }}
  />,
);
