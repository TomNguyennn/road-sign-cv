# Docker development checks

The backend keeps request spools and accepted media on `/run/road-sign`, a 256 MiB tmpfs. The
container is read-only, runs as UID 10001, has a 4 GiB memory limit and disables core dumps. These
container limits do not prove that the Docker host or its VM has swap disabled.

## Linux host or Linux VM

Run these checks on the machine that owns the Docker daemon (inside the Linux VM when Docker uses a
VM):

```sh
swapon --show
cat /proc/swaps
docker info --format '{{.OperatingSystem}} {{.Architecture}}'
```

Both swap commands must show no configured swap devices for a no-swap test. If Docker is remote,
checking the client workstation is insufficient; run the checks on the remote daemon host. A
container-side `/proc/swaps` check can be recorded as supporting evidence, but does not replace host
verification because container namespaces and VM hypervisors can hide host details.

## Docker Desktop

Docker Desktop runs containers inside a managed Linux VM. Record the Docker Desktop version, VM
resource settings and host swap status alongside any memory or media-retention test. macOS can use
host swap independently of the Linux guest, so Docker Desktop does not provide evidence for a
strict no-durable-media claim. Treat those runs as functional tests only; use a dedicated Linux host
with verified swap settings for that claim.

Never inspect or copy request media into a persistent volume while checking cleanup. Compose
persists only Caddy's local certificate authority state; the backend has no persistent media mount.
