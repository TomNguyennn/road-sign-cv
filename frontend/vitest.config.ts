import react from "@vitejs/plugin-react";
import { configDefaults, defineConfig } from "vitest/config";

export default defineConfig({
  plugins: [react()],
  test: {
    environment: "jsdom",
    clearMocks: true,
    restoreMocks: true,
    fileParallelism: false,
    exclude: [...configDefaults.exclude, "tests/e2e/**"],
  },
});
