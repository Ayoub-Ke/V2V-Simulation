# Build Docker — V2V Simulation

Ce conteneur ne sert **pas** à lancer l'application (interface graphique
Qt desktop). Il sert uniquement à vérifier que le projet compile de façon
identique chez les 4 membres du groupe, indépendamment de l'OS ou des
versions de Qt/CMake installées localement. Chacun continue de développer
et lancer l'appli normalement dans Visual Studio sous Windows.

## Pré-requis

Docker Desktop installé (Windows/Mac/Linux). Sous Windows, utiliser le
backend WSL2 (proposé par défaut à l'installation).

## Construire l'image (une seule fois, ou après modification du Dockerfile)

```bash
docker build -t v2v-build .
```

## Compiler le projet

Depuis la racine du projet (là où se trouve `CMakeLists.txt`) :

```bash
docker run --rm -v "$PWD":/workspace v2v-build ./scripts/docker-build.sh
```

Sous PowerShell (Windows) :

```powershell
docker run --rm -v ${PWD}:/workspace v2v-build ./scripts/docker-build.sh
```

Le code source de la machine hôte est monté dans le conteneur (`-v`) :
toute modification locale est prise en compte sans reconstruire l'image.
Le résultat de compilation apparaît dans `build-linux/V2VSimulation`
sur la machine hôte.

## Ouvrir un terminal dans le conteneur (debug manuel)

```bash
docker run --rm -it -v "$PWD":/workspace v2v-build
```

Puis à l'intérieur :

```bash
./scripts/docker-build.sh
```

## Pourquoi ça compile alors que le CMakeLists.txt vise Visual Studio ?

Le `CMakeLists.txt` détecte automatiquement l'OS : le chemin Qt Windows
codé en dur (`C:/Qt/...`) n'est utilisé que sous Windows ; sous Linux,
Qt est installé via `apt` dans l'image et `find_package` le trouve seul.
Le reste du code (C++/Qt standard) est portable sans modification.

## Utilité pour le groupe

- Avant de pousser sur Git, chacun peut vérifier que son code compile
  aussi dans un environnement propre (attrape les oublis d'include,
  le code qui ne compile que "par accident" chez soi).
- Base prête pour brancher une CI (GitHub Actions) plus tard, qui
  réutiliserait ce même Dockerfile pour compiler à chaque push.
