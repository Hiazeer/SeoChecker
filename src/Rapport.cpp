// Rapport SEO en HTML autonome.
#include "Seo.h"
#include <algorithm>

namespace seo {

static std::wstring echapper(const std::wstring &s) {
	std::wstring r;
	for (wchar_t c : s)
		switch (c) {
		case L'&': r += L"&amp;"; break;
		case L'<': r += L"&lt;"; break;
		case L'>': r += L"&gt;"; break;
		case L'"': r += L"&quot;"; break;
		default: r += c;
		}
	return r;
}

static const wchar_t *classe(Niveau n) {
	switch (n) {
	case Niveau::Ok: return L"ok";
	case Niveau::Info: return L"info";
	case Niveau::Avert: return L"warn";
	default: return L"err";
	}
}

std::wstring genererRapport(const Resultat &r) {
	std::wstring h;
	h += L"<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\">"
	     L"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>SEO — ";
	h += echapper(r.source) + L"</title><style>"
	     L":root{--bg:#fff;--fg:#1f2937;--mut:#6b7280;--cd:#f3f4f6;--ln:#e5e7eb;"
	     L"--ok:#16a34a;--info:#6b7280;--warn:#d97706;--err:#dc2626}"
	     L"@media(prefers-color-scheme:dark){:root{--bg:#0f172a;--fg:#e2e8f0;--mut:#94a3b8;--cd:#1e293b;--ln:#334155}}"
	     L"*{box-sizing:border-box}body{margin:0;padding:28px;max-width:860px;margin:auto;"
	     L"font:15px/1.5 'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--fg)}"
	     L"h1{font-size:22px;margin:0 0 2px}.src{color:var(--mut);margin:0 0 18px;word-break:break-all}"
	     L".top{display:flex;gap:24px;align-items:center;flex-wrap:wrap;margin-bottom:22px}"
	     L".gauge{--v:0;width:92px;height:92px;border-radius:50%;flex:none;"
	     L"background:conic-gradient(var(--sc) calc(var(--v)*1%),var(--ln) 0);display:grid;place-items:center;position:relative}"
	     L".gauge::before{content:'';position:absolute;inset:9px;border-radius:50%;background:var(--bg)}"
	     L".gauge b{position:relative;font-size:26px}"
	     L".facts{color:var(--mut);font-size:14px}.facts b{color:var(--fg)}"
	     L"section{margin:20px 0}h2{font-size:13px;text-transform:uppercase;letter-spacing:.05em;color:var(--mut);"
	     L"border-bottom:1px solid var(--ln);padding-bottom:6px;margin:0 0 10px}"
	     L".line{display:flex;gap:12px;padding:9px 0;border-bottom:1px solid var(--ln)}"
	     L".line:last-child{border:0}.dot{flex:none;width:10px;height:10px;border-radius:50%;margin-top:6px}"
	     L".ok .dot{background:var(--ok)}.info .dot{background:var(--info)}.warn .dot{background:var(--warn)}.err .dot{background:var(--err)}"
	     L".txt{flex:1}.lib{font-weight:500}.val{color:var(--mut);font-size:14px}"
	     L".adv{font-size:13.5px;margin-top:3px;padding:6px 10px;border-radius:6px;background:var(--cd)}"
	     L".ok .adv{display:none}footer{margin-top:26px;color:var(--mut);font-size:12px}"
	     L"</style></head><body>";

	const wchar_t *couleur = r.score >= 80 ? L"var(--ok)" : r.score >= 50 ? L"var(--warn)" : L"var(--err)";
	h += L"<h1>Analyse SEO</h1><p class=\"src\">" + echapper(r.source) + L"</p>";
	h += L"<div class=\"top\"><div class=\"gauge\" style=\"--v:" + std::to_wstring(r.score) + L";--sc:" + couleur +
	     L"\"><b>" + std::to_wstring(r.score) + L"</b></div><div class=\"facts\">";
	h += L"Score global sur 100<br>Type : <b>" + std::wstring(r.estHtml ? L"HTML" : L"texte / Markdown") + L"</b>";
	h += L"<br>Mots : <b>" + std::to_wstring(r.nbMots) + L"</b>";
	if (!r.motcle.empty()) h += L"<br>Mot-clé : <b>" + echapper(r.motcle) + L"</b>";
	h += L"</div></div>";

	// Regroupe par catégorie dans l'ordre d'apparition.
	std::vector<std::wstring> ordre;
	for (auto &c : r.controles)
		if (std::find(ordre.begin(), ordre.end(), c.categorie) == ordre.end()) ordre.push_back(c.categorie);
	for (auto &cat : ordre) {
		h += L"<section><h2>" + echapper(cat) + L"</h2>";
		for (auto &c : r.controles) {
			if (c.categorie != cat) continue;
			h += L"<div class=\"line " + std::wstring(classe(c.niveau)) + L"\"><span class=\"dot\"></span><div class=\"txt\">";
			h += L"<div class=\"lib\">" + echapper(c.libelle) + L"</div>";
			h += L"<div class=\"val\">" + echapper(c.valeur) + L"</div>";
			if (!c.conseil.empty()) h += L"<div class=\"adv\">💡 " + echapper(c.conseil) + L"</div>";
			h += L"</div></div>";
		}
		h += L"</section>";
	}

	h += L"<footer>Généré par SEO Checker pour Notepad++</footer></body></html>";
	return h;
}

} // namespace seo
