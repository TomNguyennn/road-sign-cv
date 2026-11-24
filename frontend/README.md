# Web frontend setup

Use Node 22.23.5 (`.nvmrc`) and npm 11.21.3. npm 10.9.4 crashed while resolving
Vitest's optional peer dependencies; npm 11.21.3 resolved the graph without
ignoring peer requirements. Install the exact lock with `npm ci --ignore-scripts`.

```sh
npm run lint
npm run typecheck
npm test
npm run build
```

`npm run dev` binds only to localhost for development. Deployment serves `dist/`
through Caddy; it must not expose the Vite development server.

The initial page identifies the prototype and states that recognition is unavailable.
It does not request a camera, call the backend, or simulate recognition. Session
controls and domain features are implemented in later tasks.

`npm run test:e2e` reserves the Playwright entry point. Browser configuration and
acceptance tests are added in T008 and the feature tasks; that suite is not yet
available and must not be reported as passing. Unit tests currently cover only
the initial page. Full accessibility and browser acceptance remain outstanding.
