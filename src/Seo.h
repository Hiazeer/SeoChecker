// Analyse SEO d'un contenu HTML ou texte (indépendant de Notepad++).
#pragma once
#include <string>
#include <vector>

namespace seo {

enum class Niveau { Ok, Info, Avert, Alerte };

struct Controle {
	std::wstring categorie; // Title, Meta, Structure, Mot-clé, Lisibilité, Liens, Images, Contenu
	std::wstring libelle;   // ce qui est mesuré
	std::wstring valeur;    // valeur mesurée
	std::wstring conseil;   // recommandation si problème
	Niveau niveau = Niveau::Ok;
};

struct Reglages {
	std::wstring motcle;  // mot-clé cible
	std::wstring domaine; // pour distinguer liens internes/externes (ex. monsite.fr)
};

struct Resultat {
	std::wstring source;   // nom de fichier ou URL
	std::wstring motcle;
	bool estHtml = false;
	int score = 0;         // 0-100
	int nbMots = 0;
	std::vector<Controle> controles;
};

// Lit la ligne d'en-tête « seo: motcle=... ; domaine=... » (même dans un commentaire HTML).
Reglages lireReglages(const std::wstring &contenu);

Resultat analyser(const std::wstring &contenu, const Reglages &reg, const std::wstring &source);

// Rapport HTML autonome (clair/sombre).
std::wstring genererRapport(const Resultat &r);

// Outils exposés pour les tests / le plugin.
bool ressembleHtml(const std::wstring &contenu);
std::string versUtf8(const std::wstring &s);
std::wstring depuisUtf8(const std::string &s);
std::wstring libelleNiveau(Niveau n);

} // namespace seo
