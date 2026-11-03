# Audit initial — avant corrections

Ce document décrit la version auditée avant les améliorations. Les défauts ci-dessous ont ensuite été traités ; consulter le rapport actuel à la racine. Les numéros de ligne sont ceux de l’ancienne version.

# Vérification du projet V2V — 29 octobre 2026

Le projet compile et les deux algorithmes produisent des colorations valides sur les graphes testés. Il reste toutefois des erreurs fonctionnelles, un crash reproductible et des éléments de l'énoncé à compléter. Le projet ne peut donc pas encore être considéré comme entièrement validé.

Référence : `projetReseauxMobiles (1).pdf`, 12 pages, fourni avec la demande. Vérification du code présent dans ce dossier ; les documents éventuellement conservés ailleurs ne sont pas évalués.

Le code applicatif existant n'a pas été modifié. Les ajouts sont ce rapport, les tests indépendants dans `tests/audit` et leurs journaux.

## Résultats exécutés

Environnement : image Docker locale `v2v-build`, GCC 13.3, Qt 6.4.2, CMake 3.28.3, C++17. Les sources sont montées en lecture seule, les compilations sont effectuées dans `/tmp` et le réseau du conteneur est désactivé.

| Vérification | Résultat |
| --- | --- |
| Compilation complète Release du programme | Réussie |
| Démarrage Qt hors écran avec les données de Mulhouse | Aucun crash observé pendant 8 secondes ; arrêt volontaire par `timeout`, code 124 |
| Suite fonctionnelle finale | 32 contrôles réussis, 8 échoués, code de sortie 1 |
| Validité des colorations | Réussie pour chaque méthode sur un graphe vide, des sommets isolés, K12 et 100 graphes aléatoires avec graine fixée |
| Rechargement d'une carte avec des tuiles en cache | Crash en Release, code 139 |
| Vérification mémoire du rechargement | AddressSanitizer confirme un `heap-use-after-free` dans `TileLayer::clear()` |
| Simulation de 2 000 véhicules, 30 mises à jour par méthode | Exécutée ; colorations valides, mais environ 500 ms par mise à jour hors rendu |

Les 100 graphes aléatoires constituent un contrôle agrégé pour chaque algorithme. Les nombres 32 et 8 désignent les assertions de la suite fonctionnelle, pas un pourcentage de conformité à l'énoncé. Le test de rechargement est exécuté séparément car il fait planter le processus.

Journaux : [compilation et démarrage](../tests/audit/results/initial-build-and-smoke.log), [suite finale et mesures Release](../tests/audit/results/release.log), [contrôle mémoire](../tests/audit/results/sanitizers.log). Le journal initial précède deux contrôles supplémentaires de vitesse et l'ajustement du cas de partition spatiale à la portée utilisée par défaut, 150 m ; le journal Release final correspond aux tests livrés. Le contrôle mémoire a été effectué avec le cas de portée 150 m, avant l'ajout des deux contrôles de vitesse.

## Conformité aux exigences

| Exigence du PDF | État constaté |
| --- | --- |
| Étape 1 : charger OSM, afficher les routes, zoomer et déplacer la carte | Fonctionnalités présentes à la lecture du code ; démarrage hors écran testé. Les gestes et le rendu visuel restent à vérifier manuellement. |
| Étape 1 : sélectionner une zone de simulation | Rectangle et coordonnées présents ; cette sélection ne limite pas le réseau ni les véhicules. |
| Étape 2 : graphe routier, directions, distances, intersections | Graphe construit et distances réelles vérifiées ; sens uniques inversés et identification de certaines intersections à corriger. |
| Étape 3 : au moins 2 000 véhicules | Population de 2 000 testée ; performance insuffisante pour l'objectif de rafraîchissement de 30 Hz choisi dans le code. Le PDF ne fixe pas de nombre d'images par seconde. |
| Étape 3 : démarrage, pause, accélération, ralentissement, réinitialisation | Commandes présentes ; pause et facteurs de vitesse testés. Régénérer/réinitialiser ne remet pas correctement à jour le graphe et les couleurs. |
| Étape 4 : graphe dynamique d'interférences et affichage sur la carte | Construction et dessin présents ; une erreur de conversion mètres/pixels peut supprimer de vraies interférences. |
| Étape 5 : au moins deux méthodes de coloration | Glouton et Welsh–Powell implémentés ; aucune arête monochrome sur les cas valides testés. |
| Étape 5 : comparer les méthodes et étudier leurs performances | Sélecteur et statistiques instantanées présents. Comparaison sur un même instantané, historique du nombre de couleurs et mesure de stabilité absents de l'application. |
| Livrables : cahier des charges, slides, manuel utilisateur/développeur | Non trouvés dans le dossier fourni. `README-docker.md` explique la compilation Docker, mais ne couvre pas le manuel demandé. |

