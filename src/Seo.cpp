#include "Seo.h"
#include <algorithm>
#include <cwctype>
#include <cstdint>

namespace seo {

// ---- Chaînes ----------------------------------------------------------------

static std::wstring trim(const std::wstring &s) {
	size_t a = s.find_first_not_of(L" \t\r\n\f\v");
	if (a == std::wstring::npos) return L"";
	size_t b = s.find_last_not_of(L" \t\r\n\f\v");
	return s.substr(a, b - a + 1);
}

static std::wstring minuscules(const std::wstring &s) {
	std::wstring r = s;
	for (auto &c : r) c = towlower(c);
	return r;
}

// Minuscules sans accents, pour comparer les mots.
static std::wstring normaliser(const std::wstring &s) {
	static const wchar_t *de = L"ÀÂÄàâäÇçÉÈÊËéèêëÎÏîïÔÖôöÙÛÜùûüŒœ";
	static const wchar_t *vers = L"aaaaaacceeeeeeeeiiiioooouuuuuuoo";
	std::wstring r;
	r.reserve(s.size());
	for (wchar_t c : s) {
		if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
		else if (const wchar_t *p = wcschr(de, c)) c = vers[p - de];
		r += c;
	}
	return r;
}

std::string versUtf8(const std::wstring &s) {
	std::string r;
	r.reserve(s.size() + s.size() / 4);
	for (size_t i = 0; i < s.size(); ++i) {
		uint32_t c = s[i];
		if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) {
			uint32_t c2 = s[i + 1];
			if (c2 >= 0xDC00 && c2 <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00); ++i; }
		}
		if (c < 0x80) r += (char)c;
		else if (c < 0x800) { r += (char)(0xC0 | (c >> 6)); r += (char)(0x80 | (c & 0x3F)); }
		else if (c < 0x10000) { r += (char)(0xE0 | (c >> 12)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
		else { r += (char)(0xF0 | (c >> 18)); r += (char)(0x80 | ((c >> 12) & 0x3F)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
	}
	return r;
}

std::wstring depuisUtf8(const std::string &s) {
	std::wstring r;
	size_t i = 0;
	while (i < s.size()) {
		uint32_t c = (unsigned char)s[i];
		int n = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
		if (n == 0) { r += (wchar_t)c; ++i; continue; }
		c &= (0x3F >> n);
		if (i + n >= s.size()) break;
		for (int k = 0; k < n; ++k) c = (c << 6) | ((unsigned char)s[i + 1 + k] & 0x3F);
		i += n + 1;
		if (c >= 0x10000) { c -= 0x10000; r += (wchar_t)(0xD800 + (c >> 10)); r += (wchar_t)(0xDC00 + (c & 0x3FF)); }
		else r += (wchar_t)c;
	}
	return r;
}

std::wstring libelleNiveau(Niveau n) {
	switch (n) {
	case Niveau::Ok: return L"OK";
	case Niveau::Info: return L"Info";
	case Niveau::Avert: return L"À revoir";
	default: return L"Problème";
	}
}

// Décode les entités HTML les plus courantes.
static std::wstring decoderEntites(const std::wstring &s) {
	struct { const wchar_t *nom; wchar_t c; } table[] = {
	    {L"amp", L'&'}, {L"lt", L'<'}, {L"gt", L'>'}, {L"quot", L'"'}, {L"apos", L'\''},
	    {L"nbsp", L' '}, {L"eacute", L'é'}, {L"egrave", L'è'}, {L"agrave", L'à'}, {L"ccedil", L'ç'},
	    {L"ecirc", L'ê'}, {L"ocirc", L'ô'}, {L"icirc", L'î'}, {L"ucirc", L'û'}, {L"acirc", L'â'},
	    {L"euro", L'€'}, {L"laquo", L'«'}, {L"raquo", L'»'}, {L"hellip", L'…'}, {L"rsquo", L'\''}};
	std::wstring r;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] != L'&') { r += s[i]; continue; }
		size_t f = s.find(L';', i);
		if (f == std::wstring::npos || f - i > 10) { r += s[i]; continue; }
		std::wstring ent = s.substr(i + 1, f - i - 1);
		if (!ent.empty() && ent[0] == L'#') {
			int code = ent[1] == L'x' || ent[1] == L'X' ? wcstol(ent.c_str() + 2, nullptr, 16)
			                                            : _wtoi(ent.c_str() + 1);
			if (code > 0) { r += (wchar_t)code; i = f; continue; }
		}
		bool trouve = false;
		for (auto &e : table)
			if (ent == e.nom) { r += e.c; i = f; trouve = true; break; }
		if (!trouve) r += s[i];
	}
	return r;
}

