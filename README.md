# Simulateur de Cerveau (C++)

*[English version](README.en.md)*

Un programme en terminal qui montre comment fonctionne le cerveau, avec de vrais modèles issus des neurosciences computationnelles :

1. **Neurones** : comment un neurone émet des potentiels d'action (spikes), et comment 4 vrais types de neurones se comportent différemment.
2. **Cortex** : 1000 neurones connectés qui produisent d'eux-mêmes des ondes cérébrales (alpha, bêta, gamma).
3. **Apprentissage** : le chien de Pavlov. Un neurone apprend qu'une cloche annonce la nourriture grâce à la STDP, la règle d'apprentissage utilisée par les vraies synapses.

Aucune bibliothèque externe : du C++17 pur et des graphiques en ASCII.

## Compiler et lancer

**Avec g++ (le plus simple) :**

```bash
g++ -std=c++17 -O2 -Iinclude src/main.cpp -o brain
./brain
```

Sous Windows (PowerShell) : `g++ -std=c++17 -O2 -Iinclude src/main.cpp -o brain.exe` puis `.\brain.exe`

**Avec CMake :**

```bash
cmake -S . -B build
cmake --build build --config Release
./build/brain
```

Sous Windows, l'exécutable se trouve dans `build\Release\brain.exe`.

**Commandes disponibles :**

```bash
./brain              # menu interactif
./brain neurons      # expérience 1 : un neurone
./brain cortex       # expérience 2 : ondes cérébrales
./brain learn        # expérience 3 : apprentissage
./brain all          # tout lancer
./brain all --no-color
```

Astuce : agrandissez la fenêtre du terminal, certains graphiques font environ 110 caractères de large.

## Structure du projet

```
include/
  neuron.hpp    modèle de neurone d'Izhikevich + 4 types de neurones
  network.hpp   réseau cortical de 1000 neurones + analyse spectrale (type EEG)
  learning.hpp  circuit de conditionnement pavlovien avec apprentissage STDP
  plot.hpp      graphiques ASCII (tension, barres, histogrammes)
  color.hpp     couleurs du terminal (ANSI)
src/
  main.cpp      les trois expériences et le menu
```

## La science derrière le projet

**Modèle de neurone (Izhikevich, 2003).** Deux équations décrivent la tension `v` d'un neurone et sa variable de récupération `u` :

```
v' = 0.04v² + 5v + 140 − u + I
u' = a(bv − u)
si v ≥ 30 mV :  v = c,  u = u + d     (spike !)
```

En changeant seulement `a, b, c, d`, on obtient des neurones à décharge régulière, en rafales (bursting), « chattering » ou à décharge rapide.

**Réseau cortical.** 800 neurones excitateurs et 200 neurones inhibiteurs, connectés aléatoirement, avec un bruit sensoriel aléatoire. L'excitation propage l'activité, l'inhibition l'arrête, et ce va-et-vient synchronise les neurones en rythmes. Le programme les mesure avec une transformée de Fourier, comme dans une analyse EEG.

**Apprentissage (STDP).** Plasticité dépendante du temps d'occurrence des spikes (Bi & Poo, 1998) :
- l'entrée s'active juste **avant** la sortie → la synapse devient **plus forte**
- l'entrée s'active juste **après** la sortie → la synapse devient **plus faible**

Comme la cloche arrive toujours juste avant que la nourriture fasse réagir le neurone, les synapses de la cloche se renforcent jusqu'à ce que la cloche seule déclenche le neurone. La lumière n'est jamais suivie de nourriture, donc elle reste ignorée.

## Idées pour aller plus loin

- **Extinction** : faire sonner la cloche plusieurs fois sans nourriture. Pouvez-vous ajouter une règle qui fait « oublier » le neurone ?
- **Graphiques en temps réel** : dessiner le réseau avec SFML ou raylib (les neurones s'allument quand ils s'activent).
- **Reconnaissance de formes** : entraîner des neurones de sortie avec STDP et inhibition latérale pour reconnaître des chiffres manuscrits (Diehl & Cook, 2015).
- **Sommeil** : réduire le bruit d'entrée et observer l'apparition d'ondes lentes (delta).
- **Performance** : passer à 10 000 neurones avec des connexions creuses et du multithreading.

## Références

- E. M. Izhikevich, *Simple Model of Spiking Neurons*, IEEE Trans. Neural Networks, 2003
- G. Bi & M. Poo, *Synaptic Modifications in Cultured Hippocampal Neurons*, J. Neuroscience, 1998
