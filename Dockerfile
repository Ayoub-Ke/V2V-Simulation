# Environnement de build commun pour le projet V2V Simulation.
# Ne sert PAS à afficher l'interface graphique (appli desktop Qt) : uniquement à compiler
# le projet de façon identique chez tous les membres du groupe, et en CI plus tard.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# --- Outils de build + Qt6 + dépendances -----------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    qt6-base-dev \
    qt6-base-dev-tools \
    libqt6network6t64 \
    libgl1-mesa-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# --- pugixml compilé depuis les sources : garantit le fichier pugixml-config.cmake -----
# (le paquet apt ne le fournit pas toujours selon la version d'Ubuntu)
RUN git clone --depth 1 --branch v1.14 https://github.com/zeux/pugixml.git /tmp/pugixml \
    && cmake -S /tmp/pugixml -B /tmp/pugixml/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build /tmp/pugixml/build \
    && cmake --install /tmp/pugixml/build \
    && rm -rf /tmp/pugixml

WORKDIR /workspace

# Le code source est monté en volume au lancement (voir README-docker.md) : pas copié ici,
# pour que les modifications locales soient prises en compte sans reconstruire l'image.
CMD ["bash"]