bool ressembleHtml(const std::wstring &c) {
	std::wstring n = minuscules(c);
	const wchar_t *marqueurs[] = {L"<html", L"<body", L"<head", L"<!doctype", L"<p>", L"<div", L"<title"};
	for (auto m : marqueurs)
		if (n.find(m) != std::wstring::npos) return true;
	return false;
}

// ---- Réglages (ligne d'en-tête) ---------------------------------------------

Reglages lireReglages(const std::wstring &contenu) {
	Reglages r;
	std::wstring n = minuscules(contenu);
	size_t p = n.find(L"seo:");
	if (p == std::wstring::npos) return r;
	size_t fin = contenu.find_first_of(L"\r\n", p);
	std::wstring ligne = contenu.substr(p + 4, (fin == std::wstring::npos ? contenu.size() : fin) - p - 4);
	size_t ferme = ligne.find(L"-->");
	if (ferme != std::wstring::npos) ligne = ligne.substr(0, ferme);
	// découpe par ; ou |
	size_t pos = 0;
	while (pos < ligne.size()) {
		size_t sep = ligne.find_first_of(L";|", pos);
		std::wstring paire = ligne.substr(pos, sep == std::wstring::npos ? std::wstring::npos : sep - pos);
		size_t eq = paire.find(L'=');
		if (eq != std::wstring::npos) {
			std::wstring cle = normaliser(trim(paire.substr(0, eq)));
			std::wstring val = trim(paire.substr(eq + 1));
			if (cle == L"motcle" || cle == L"mot-cle" || cle == L"keyword" || cle == L"kw" || cle == L"cible")
				r.motcle = val;
			else if (cle == L"domaine" || cle == L"domain" || cle == L"site" || cle == L"url")
				r.domaine = minuscules(val);
		}
		if (sep == std::wstring::npos) break;
		pos = sep + 1;
	}
	// nettoie le domaine (enlève protocole et chemin)
	std::wstring &d = r.domaine;
	size_t pp = d.find(L"//");
	if (pp != std::wstring::npos) d = d.substr(pp + 2);
	size_t sl = d.find(L'/');
	if (sl != std::wstring::npos) d = d.substr(0, sl);
	if (d.compare(0, 4, L"www.") == 0) d = d.substr(4);
	return r;
}

// ---- Extraction HTML --------------------------------------------------------

struct Element { std::wstring titre, metaDesc, texte; };

// Contenu d'une balise paire : <tag ...>contenu</tag> (première occurrence).
static std::wstring contenuBalise(const std::wstring &bas, const std::wstring &html, const wchar_t *tag) {
	std::wstring ouv = std::wstring(L"<") + tag;
	size_t p = bas.find(ouv);
	if (p == std::wstring::npos) return L"";
	size_t sup = bas.find(L'>', p);
	if (sup == std::wstring::npos) return L"";
	size_t ferme = bas.find(std::wstring(L"</") + tag, sup);
	if (ferme == std::wstring::npos) return L"";
	return trim(html.substr(sup + 1, ferme - sup - 1));
}

