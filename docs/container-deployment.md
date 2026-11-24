# Local Compose deployment

Copy `config/demo.env.example` to `config/demo.env`, set `DEMO_HOST` to the machine name or LAN address used by the browser, and run `docker compose up --build`. Caddy terminates HTTPS with its internal CA and proxies `/api/v1` to the private backend network. Trust the generated root certificate on each test device before opening the camera.

The backend is non-root, read-only, drops capabilities, has a 4 GiB memory limit, a 256 MiB `/run/road-sign` tmpfs, and core dumps disabled. Session uploads and decoder files must remain under that tmpfs. Caddy persistent state contains CA data only. The private backend is not published on a host port; local and Azure deployments must not expose the admission route.

The prototype currently has no verified model bundle. `/api/v1/ready` therefore returns 503; this is expected, and no placeholder inference behavior is substituted.

Before a demo on a Linux host or VM, verify swap is disabled with `swapon --show` and `cat /proc/swaps`. If active swap is listed, turn it off for the host and persist the setting through the machine's approved OS configuration before the demo. Docker memory limits do not disable host swap.

The Compose smoke test builds both image targets, starts the stack, asserts health is 200 and readiness is 503 without a model bundle, then removes test containers and volumes.
