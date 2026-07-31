# Example project Dockerfile for a blank project.
# Copy to .devcontainer/Dockerfile in your repo, then add your own layers.
#
# claude-dev-base:local is built by devcontainer.json's `initializeCommand`
# before this image, so this FROM resolves from the local Docker daemon —
# no registry, no docker login.

FROM claude-dev-base:local

# The base image ends as USER node. Re-elevate to install system packages,
# then drop back to node at the end (the devcontainer's remoteUser).
USER root

# ── Your project's system packages ──────────────────────────────────────────
# RUN apt-get update && apt-get install -y --no-install-recommends \
#   postgresql-client \
#   && apt-get clean && rm -rf /var/lib/apt/lists/*

# ── Arduino toolchain for building / verifying the ESP32 firmware ────────────
# arduino-cli drives the compile; pyserial is required by the esp32 core's
# build scripts. The ESP32 core + libraries are large, so they are installed
# on first run by tools/compile.sh rather than baked into the image.
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
      | BINDIR=/usr/local/bin sh \
 && pip3 install pyserial

# ── Your project's Python deps ──────────────────────────────────────────────
# The base ships CPython 3.10.20 (built from source) with pip on PATH and sets
# PIP_BREAK_SYSTEM_PACKAGES=1, so plain pip3 install works.
# COPY requirements.txt /tmp/requirements.txt
# RUN pip3 install -r /tmp/requirements.txt

# ── Your project's global Node CLIs ─────────────────────────────────────────
# RUN npm install -g some-cli

# ── Optional: widen the base firewall's allowlist ───────────────────────────
# Only needed if you enable the firewall (see devcontainer.json). One domain
# per line; `#` comments allowed.
# RUN printf 'pypi.org\nfiles.pythonhosted.org\n' > /etc/firewall-extra-domains

USER node