// Valeur d'un attribut dans un fragment de balise.
static std::wstring attribut(const std::wstring &balise, const wchar_t *nom) {
	std::wstring b = minuscules(balise);
	std::wstring cle = std::wstring(nom) + L"=";
	size_t p = b.find(cle);
	if (p == std::wstring::npos) return L"";
	// vérifie que c'est bien le début d'un nom d'attribut
	if (p > 0 && (iswalnum(b[p - 1]) || b[p - 1] == L'-')) {
		do {
			p = b.find(cle, p + 1);
		} while (p != std::wstring::npos && p > 0 && (iswalnum(b[p - 1]) || b[p - 1] == L'-'));
		if (p == std::wstring::npos) return L"";
	}
	size_t v = p + cle.size();
	if (v >= balise.size()) return L"";
	wchar_t q = balise[v];
	if (q == L'"' || q == L'\'') {
		size_t f = balise.find(q, v + 1);
		return f == std::wstring::npos ? L"" : balise.substr(v + 1, f - v - 1);
	}
	size_t f = balise.find_first_of(L" \t\r\n>", v);
	return balise.substr(v, f == std::wstring::npos ? std::wstring::npos : f - v);
}

// Toutes les balises <tag ...> (fragment d'ouverture, sans le contenu).
static std::vector<std::wstring> toutesBalises(const std::wstring &html, const wchar_t *tag) {
	std::vector<std::wstring> r;
	std::wstring bas = minuscules(html);
	std::wstring ouv = std::wstring(L"<") + tag;
	size_t p = 0;
	while ((p = bas.find(ouv, p)) != std::wstring::npos) {
		wchar_t apres = p + ouv.size() < bas.size() ? bas[p + ouv.size()] : L' ';
		if (iswalpha(apres)) { p += ouv.size(); continue; } // ex. <a vs <article
		size_t f = html.find(L'>', p);
		if (f == std::wstring::npos) break;
		r.push_back(html.substr(p, f - p + 1));
		p = f + 1;
	}
	return r;
}

// Supprime scripts, styles et balises ; renvoie le texte visible.
static std::wstring texteVisible(const std::wstring &html) {
	std::wstring bas = minuscules(html), out;
	auto retirerBloc = [&](std::wstring &h, const wchar_t *tag) {
		std::wstring b = minuscules(h), o;
		std::wstring deb = std::wstring(L"<") + tag, fin = std::wstring(L"</") + tag;
		size_t p = 0;
		while (p < h.size()) {
			size_t d = b.find(deb, p);
			if (d == std::wstring::npos) { o += h.substr(p); break; }
			o += h.substr(p, d - p);
			size_t f = b.find(fin, d);
			if (f == std::wstring::npos) break;
			f = b.find(L'>', f);
			p = f == std::wstring::npos ? h.size() : f + 1;
		}
		h = o;
	};
	std::wstring h = html;
	retirerBloc(h, L"script");
	retirerBloc(h, L"style");
	// retire les balises
	bool dans = false;
	for (wchar_t c : h) {
		if (c == L'<') dans = true;
		else if (c == L'>') { dans = false; out += L' '; }
		else if (!dans) out += c;
	}
	return decoderEntites(out);
}

// ---- Mots et lisibilité -----------------------------------------------------

static std::vector<std::wstring> enMots(const std::wstring &texte) {
	std::vector<std::wstring> mots;
	std::wstring cur;
	for (wchar_t c : texte) {
		if (iswalnum(c) || c == L'-' || c == L'\'') cur += c;
		else if (!cur.empty()) { mots.push_back(cur); cur.clear(); }
	}
	if (!cur.empty()) mots.push_back(cur);
	return mots;
}

// Occurrences du mot-clé (expression entière, insensible casse/accents) dans le texte.
static int compterMotcle(const std::wstring &texte, const std::wstring &motcle) {
	std::wstring t = normaliser(texte), m = normaliser(trim(motcle));
	if (m.empty()) return 0;
	int n = 0;
	size_t p = 0;
	while ((p = t.find(m, p)) != std::wstring::npos) {
		bool avant = p == 0 || !iswalnum(t[p - 1]);
		bool apres = p + m.size() >= t.size() || !iswalnum(t[p + m.size()]);
		if (avant && apres) ++n;
		p += m.size();
	}
	return n;
}

