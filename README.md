# SEO Checker

**Plugin Notepad++ d'analyse SEO on-page.** Analysez le référencement d'une page — le fichier ouvert (HTML, Markdown ou texte) **ou** une URL en ligne — directement dans Notepad++ (64 bits). Un panneau donne un score sur 100, un rapport HTML détaillé s'ouvre dans le navigateur.

## Fonctions

- **Deux sources** : le document ouvert, ou une page web téléchargée (WinHTTP).
- **Panneau ancré** : score global et liste des contrôles en couleur (vert / orange / rouge).
- **Rapport détaillé** : page HTML lisible avec une jauge et un conseil par point.
- **Contrôles** : title (longueur, mot-clé), meta description, structure Hn, présence et densité du mot-clé, lisibilité (longueur des phrases), nombre de mots, ratio texte/code, liens internes/externes, images sans attribut `alt`.

## Mot-clé cible

Ajoutez une ligne d'en-tête dans le fichier (la commande « Insérer l'en-tête SEO » le fait pour vous) :

```
<!-- seo: motcle=location vélo annecy | domaine=monsite.fr -->
```

- `motcle` : l'expression visée.
- `domaine` : votre site, pour classer les liens internes / externes.

Voir [exemple.html](exemple.html).

## Installation (binaire)

1. Téléchargez `SeoChecker.zip` depuis les *Releases*.
2. Fermez Notepad++.
3. Décompressez-le dans `C:\Program Files\Notepad++\plugins\` pour obtenir
   `C:\Program Files\Notepad++\plugins\SeoChecker\SeoChecker.dll`.
4. Rouvrez Notepad++ : menu **Modules > SEO Checker**.

Le dossier doit porter le même nom que la DLL.

## Raccourcis

`Ctrl+Alt+Maj` + `E` (panneau), `A` (analyser le fichier), `U` (analyser une URL), `R` (rapport détaillé).

## Compilation depuis les sources

Avec [MinGW-w64](https://winlibs.com/) (GCC) :

```bat
build.bat
```

Cela produit `build\SeoChecker.dll` et lance les tests sur l'exemple. La DLL est liée à `winhttp` pour le téléchargement d'URL.

## Licence

MIT — voir [LICENSE](LICENSE).
