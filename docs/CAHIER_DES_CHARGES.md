# Cahier des charges — base technique à personnaliser

Ce document décrit le logiciel livré après corrections. Les participants et décisions propres au groupe doivent être renseignés avant remise ; ce document ne prétend pas remplacer le cahier des charges déjà envoyé au responsable du cours.

## Sujet et objectifs

Simuler en C++ les déplacements d'au moins 2 000 véhicules sur un réseau routier OpenStreetMap. Construire un graphe dynamique d'interférences, affecter des canaux distincts aux véhicules voisins et comparer au moins deux méthodes de coloration en qualité, temps et stabilité.

## Participants et organisation

| Étape | Responsable | Contributeurs |
| --- | --- | --- |
| Visualisation du territoire | À renseigner | À renseigner |
| Graphe routier | À renseigner | À renseigner |
| Simulation des véhicules | À renseigner | À renseigner |
| Graphe d'interférences | À renseigner | À renseigner |
| Coloration et expérimentation | À renseigner | À renseigner |
| Intégration, tests et livrables | À renseigner | À renseigner |

Méthode proposée : tâches courtes suivies dans un tableau partagé, démonstration régulière, vérification des tests avant intégration. Le groupe doit préciser sa méthode réellement adoptée et les outils de suivi utilisés.

## Fonctionnalités et critères de recette

| Fonction | Critère de recette |
| --- | --- |
| Carte interactive | Charger OSM, se déplacer, zoomer, recadrer et recharger sans conserver les anciens objets. |
| Réseau routier | Conserver la géométrie, respecter les sens uniques, utiliser des distances réelles et identifier les intersections topologiques. |
| Population | Simuler 2 000 véhicules et permettre génération, pause, accélération, ralentissement et réinitialisation. |
| Zone | Ne placer et ne déplacer les véhicules que sur les segments inclus dans la sélection ; refuser une zone sans route. |
| Interférences | Retrouver les mêmes paires que le calcul sphérique de référence, indépendamment de l'échelle de projection. |
| Coloration | Aucun canal identique aux deux extrémités d'une arête ; méthodes gloutonne et Welsh–Powell. |
| Comparaison | Exécuter les deux méthodes sur le même instantané et conserver leurs temps et nombres de canaux. |
| Analyse | Historique des canaux, coût des mises à jour, changements de canaux et export CSV. |
| Robustesse | Entrées invalides refusées sans perdre la simulation ; tests des rechargements répétés et des changements de population. |

La fenêtre comporte un en-tête pour les fichiers, un panneau de commandes à gauche, quatre indicateurs, une carte centrale et deux panneaux d'analyse. Aperçu réel : [capture de l'interface](../tests/audit/results/interface.png).

## Outils et architecture

C++17, Qt 6 Widgets/Network, pugixml, CMake, Ninja, CTest et Docker. L'IDE de chaque participant et la gestion de versions effectivement employée restent à renseigner. Le manuel [README.md](../README.md) décrit les composants et les procédures de compilation, lancement et test.

## Échéances du sujet

| Échéance annoncée | Attendu |
| --- | --- |
| 25 septembre 2026 | Groupes et répartition des rôles |
| 2 octobre 2026 | Cahier des charges |
| Semaine du 2 novembre 2026 | Point intermédiaire, démonstration, difficultés et planning final |
| 10 décembre 2026 | Soutenance et package complet |

Le groupe doit ajouter ses jalons internes et confirmer son rendez-vous. Les dates ci-dessus viennent du sujet et ne constituent pas une confirmation de démarches effectuées.

## Risques et solutions de repli

Le fond cartographique dépend du réseau et d'un service de tuiles ; les routes vectorielles fonctionnent hors ligne. Les zones très denses peuvent produire beaucoup d'arêtes ; les liens sont donc masqués initialement, avec une inspection par véhicule. Les performances doivent être mesurées sur les machines de démonstration. Le déplacement retenu est une marche aléatoire sur les routes et ne calcule pas un itinéraire vers une destination.

## Livrables

Sources, données, exécutables adaptés à la plateforme, manuel utilisateur/développeur avec bilan des IAG, cahier des charges personnalisé et slides du point intermédiaire. Le rapport de validation et les journaux de tests accompagnent le code.
