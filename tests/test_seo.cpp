// Test de l'analyse SEO hors Notepad++ : test_seo.exe exemple.html rapport.html
#include "../src/Seo.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <windows.h>

using namespace seo;

static int g_echecs = 0;
#define VERIFIE(c) do { if (!(c)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #c); ++g_echecs; } } while (0)

static std::wstring lireUtf8(const char *chemin) {
	std::ifstream f(chemin, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return depuisUtf8(ss.str());
}

static const Controle *trouver(const Resultat &r, const wchar_t *libFragment) {
	for (auto &c : r.controles)
		if (c.libelle.find(libFragment) != std::wstring::npos) return &c;
	return nullptr;
}

int main(int argc, char **argv) {
	if (argc < 3) { std::printf("usage : test_seo exemple.html rapport.html\n"); return 2; }

	VERIFIE(ressembleHtml(L"<html><body>x</body></html>"));
	VERIFIE(!ressembleHtml(L"Juste du texte simple"));
	VERIFIE(depuisUtf8(versUtf8(L"éàç€ 日本")) == L"éàç€ 日本");

	std::wstring contenu = lireUtf8(argv[1]);
	Reglages reg = lireReglages(contenu);
	VERIFIE(reg.motcle == L"location vélo annecy");
	VERIFIE(reg.domaine == L"monsite.fr");

	Resultat r = analyser(contenu, reg, L"exemple.html");
	VERIFIE(r.estHtml);
	VERIFIE(r.nbMots > 100);

	const Controle *title = trouver(r, L"title");
	VERIFIE(title && title->niveau == Niveau::Ok); // 62? vérifie plage
	const Controle *hn = trouver(r, L"Titres Hn");
	VERIFIE(hn && hn->valeur.find(L"H1:1") != std::wstring::npos && hn->valeur.find(L"H2:2") != std::wstring::npos);
	VERIFIE(hn->niveau == Niveau::Ok);

	const Controle *mc = trouver(r, L"Présence de");
	VERIFIE(mc && mc->valeur.find(L"fois") != std::wstring::npos);

	const Controle *liens = trouver(r, L"Liens internes");
	VERIFIE(liens && liens->valeur == L"2 / 1"); // 2 internes (/tarifs, /guide), 1 externe (lac-annecy.com)

	const Controle *imgs = trouver(r, L"Images sans attribut alt");
	VERIFIE(imgs && imgs->valeur == L"1 sur 2");

	const Controle *ratio = trouver(r, L"Ratio texte");
	VERIFIE(ratio != nullptr);

	std::printf("Score HTML : %d/100, %d mots\n", r.score, r.nbMots);

	// Mode texte
	std::wstring txt = L"# Mon titre d'article\n\nUn paragraphe de test avec quelques mots simples pour la lecture.";
	Resultat rt = analyser(txt, Reglages{}, L"note.md");
	VERIFIE(!rt.estHtml);
	VERIFIE(trouver(rt, L"Titres Markdown") != nullptr);

	std::string html = versUtf8(genererRapport(r));
	std::ofstream(argv[2], std::ios::binary) << html;
	VERIFIE(html.find("location") != std::string::npos);

	std::printf(g_echecs ? "%d echec(s)\n" : "Tous les tests passent\n", g_echecs);
	return g_echecs ? 1 : 0;
}
