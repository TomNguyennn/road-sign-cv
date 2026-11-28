import { expect, test } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.goto("/");
});

test("keyboard reaches labelled controls in contract order with visual results hidden", async ({
  page,
}) => {
  const names = [
    "Input mode",
    "Start",
    "Stop",
    "Mute",
    "Repeat latest",
    "Show visual results",
  ];
  for (const name of names) {
    const control = page.getByRole(
      name === "Input mode" ? "combobox" : "button",
      { name },
    );
    await expect(control).toHaveAccessibleName(name);
  }
  await expect(page.getByRole("status")).toBeVisible();
  await expect(
    page.getByRole("region", { name: /transcript|results/i }),
  ).not.toHaveAttribute("aria-live", "polite");
  await page.keyboard.press("Tab");
  await expect(
    page.getByRole("combobox", { name: "Input mode" }),
  ).toBeFocused();
  await expect(page.getByRole("combobox", { name: "Input mode" })).toHaveCSS(
    "outline-style",
    /^(?!none$).+/,
  );
});

test("muted repeat explains why it is unavailable, and stop remains reachable", async ({
  page,
}) => {
  await page.getByRole("button", { name: "Start" }).click();
  await page.getByRole("button", { name: "Mute" }).click();
  const repeat = page.getByRole("button", { name: "Repeat latest" });
  await expect(repeat).toBeDisabled();
  await expect(page.getByText(/unmute.*repeat|repeat.*muted/i)).toBeVisible();
  await page.getByRole("button", { name: "Unmute" }).click();
  await expect(page.getByRole("button", { name: "Mute" })).toBeVisible();
  await page.getByRole("button", { name: "Stop" }).click();
  await expect(page.getByRole("status")).toContainText(/stopped|ended/i);
  await expect(page.getByRole("button", { name: "Start" })).toBeEnabled();
});

test("camera denial has a recovery action and leaves no active capture", async ({
  page,
  context,
}) => {
  await context.clearPermissions();
  await page
    .getByRole("combobox", { name: "Input mode" })
    .selectOption("camera");
  await page.getByRole("button", { name: "Start" }).click();
  await expect(page.getByRole("status")).toContainText(
    /camera.*(denied|permission)/i,
  );
  await expect(page.getByRole("button", { name: "Start" })).toBeEnabled();
  await expect(page.locator("video")).toHaveCount(0);
});

test("malformed upload reports attempts and permits explicit retry", async ({
  page,
}) => {
  await page
    .getByRole("combobox", { name: "Input mode" })
    .selectOption("image");
  await expect(page.getByText(/attempts|unlimited/i)).toBeVisible();
  await page.getByRole("button", { name: "Start" }).click();
  await page.getByLabel(/file|image/i).setInputFiles({
    name: "broken.png",
    mimeType: "image/png",
    buffer: Buffer.from("not an image"),
  });
  await expect(page.getByRole("status")).toContainText(
    /invalid|unsupported|try again/i,
  );
  await expect(page.getByLabel(/file|image/i)).toBeEnabled();
});

test("source-specific controls have labels and disabled reasons", async ({
  page,
}) => {
  const mode = page.getByRole("combobox", { name: "Input mode" });
  await mode.selectOption("image");
  await expect(page.getByLabel(/file|image/i)).toHaveAttribute("type", "file");
  await expect(page.getByText(/jpeg|png/i)).toBeVisible();
  await mode.selectOption("video");
  await expect(page.getByLabel(/file|video/i)).toHaveAttribute("type", "file");
  await expect(
    page.getByRole("slider", { name: /seek|position/i }),
  ).toBeDisabled();
  await expect(page.getByText(/upload.*seek|seek.*upload/i)).toBeVisible();
});

test("camera track revocation stops capture and requires explicit restart", async ({
  page,
}) => {
  await page.addInitScript(() => {
    const stream = document.createElement("canvas").captureStream(5);
    Object.defineProperty(window, "testCameraTrack", {
      value: stream.getVideoTracks()[0],
    });
    navigator.mediaDevices.getUserMedia = () => Promise.resolve(stream);
  });
  await page.reload();
  await page
    .getByRole("combobox", { name: "Input mode" })
    .selectOption("camera");
  await page.getByRole("button", { name: "Start" }).click();
  await page.evaluate(() => {
    const track = (window as unknown as { testCameraTrack: MediaStreamTrack })
      .testCameraTrack;
    track.dispatchEvent(new Event("ended"));
  });
  await expect(page.getByRole("status")).toContainText(
    /camera.*(ended|lost|restart)/i,
  );
  await expect(page.getByRole("button", { name: "Start" })).toBeEnabled();
});

test("EventSource failure closes the session and never resumes automatically", async ({
  page,
}) => {
  await page.addInitScript(() => {
    class FailedEventSource extends EventTarget {
      constructor() {
        super();
        queueMicrotask(() => this.dispatchEvent(new Event("error")));
      }
      close() {}
    }
    Object.defineProperty(window, "EventSource", { value: FailedEventSource });
  });
  await page.reload();
  await page.getByRole("button", { name: "Start" }).click();
  await expect(page.getByRole("status")).toContainText(/connection|restart/i);
  await expect(page.getByRole("button", { name: "Start" })).toBeEnabled();
});

test("hidden page ends capture and requires a new Start", async ({ page }) => {
  await page.getByRole("button", { name: "Start" }).click();
  await page.evaluate(() => {
    Object.defineProperty(document, "visibilityState", {
      configurable: true,
      value: "hidden",
    });
    document.dispatchEvent(new Event("visibilitychange"));
  });
  await expect(page.getByRole("status")).toContainText(
    /hidden|stopped|start again/i,
  );
  await expect(page.getByRole("button", { name: "Start" })).toBeEnabled();
});

test("blocked audio keeps text readable and offers explicit activation", async ({
  page,
}) => {
  await page.addInitScript(() => {
    window.AudioContext.prototype.resume = () =>
      Promise.reject(new Error("activation blocked"));
  });
  await page.reload();
  await page.getByRole("button", { name: "Start" }).click();
  await expect(page.getByRole("status")).toContainText(
    /audio.*(blocked|enable)/i,
  );
  await expect(
    page.getByRole("button", { name: "Enable audio" }),
  ).toBeVisible();
  await expect(
    page.getByRole("region", { name: /transcript|results/i }),
  ).not.toHaveAttribute("aria-live", "polite");
});