// ---- Analyse ----------------------------------------------------------------

static void ajouter(Resultat &r, const wchar_t *cat, const std::wstring &lib, const std::wstring &val,
                    Niveau niv, const std::wstring &conseil = L"") {
	r.controles.push_back({cat, lib, val, conseil, niv});
}

static std::wstring nb(int n) { return std::to_wstring(n); }

Resultat analyser(const std::wstring &contenu, const Reglages &reg, const std::wstring &source) {
	Resultat r;
	r.source = source;
	r.motcle = reg.motcle;
	r.estHtml = ressembleHtml(contenu);
	std::wstring bas = minuscules(contenu);

	std::wstring texte;
	std::wstring titre, meta;
	if (r.estHtml) {
		titre = decoderEntites(contenuBalise(bas, contenu, L"title"));
		for (auto &m : toutesBalises(contenu, L"meta")) {
			std::wstring n = minuscules(attribut(m, L"name"));
			if (n == L"description") { meta = decoderEntites(attribut(m, L"content")); break; }
		}
		texte = texteVisible(contenu);
	} else {
		// texte / markdown : 1re ligne non vide = titre présumé
		texte = contenu;
		size_t p = 0;
		while (p < contenu.size()) {
			size_t f = contenu.find_first_of(L"\r\n", p);
			std::wstring l = trim(contenu.substr(p, f == std::wstring::npos ? std::wstring::npos : f - p));
			if (!l.empty() && l.find(L"seo:") == std::wstring::npos) {
				while (!l.empty() && l[0] == L'#') l.erase(0, 1);
				titre = trim(l);
				break;
			}
			if (f == std::wstring::npos) break;
			p = f + 1;
		}
	}

	auto mots = enMots(texte);
	r.nbMots = (int)mots.size();

	// --- Title ---
	if (titre.empty())
		ajouter(r, L"Title", L"Balise title", L"absente", Niveau::Alerte,
		        L"Ajoutez une balise <title> : c'est le facteur on-page le plus visible.");
	else {
		int n = (int)titre.size();
		Niveau niv = (n >= 30 && n <= 60) ? Niveau::Ok : (n < 30 ? Niveau::Avert : Niveau::Avert);
		std::wstring conseil = n < 30 ? L"Un peu court : visez 30 à 60 caractères."
		                     : n > 60 ? L"Un peu long : risque de troncature dans Google (visez ≤ 60)."
		                              : L"";
		ajouter(r, L"Title", L"Longueur du title", nb(n) + L" car. : « " + titre + L" »", niv, conseil);
	}

	// --- Meta description ---
	if (r.estHtml) {
		if (meta.empty())
			ajouter(r, L"Meta", L"Meta description", L"absente", Niveau::Alerte,
			        L"Ajoutez <meta name=\"description\" content=\"…\"> (70 à 160 caractères).");
		else {
			int n = (int)meta.size();
			Niveau niv = (n >= 70 && n <= 160) ? Niveau::Ok : Niveau::Avert;
			std::wstring conseil = n < 70 ? L"Trop courte : visez 70 à 160 caractères."
			                     : n > 160 ? L"Trop longue : sera tronquée (visez ≤ 160)." : L"";
			ajouter(r, L"Meta", L"Longueur de la meta description", nb(n) + L" car.", niv, conseil);
		}
	}

	// --- Structure Hn ---
	if (r.estHtml) {
		int h[7] = {0};
		for (int i = 1; i <= 6; ++i) {
			wchar_t t[3] = {L'h', (wchar_t)(L'0' + i), 0};
			h[i] = (int)toutesBalises(contenu, t).size();
		}
		std::wstring rep = L"H1:" + nb(h[1]) + L" H2:" + nb(h[2]) + L" H3:" + nb(h[3]);
		if (h[4] + h[5] + h[6]) rep += L" H4:" + nb(h[4]) + L" H5:" + nb(h[5]) + L" H6:" + nb(h[6]);
		if (h[1] == 1) ajouter(r, L"Structure", L"Titres Hn", rep, h[2] ? Niveau::Ok : Niveau::Avert,
		                       h[2] ? L"" : L"Aucun H2 : structurez le contenu en sous-titres.");
		else ajouter(r, L"Structure", L"Titres Hn", rep, Niveau::Alerte,
		             h[1] == 0 ? L"Aucun H1 : chaque page doit avoir un titre H1 unique."
		                       : L"Plusieurs H1 : gardez-en un seul.");
	} else {
		int h1 = 0, h2 = 0;
		size_t p = 0;
		while (p < contenu.size()) {
			size_t f = contenu.find_first_of(L"\r\n", p);
			std::wstring l = trim(contenu.substr(p, f == std::wstring::npos ? std::wstring::npos : f - p));
			if (l.compare(0, 2, L"# ") == 0) ++h1;
			else if (l.compare(0, 3, L"## ") == 0) ++h2;
			if (f == std::wstring::npos) break;
			p = f + 1;
		}
		if (h1 || h2)
			ajouter(r, L"Structure", L"Titres Markdown", L"# :" + nb(h1) + L"  ## :" + nb(h2),
			        h1 == 1 ? Niveau::Ok : Niveau::Avert,
			        h1 == 1 ? L"" : L"Un seul titre de niveau 1 (#) est recommandé.");
	}

	// --- Mot-clé ---
	if (reg.motcle.empty())
		ajouter(r, L"Mot-clé", L"Mot-clé cible", L"non défini", Niveau::Info,
		        L"Ajoutez en tête : <!-- seo: motcle=votre expression -->");
	else {
		int occ = compterMotcle(texte, reg.motcle);
		double densite = r.nbMots ? 100.0 * occ / r.nbMots : 0;
		wchar_t d[16];
		swprintf(d, 16, L"%.1f", densite);
		Niveau niv = occ == 0 ? Niveau::Alerte : (densite >= 0.5 && densite <= 3.0) ? Niveau::Ok : Niveau::Avert;
		std::wstring conseil = occ == 0 ? L"Le mot-clé n'apparaît pas dans le texte."
		                     : densite > 3.0 ? L"Densité élevée : risque de sur-optimisation (visez 0,5 à 3 %)."
		                     : densite < 0.5 ? L"Densité faible : employez le mot-clé un peu plus souvent." : L"";
		ajouter(r, L"Mot-clé", L"Présence de « " + reg.motcle + L" »", nb(occ) + L" fois (" + d + L" %)", niv, conseil);
		bool dansTitre = compterMotcle(titre, reg.motcle) > 0;
		ajouter(r, L"Mot-clé", L"Mot-clé dans le title", dansTitre ? L"oui" : L"non",
		        dansTitre ? Niveau::Ok : Niveau::Avert,
		        dansTitre ? L"" : L"Placez le mot-clé dans le title, idéalement au début.");
		if (r.estHtml && !meta.empty())
			ajouter(r, L"Mot-clé", L"Mot-clé dans la meta description",
			        compterMotcle(meta, reg.motcle) > 0 ? L"oui" : L"non",
			        compterMotcle(meta, reg.motcle) > 0 ? Niveau::Ok : Niveau::Avert,
			        compterMotcle(meta, reg.motcle) > 0 ? L"" : L"Reprenez le mot-clé dans la meta description.");
	}

	// --- Lisibilité : mots par phrase (phrase = fin sur . ! ?) ---
	int phrases = 0, longues = 0, maxPhrase = 0;
	{
		int m = 0;
		bool dansMot = false;
		for (size_t i = 0; i <= texte.size(); ++i) {
			wchar_t c = i < texte.size() ? texte[i] : L'.';
			if (iswalnum(c)) { if (!dansMot) { ++m; dansMot = true; } }
			else {
				dansMot = false;
				if (c == L'.' || c == L'!' || c == L'?' || i == texte.size()) {
					if (m > 0) { ++phrases; maxPhrase = std::max(maxPhrase, m); if (m > 25) ++longues; m = 0; }
				}
			}
		}
	}
	if (phrases > 0) {
		int moy = r.nbMots / phrases;
		Niveau niv = moy <= 20 ? Niveau::Ok : moy <= 25 ? Niveau::Avert : Niveau::Alerte;
		ajouter(r, L"Lisibilité", L"Longueur moyenne des phrases", nb(moy) + L" mots (max " + nb(maxPhrase) + L")",
		        niv, moy > 20 ? L"Phrases longues : raccourcissez pour faciliter la lecture." : L"");
		if (longues)
			ajouter(r, L"Lisibilité", L"Phrases de plus de 25 mots", nb(longues),
			        longues * 4 > phrases ? Niveau::Avert : Niveau::Info,
			        longues * 4 > phrases ? L"Beaucoup de phrases longues : scindez-les." : L"");
	}

	// --- Volume de contenu ---
	Niveau nivMots = r.nbMots >= 300 ? Niveau::Ok : r.nbMots >= 150 ? Niveau::Avert : Niveau::Alerte;
	ajouter(r, L"Contenu", L"Nombre de mots", nb(r.nbMots), nivMots,
	        r.nbMots < 300 ? L"Contenu court : au moins 300 mots pour un article de fond." : L"");

	// --- Ratio texte/code (HTML uniquement) ---
	if (r.estHtml) {
		size_t carTexte = trim(texte).size();
		size_t carTotal = std::max<size_t>(1, contenu.size());
		int ratio = (int)(100 * carTexte / carTotal);
		Niveau niv = ratio >= 15 ? Niveau::Ok : ratio >= 8 ? Niveau::Avert : Niveau::Alerte;
		ajouter(r, L"Contenu", L"Ratio texte / code", nb(ratio) + L" %", niv,
		        ratio < 15 ? L"Peu de texte pour beaucoup de HTML : enrichissez le contenu ou allégez le balisage."
		               : L"");
	}

	// --- Liens & images (HTML) ---
	if (r.estHtml) {
		int interne = 0, externe = 0, sansTexte = 0;
		for (auto &a : toutesBalises(contenu, L"a")) {
			std::wstring href = minuscules(attribut(a, L"href"));
			if (href.empty() || href[0] == L'#' || href.compare(0, 11, L"javascript:") == 0) continue;
			bool abs = href.compare(0, 4, L"http") == 0 || href.compare(0, 2, L"//") == 0;
			if (!abs) ++interne;
			else if (!reg.domaine.empty() && href.find(reg.domaine) != std::wstring::npos) ++interne;
			else ++externe;
		}
		ajouter(r, L"Liens", L"Liens internes / externes", nb(interne) + L" / " + nb(externe),
		        interne > 0 ? Niveau::Ok : Niveau::Avert,
		        interne == 0 ? L"Aucun lien interne : reliez vers vos autres pages." : L"");

		int img = 0, sansAlt = 0;
		for (auto &im : toutesBalises(contenu, L"img")) {
			++img;
			if (trim(attribut(im, L"alt")).empty()) ++sansAlt;
		}
		if (img > 0)
			ajouter(r, L"Images", L"Images sans attribut alt", nb(sansAlt) + L" sur " + nb(img),
			        sansAlt == 0 ? Niveau::Ok : Niveau::Avert,
			        sansAlt ? L"Renseignez l'attribut alt (accessibilité et SEO images)." : L"");
		else
			ajouter(r, L"Images", L"Images", L"aucune", Niveau::Info, L"");
		(void)sansTexte;
	}

	// --- Score ---
	int score = 100;
	for (auto &c : r.controles) {
		if (c.niveau == Niveau::Info) continue;
		if (c.niveau == Niveau::Avert) score -= 7;
		else if (c.niveau == Niveau::Alerte) score -= 15;
	}
	r.score = std::max(0, std::min(100, score));
	return r;
}

} // namespace seo