La destination figure parmi les exemples de propriétés possibles d'un véhicule dans le PDF. Le code utilise des déplacements aléatoires sans itinéraire vers une destination : ce point est à rapprocher du cahier des charges du groupe, sans le considérer à lui seul comme un défaut obligatoire.

## Défauts à traiter en priorité

### 1. Crash au rechargement de la carte

Emplacements : [MapView.cpp](../src/MapView.cpp#L160) et [TileLayer.cpp](../src/TileLayer.cpp#L21).

`m_scene->clear()` détruit les éléments graphiques, dont les tuiles. Ensuite, `m_tileLayer->loadArea()` appelle `TileLayer::clear()`, qui parcourt les anciens pointeurs de `m_items` et tente de les supprimer à nouveau.

Reproduction automatisée : charger deux fois une petite carte dont les tuiles sont déjà dans le cache. Le premier chargement réussit ; le second plante. AddressSanitizer situe l'accès à un objet déjà libéré à `TileLayer.cpp:25`, après sa destruction par `QGraphicsScene::clear()`.

Correction proposée : vider la couche de tuiles avant de vider la scène et définir clairement qui détruit ces objets. Lors d'un changement de carte, invalider également les réponses réseau encore en attente pour éviter d'insérer d'anciennes tuiles avec la nouvelle projection ; ce second risque résulte de la lecture du code et n'a pas été testé avec un serveur réseau.

### 2. Interférences oubliées par la partition spatiale

Emplacement : [InterferenceGraph.cpp](../src/InterferenceGraph.cpp#L95).

`cellSize = maxRange * 2.0` utilise une portée en mètres comme une taille en pixels. L'examen des neuf cellules voisines ne garantit donc pas de couvrir la portée réelle.

Reproduction : projection à zoom 18 autour de Mulhouse, 101 véhicules, portée 150 m. Deux véhicules placés à `(299, 0)` et `(600, 0)` sont distants d'environ 121 m, mais occupent les cellules 0 et 2. La recherche exhaustive trouve l'arête ; la version optimisée l'oublie. Les 99 autres véhicules sont éloignés et servent à déclencher la version optimisée, utilisée seulement au-delà de 100 véhicules.

Ce zoom est celui de la projection choisie au chargement d'une petite carte, et non le facteur de zoom visuel appliqué par la molette. Sur le jeu Mulhouse fourni, la projection choisie est le zoom 15 et la comparaison exhaustive du benchmark ne révèle pas d'arête manquante.

Correction proposée : travailler dans un repère métrique cohérent ou convertir la portée en pixels avec une couverture garantie à la latitude et au zoom concernés, puis conserver la comparaison de distance réelle. Une coloration correcte d'un graphe incomplet ne garantit pas l'absence d'interférences réelles.

### 3. Graphe et couleurs périmés après génération ou réinitialisation

Emplacements : [VehicleSimulation.cpp](../src/VehicleSimulation.cpp#L25), [VehicleSimulation.h](../src/VehicleSimulation.h#L24), [MapView.cpp](../src/MapView.cpp#L282).

`spawn()` remplace les véhicules sans reconstruire ni vider le graphe d'interférences et la coloration. À l'arrêt, `tick()` ne les actualise pas. Après une première génération, les arêtes peuvent manquer ; après une réduction de la population, elles peuvent encore référencer des indices hors du nouveau vecteur. `drawForeground()` utilise ces indices sans vérification.

Reproduction : créer 10 véhicules proches, calculer le graphe et les couleurs, mettre en pause puis générer 1 véhicule. Les arêtes et les 10 anciennes couleurs subsistent. `clear()` vide les véhicules et les interférences, mais conserve le résultat de coloration et son nombre de couleurs. La réinitialisation passe elle aussi par `spawn()`.

Correction proposée : synchroniser véhicules, graphe, couleurs et statistiques après chaque génération/réinitialisation ; effacer aussi `m_coloringResult` dans `clear()`. Un changement de méthode pendant la pause doit également recalculer le résultat si la coloration est activée.

### 4. Coût de calcul élevé avec 2 000 véhicules

Mesure Release sur `data/mulhouse.osm` : 2 270 nœuds, 3 775 arêtes routières orientées, projection zoom 15. Chaque méthode effectue 30 mises à jour de 1/30 seconde simulée. La population initiale est fixée par une graine ; les choix de direction ultérieurs restent ceux du simulateur.

| Méthode active | Mise à jour moyenne, hors rendu | 95e percentile | Coloration seule, moyenne | Arêtes d'interférence finales |
| --- | --- | --- | --- | --- |
| Glouton | 507,7 ms | 563,7 ms | 4,7 ms | 59 449 |
| Welsh–Powell | 499,2 ms | 526,2 ms | 5,5 ms | 59 512 |

Les deux résultats sont valides et les arêtes coïncident avec la recherche exhaustive à la fin de chaque série. Sur chacun des deux instantanés finaux, la comparaison directe donne 58 couleurs pour le glouton et 56 pour Welsh–Powell. Ce résultat concerne ces exemples et ne prouve pas que Welsh–Powell gagne systématiquement.

Le timer de l'application vise environ 33 ms par mise à jour ; le calcul mesuré à lui seul correspond ici à environ deux mises à jour par seconde. Ce n'est pas une mesure des images par seconde de l'interface. Les valeurs dépendent de la machine et du jeu de données.

Le temps de coloration est faible par rapport au temps total. La construction du graphe répète la déprojection GPS et le calcul trigonométrique pour chaque paire candidate. À zoom 15, les cellules de 300 pixels couvrent une grande partie de la petite carte et filtrent peu les candidats.

Pistes : convertir les positions une seule fois par mise à jour, corriger le dimensionnement métrique de la grille, mesurer séparément construction et affichage, puis adapter la fréquence de recalcul. Les mesures livrées ne constituent pas une optimisation du programme.

### 5. Sélection de zone sans effet sur la simulation

Emplacements : [MapView.cpp](../src/MapView.cpp#L125) et [main.cpp](../src/main.cpp#L289).

Le relâchement du rectangle émet `zoneSelected`, dont le récepteur affiche seulement un message dans la barre d'état. Le graphe routier reste celui de tout le fichier, et `spawn()` choisit parmi toutes ses arêtes. La sélection n'est pas transmise au simulateur.

À compléter pour donner un effet à la « zone de simulation » : restreindre les routes admissibles et le placement/déplacement des véhicules, avec une règle de bord explicite.

### 6. Comparaison et analyse des colorations à compléter

Emplacements : [main.cpp](../src/main.cpp#L182) et [GraphColoring.h](../src/GraphColoring.h#L20).

L'interface permet de choisir une méthode et affiche son nombre de couleurs et son temps instantané. Elle ne conserve pas les résultats des deux méthodes sur le même graphe ni l'évolution des résultats.

Pour couvrir l'étude demandée aux pages 7 et 8 : ajouter une comparaison sur un instantané commun, relever les temps et nombres de couleurs dans la durée, puis mesurer les changements de canaux entre deux mises à jour. Prévoir un export ou des graphiques pour présenter les expériences. Les critères détaillés de la page 8 sont proposés par le sujet ; le choix des indicateurs finaux relève du cahier des charges.

## Autres défauts confirmés par les tests

- **Sens unique inversé** : `oneway=-1` est interprété comme une route à double sens, alors qu'il doit permettre seulement le parcours inverse de l'ordre des nœuds. Le fichier Mulhouse fourni ne contient pas cette valeur ; le défaut concerne d'autres données acceptées par l'application. Voir [OsmParser.cpp](../src/OsmParser.cpp#L78).
- **Intersections** : `RoadGraph::degree()` compte les identifiants de routes OSM distincts et l'affichage utilise le seuil 2. Une simple coupure d'un chemin OSM en deux est alors marquée comme une intersection. Un comptage complémentaire sur les données fournies donne 124 marqueurs avec au plus deux voisins distincts et 6 nœuds ayant au moins trois voisins que cette règle ne marque pas. Ce contrôle topologique ne modélise pas toutes les restrictions de circulation. Voir [RoadGraph.cpp](../src/RoadGraph.cpp#L107).
- **Fichier sans segment utilisable** : un fichier contenant une route dont tous les nœuds sont absents est accepté ; les bornes restent invalides. Le chargement devrait échouer avec une erreur avant la projection. Voir [OsmParser.cpp](../src/OsmParser.cpp#L98).

## Livrables à vérifier avant la remise

Le PDF demande le cahier des charges, les slides du point intermédiaire, les sources/données/exécutables et un manuel utilisateur/développeur. Le manuel doit notamment couvrir l'usage des IAG, l'architecture, les prérequis, les outils, le lancement et les fonctionnalités. Ces documents n'ont pas été trouvés ici, hormis le guide de compilation Docker.

Le sujet place le point intermédiaire dans la semaine du 2 novembre 2026 et la soutenance le 10 décembre 2026. Il reste à vérifier les livrables détenus par le groupe et à préparer une démonstration avec les limites observées.

## Relancer les tests

Depuis la racine du projet, avec l'image `v2v-build` disponible :

```bash
docker run --rm --network none -v "$PWD":/workspace:ro v2v-build bash -c '
  cmake -S /workspace/tests/audit -B /tmp/audit -G Ninja -DCMAKE_BUILD_TYPE=Release &&
  cmake --build /tmp/audit -j 2 &&
  QT_QPA_PLATFORM=offscreen ctest --test-dir /tmp/audit --output-on-failure
'
```

Les tests doivent actuellement signaler les défauts décrits : l'échec de CTest est attendu tant que le code n'est pas corrigé. Pour activer le contrôle mémoire, ajouter `-DAUDIT_SANITIZERS=ON` et utiliser une compilation Debug ; `detect_leaks=0` est défini par CTest afin de centrer ce contrôle sur les accès mémoire.

Pour les mesures, dans le même type de conteneur après compilation :

```bash
QT_QPA_PLATFORM=offscreen /tmp/audit/V2VAudit benchmark
```

Les tests compilent directement les fichiers de production avec Qt et pugixml, sans remplacer les algorithmes par des copies. Ils constituent un projet CMake indépendant et ne changent pas la compilation habituelle.

## Limites de cette vérification

Les téléchargements réels de tuiles, le rendu visuel et les gestes utilisateur n'ont pas été vérifiés sur un écran. Le test de rechargement utilise de vraies images préchargées dans le cache de test Qt pour reproduire le défaut hors réseau. Les performances portent sur le jeu fourni et une population de 2 000 véhicules, pas sur toutes les cartes ou toutes les densités. La compilation Windows/Visual Studio n'a pas été exécutée.
