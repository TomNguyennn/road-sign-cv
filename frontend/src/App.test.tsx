// @vitest-environment jsdom
import { cleanup, render, screen } from "@testing-library/react";
import { afterEach, expect, test } from "vitest";
import { App } from "./App";

afterEach(cleanup);

test("identifies the prototype and does not offer unavailable recognition controls", () => {
  render(<App />);
  expect(screen.getByRole("heading", { level: 1 }).textContent).toBe(
    "Assistive sign recognition",
  );
  expect(screen.getByText("Recognition is not yet available.")).toBeTruthy();
  expect(screen.queryByRole("button")).toBeNull();
});
