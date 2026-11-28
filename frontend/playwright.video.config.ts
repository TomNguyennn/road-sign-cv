import { defineConfig, devices } from "@playwright/test";

export default defineConfig({
  testDir: "./tests/e2e",
  testMatch: "TrackSigns.spec.ts",
  fullyParallel: false,
  reporter: "list",
  use: { baseURL: "http://127.0.0.1:5174" },
  projects: [{ name: "chromium", use: { ...devices["Desktop Chrome"] } }],
  webServer: {
    command: "npm run dev -- --host 127.0.0.1 --port 5174 --strictPort",
    url: "http://127.0.0.1:5174/tests/e2e/video-harness.html",
    reuseExistingServer: false,
  },
});
