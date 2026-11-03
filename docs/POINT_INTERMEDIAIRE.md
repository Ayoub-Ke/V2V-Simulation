# Support du point intermédiaire — préparation

À adapter à la présentation du groupe : participants, responsabilités, difficultés réellement rencontrées et planning de fin de projet.

## 1. Objectif

Simuler la mobilité de 2 000 véhicules et étudier l'affectation des canaux radio par coloration d'un graphe d'interférences dynamique.

## 2. Démonstration

Ouvrir Mulhouse, démarrer les 2 000 véhicules, changer la vitesse du temps, sélectionner une zone, cliquer sur un véhicule, comparer les méthodes, exporter les mesures et recharger la carte. Montrer la remise à zéro et la conservation de la carte en cas de fichier invalide.

## 3. Avancement technique

Les cinq étapes disposent d'une implémentation. Les corrections concernent la durée de vie des tuiles, les unités de la grille spatiale, l'état après génération, les sens uniques, la restriction géographique et les performances. La comparaison utilise un instantané commun ; l'historique mesure aussi les réaffectations de canaux.

Utiliser la [capture réelle](../tests/audit/results/interface.png), le [rapport de validation](../RAPPORT_VERIFICATION.md) et ses journaux pour illustrer ces points.

## 4. Difficultés et résultats

Expliquer le double effacement des tuiles et sa correction, le passage à une grille 3D métrique et la différence entre validité de coloration, nombre de canaux et stabilité. Présenter les temps mesurés en précisant la machine, le jeu de données et l'exclusion du rendu.

Le téléchargement réel des tuiles et la plateforme Windows restent à vérifier sur les postes du groupe. Préparer une démonstration avec les routes vectorielles comme solution de repli hors ligne.

## 5. Suite du projet

Le groupe doit renseigner ses jalons jusqu'au 10 décembre 2026 : revue du code et compréhension par chacun, expériences sur différentes densités et portées, vérification sur les machines cibles, personnalisation du cahier des charges et du bilan des IAG, préparation du package et répétition de la soutenance.
