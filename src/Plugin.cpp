// SEO Checker — plugin Notepad++ d'analyse SEO on-page.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0601
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "NppPlugin.h"
#include "Seo.h"
#include "Fetch.h"

using namespace seo;

static const wchar_t NOM_PLUGIN[] = L"SEO Checker";
static const wchar_t NOM_MODULE[] = L"SeoChecker.dll";

static HINSTANCE g_hInst;
static NppData g_npp;
static Resultat g_dernier;
static bool g_aResultat = false;
static std::wstring g_derniereUrl;

// ---- Scintilla / Notepad++ --------------------------------------------------

static HWND sci() {
	int vue = -1;
	SendMessage(g_npp._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&vue);
	return vue == 1 ? g_npp._scintillaSecondHandle : g_npp._scintillaMainHandle;
}
static LRESULT S(UINT m, WPARAM w = 0, LPARAM l = 0) { return SendMessage(sci(), m, w, l); }

static std::wstring texteDocument() {
	intptr_t len = S(SCI_GETLENGTH);
	std::string buf(len + 1, '\0');
	S(SCI_GETTEXT, len + 1, (LPARAM)&buf[0]);
	buf.resize(len);
	UINT cp = S(SCI_GETCODEPAGE) == SC_CP_UTF8 ? CP_UTF8 : CP_ACP;
	int n = MultiByteToWideChar(cp, 0, buf.data(), (int)buf.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(cp, 0, buf.data(), (int)buf.size(), &w[0], n);
	return w;
}

static std::wstring nomFichier() {
	wchar_t nom[MAX_PATH] = L"";
	SendMessage(g_npp._nppHandle, NPPM_GETNAMEPART, MAX_PATH, (LPARAM)nom);
	return nom[0] ? nom : L"(document sans nom)";
}

static std::wstring finDeLigne() {
	switch (S(SCI_GETEOLMODE)) {
	case SC_EOL_CRLF: return L"\r\n";
	case SC_EOL_CR: return L"\r";
	default: return L"\n";
	}
}

// ---- Petite boîte de saisie (modale) ----------------------------------------

static std::wstring g_saisieVal;
static bool g_saisieOk;
static HWND g_edit;

static LRESULT CALLBACK procSaisie(HWND h, UINT m, WPARAM w, LPARAM l) {
	switch (m) {
	case WM_COMMAND:
		if (LOWORD(w) == IDOK) {
			int n = GetWindowTextLengthW(g_edit);
			std::wstring s(n + 1, L'\0');
			GetWindowTextW(g_edit, &s[0], n + 1);
			s.resize(n);
			g_saisieVal = s;
			g_saisieOk = true;
			DestroyWindow(h);
		} else if (LOWORD(w) == IDCANCEL) {
			DestroyWindow(h);
		}
		return 0;
	case WM_CLOSE: DestroyWindow(h); return 0;
	case WM_DESTROY: PostQuitMessage(0); return 0;
	}
	return DefWindowProc(h, m, w, l);
}

static int ech(int v) {
	static int dpi = 0;
	if (!dpi) { HDC dc = GetDC(nullptr); dpi = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(nullptr, dc); }
	return MulDiv(v, dpi, 96);
}

// Renvoie le texte saisi, ou chaîne vide si annulé.
static std::wstring demanderTexte(const wchar_t *titre, const wchar_t *invite, const std::wstring &initial) {
	static bool enreg = false;
	const wchar_t *cls = L"SeoCheckerSaisie";
	if (!enreg) {
		WNDCLASSW wc = {};
		wc.lpfnWndProc = procSaisie;
		wc.hInstance = g_hInst;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
		wc.lpszClassName = cls;
		RegisterClassW(&wc);
		enreg = true;
	}
	g_saisieOk = false;
	g_saisieVal.clear();

	int W = ech(420), H = ech(140);
	RECT rp;
	GetWindowRect(g_npp._nppHandle, &rp);
	int x = rp.left + ((rp.right - rp.left) - W) / 2, y = rp.top + ((rp.bottom - rp.top) - H) / 2;
	HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, cls, titre, WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, W, H,
	                           g_npp._nppHandle, nullptr, g_hInst, nullptr);

	NONCLIENTMETRICSW ncm = {sizeof(ncm)};
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
	HFONT police = CreateFontIndirectW(&ncm.lfMessageFont);

	HWND lbl = CreateWindowExW(0, L"STATIC", invite, WS_CHILD | WS_VISIBLE, ech(12), ech(10), W - ech(30), ech(20),
	                           dlg, nullptr, g_hInst, nullptr);
	g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initial.c_str(),
	                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, ech(12), ech(34), W - ech(36),
	                         ech(24), dlg, nullptr, g_hInst, nullptr);
	HWND ok = CreateWindowExW(0, L"BUTTON", L"Analyser", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
	                          W - ech(200), ech(70), ech(90), ech(26), dlg, (HMENU)IDOK, g_hInst, nullptr);
	HWND annul = CreateWindowExW(0, L"BUTTON", L"Annuler", WS_CHILD | WS_VISIBLE | WS_TABSTOP, W - ech(104), ech(70),
	                             ech(90), ech(26), dlg, (HMENU)IDCANCEL, g_hInst, nullptr);
	for (HWND c : {lbl, g_edit, ok, annul}) SendMessage(c, WM_SETFONT, (WPARAM)police, TRUE);

	ShowWindow(dlg, SW_SHOW);
	SetFocus(g_edit);
	SendMessage(g_edit, EM_SETSEL, 0, -1);
	EnableWindow(g_npp._nppHandle, FALSE);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { SendMessage(dlg, WM_COMMAND, IDOK, 0); continue; }
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { SendMessage(dlg, WM_COMMAND, IDCANCEL, 0); continue; }
		if (!IsDialogMessage(dlg, &msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
	}
	EnableWindow(g_npp._nppHandle, TRUE);
	SetForegroundWindow(g_npp._nppHandle);
	DeleteObject(police);
	return g_saisieOk ? g_saisieVal : L"";
}

// ---- Panneau ----------------------------------------------------------------

enum { ID_LISTE = 100, ID_RESUME, ID_B_FICHIER, ID_B_URL, ID_B_RAPPORT, ID_B_ENTETE };
static HWND g_panneau, g_resume, g_liste, g_boutons[4];
static HFONT g_police;
static bool g_visible = false, g_enregistre = false;
static tTbData g_tb;
static int g_cmdPanneau = 0;
static std::vector<COLORREF> g_couleurs;

static void cocherMenu();

static void remplirPanneau() {
	if (!g_panneau) return;
	const Resultat &r = g_dernier;
	std::wstring res;
	if (!g_aResultat) {
		res = L"Cliquez « Fichier » pour analyser le document courant, ou « URL » pour une page en ligne.\r\n\r\n"
		      L"Astuce : placez en tête de votre fichier\r\n<!-- seo: motcle=votre expression | domaine=monsite.fr -->";
		SetWindowTextW(g_resume, res.c_str());
		SendMessage(g_liste, LVM_DELETEALLITEMS, 0, 0);
		g_couleurs.clear();
		return;
	}
	res = L"Source : " + r.source + L"\r\n";
	res += L"Score : " + std::to_wstring(r.score) + L" / 100";
	res += r.score >= 80 ? L"  ✔ bon" : r.score >= 50 ? L"  ~ à améliorer" : L"  ✗ à retravailler";
	res += L"\r\n";
	res += std::wstring(L"Type : ") + (r.estHtml ? L"HTML" : L"texte / Markdown") + L"   ·   Mots : " +
	       std::to_wstring(r.nbMots);
	if (!r.motcle.empty()) res += L"   ·   Mot-clé : " + r.motcle;
	int warn = 0, err = 0;
	for (auto &c : r.controles) {
		if (c.niveau == Niveau::Avert) ++warn;
		else if (c.niveau == Niveau::Alerte) ++err;
	}
	res += L"\r\n" + std::to_wstring(err) + L" problème(s), " + std::to_wstring(warn) + L" point(s) à revoir.";
	SetWindowTextW(g_resume, res.c_str());

	SendMessage(g_liste, WM_SETREDRAW, FALSE, 0);
	SendMessage(g_liste, LVM_DELETEALLITEMS, 0, 0);
	g_couleurs.clear();
	const COLORREF VERT = RGB(22, 140, 60), ORANGE = RGB(200, 110, 0), ROUGE = RGB(200, 30, 30), GRIS = RGB(130, 130, 130);
	for (auto &c : r.controles) {
		LVITEMW it = {};
		it.mask = LVIF_TEXT;
		it.iItem = (int)g_couleurs.size();
		std::wstring col0 = c.categorie;
		it.pszText = const_cast<wchar_t *>(col0.c_str());
		int idx = (int)SendMessage(g_liste, LVM_INSERTITEMW, 0, (LPARAM)&it);
		std::wstring val = c.valeur;
		if (!c.conseil.empty()) val += L"  — " + c.conseil;
		const std::wstring *cols[] = {&c.libelle, &val, nullptr};
		std::wstring niv = libelleNiveau(c.niveau);
		const std::wstring *tout[] = {&c.libelle, &val, &niv};
		for (int k = 0; k < 3; ++k) {
			LVITEMW s = {};
			s.iSubItem = k + 1;
			s.pszText = const_cast<wchar_t *>(tout[k]->c_str());
			SendMessage(g_liste, LVM_SETITEMTEXTW, idx, (LPARAM)&s);
		}
		(void)cols;
		COLORREF cc = c.niveau == Niveau::Ok ? VERT : c.niveau == Niveau::Avert ? ORANGE
		            : c.niveau == Niveau::Alerte ? ROUGE : GRIS;
		g_couleurs.push_back(cc);
	}
	SendMessage(g_liste, WM_SETREDRAW, TRUE, 0);
}

static void analyserFichier() {
	std::wstring contenu = texteDocument();
	Reglages reg = lireReglages(contenu);
	g_dernier = analyser(contenu, reg, nomFichier());
	g_aResultat = true;
	remplirPanneau();
}

static void ouvrirRapport();

static void analyserUrl() {
	std::wstring url = demanderTexte(L"Analyser une URL", L"Adresse de la page à analyser :",
	                                 g_derniereUrl.empty() ? L"https://" : g_derniereUrl);
	if (url.empty()) return;
	g_derniereUrl = url;
	HCURSOR anc = SetCursor(LoadCursor(nullptr, IDC_WAIT));
	Telechargement t = telecharger(url);
	SetCursor(anc);
	if (!t.ok) {
		MessageBoxW(g_npp._nppHandle, t.erreur.c_str(), NOM_PLUGIN, MB_ICONWARNING);
		return;
	}
	// mot-clé : en-tête de la page téléchargée, sinon en-tête du document courant
	Reglages reg = lireReglages(t.html);
	if (reg.motcle.empty()) {
		Reglages local = lireReglages(texteDocument());
		if (!reg.domaine.empty()) local.domaine = reg.domaine;
		reg.motcle = local.motcle;
		if (reg.domaine.empty()) reg.domaine = local.domaine;
	}
	g_dernier = analyser(t.html, reg, url);
	g_aResultat = true;
	if (!g_visible) { /* ouvre le panneau pour montrer le résultat */ }
	remplirPanneau();
	ouvrirRapport(); // pour une URL, on ouvre directement le rapport détaillé
}

static void ouvrirRapport() {
	if (!g_aResultat) { analyserFichier(); if (!g_aResultat) return; }
	std::string html = versUtf8(genererRapport(g_dernier));
	wchar_t dossier[MAX_PATH];
	GetTempPathW(MAX_PATH, dossier);
	std::wstring chemin = std::wstring(dossier) + L"SeoChecker";
	CreateDirectoryW(chemin.c_str(), nullptr);
	chemin += L"\\rapport_seo.html";
	HANDLE f = CreateFileW(chemin.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE) { MessageBoxW(g_npp._nppHandle, L"Écriture impossible.", NOM_PLUGIN, MB_ICONERROR); return; }
	DWORD n;
	WriteFile(f, html.data(), (DWORD)html.size(), &n, nullptr);
	CloseHandle(f);
	ShellExecuteW(g_npp._nppHandle, L"open", chemin.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void insererEntete() {
	std::wstring eol = finDeLigne();
	std::wstring motcle = L"MON MOT-CLÉ";
	std::wstring l = L"<!-- seo: motcle=" + motcle + L" | domaine=monsite.fr -->" + eol;
	std::string cp = [&] {
		UINT c = S(SCI_GETCODEPAGE) == SC_CP_UTF8 ? CP_UTF8 : CP_ACP;
		int n = WideCharToMultiByte(c, 0, l.data(), (int)l.size(), nullptr, 0, nullptr, nullptr);
		std::string r(n, '\0');
		WideCharToMultiByte(c, 0, l.data(), (int)l.size(), &r[0], n, nullptr, nullptr);
		return r;
	}();
	S(SCI_INSERTTEXT, 0, (LPARAM)cp.c_str());
	// sélectionne le texte du mot-clé à remplacer
	UINT c = S(SCI_GETCODEPAGE) == SC_CP_UTF8 ? CP_UTF8 : CP_ACP;
	auto oct = [&](const std::wstring &w) {
		return WideCharToMultiByte(c, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	};
	intptr_t deb = oct(L"<!-- seo: motcle=");
	S(SCI_SETSEL, deb, deb + oct(motcle));
	S(SCI_GRABFOCUS);
}

static void placer(int w, int h) {
	int m = ech(4), bh = ech(24), bw = (w - m * 5) / 4;
	for (int i = 0; i < 4; ++i) MoveWindow(g_boutons[i], m + i * (bw + m), m, bw, bh, TRUE);
	int y = m * 2 + bh, rh = ech(120);
	MoveWindow(g_resume, m, y, w - 2 * m, rh, TRUE);
	y += rh + m;
	MoveWindow(g_liste, m, y, w - 2 * m, h - y - m, TRUE);
}

static LRESULT CALLBACK procPanneau(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_SIZE: placer(LOWORD(lp), HIWORD(lp)); return 0;
	case WM_COMMAND:
		switch (LOWORD(wp)) {
		case ID_B_FICHIER: analyserFichier(); break;
		case ID_B_URL: analyserUrl(); break;
		case ID_B_RAPPORT: ouvrirRapport(); break;
		case ID_B_ENTETE: insererEntete(); break;
		}
		return 0;
	case WM_NOTIFY: {
		NMHDR *nh = (NMHDR *)lp;
		if (nh->hwndFrom == g_liste && nh->code == NM_CUSTOMDRAW) {
			NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)lp;
			if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
			if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
				size_t i = cd->nmcd.dwItemSpec;
				if (i < g_couleurs.size()) cd->clrText = g_couleurs[i];
				return CDRF_NEWFONT;
			}
			return CDRF_DODEFAULT;
		}
		if (nh->hwndFrom == g_npp._nppHandle && LOWORD(nh->code) == DMN_CLOSE) {
			g_visible = false;
			cocherMenu();
		}
		break;
	}
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}

static void creerPanneau() {
	INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
	InitCommonControlsEx(&icc);
	WNDCLASSW wc = {};
	wc.lpfnWndProc = procPanneau;
	wc.hInstance = g_hInst;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = L"SeoCheckerPanneau";
	RegisterClassW(&wc);
	g_panneau = CreateWindowExW(0, wc.lpszClassName, NOM_PLUGIN, WS_CHILD | WS_CLIPCHILDREN, 0, 0, ech(400), ech(600),
	                            g_npp._nppHandle, nullptr, g_hInst, nullptr);

	NONCLIENTMETRICSW ncm = {sizeof(ncm)};
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
	g_police = CreateFontIndirectW(&ncm.lfMessageFont);

	static const wchar_t *lib[] = {L"Fichier", L"URL…", L"Rapport", L"En-tête"};
	int ids[] = {ID_B_FICHIER, ID_B_URL, ID_B_RAPPORT, ID_B_ENTETE};
	for (int i = 0; i < 4; ++i)
		g_boutons[i] = CreateWindowExW(0, L"BUTTON", lib[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10,
		                               g_panneau, (HMENU)(INT_PTR)ids[i], g_hInst, nullptr);
	g_resume = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
	                           WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0, 0, 10, 10,
	                           g_panneau, (HMENU)ID_RESUME, g_hInst, nullptr);
	g_liste = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
	                          WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 0, 0, 10, 10, g_panneau,
	                          (HMENU)ID_LISTE, g_hInst, nullptr);
	SendMessage(g_liste, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
	for (HWND h : {g_boutons[0], g_boutons[1], g_boutons[2], g_boutons[3], g_resume, g_liste})
		SendMessage(h, WM_SETFONT, (WPARAM)g_police, TRUE);
	struct { const wchar_t *n; int w; } cols[] = {{L"Catégorie", 80}, {L"Élément", 150}, {L"Valeur / conseil", 240}, {L"Niveau", 70}};
	for (int i = 0; i < 4; ++i) {
		LVCOLUMNW c = {};
		c.mask = LVCF_TEXT | LVCF_WIDTH;
		c.pszText = const_cast<wchar_t *>(cols[i].n);
		c.cx = ech(cols[i].w);
		SendMessage(g_liste, LVM_INSERTCOLUMNW, i, (LPARAM)&c);
	}
}

static void cmdPanneau() {
	if (!g_panneau) creerPanneau();
	if (!g_enregistre) {
		g_tb = {};
		g_tb.hClient = g_panneau;
		g_tb.pszName = L"SEO Checker";
		g_tb.dlgID = g_cmdPanneau;
		g_tb.uMask = DWS_DF_CONT_RIGHT;
		g_tb.pszModuleName = NOM_MODULE;
		SendMessage(g_npp._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&g_tb);
		g_enregistre = true;
		g_visible = false;
	}
	g_visible = !g_visible;
	SendMessage(g_npp._nppHandle, g_visible ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, (LPARAM)g_panneau);
	cocherMenu();
	if (g_visible) { if (!g_aResultat) analyserFichier(); else remplirPanneau(); }
}

static void cmdAnalyser() { if (!g_panneau || !g_visible) cmdPanneau(); analyserFichier(); }
static void cmdUrl() { if (!g_panneau || !g_visible) cmdPanneau(); analyserUrl(); }
static void cmdRapport() { ouvrirRapport(); }
static void cmdEntete() { insererEntete(); }

static void cmdAide() {
	MessageBoxW(g_npp._nppHandle,
	            L"SEO Checker analyse le fichier ouvert (HTML, Markdown ou texte) ou une page en ligne.\n\n"
	            L"Mot-clé cible — ajoutez en tête du fichier :\n"
	            L"  <!-- seo: motcle=location vélo annecy | domaine=monsite.fr -->\n"
	            L"  (motcle = expression visée ; domaine = votre site, pour classer les liens)\n\n"
	            L"Commandes (menu Modules > SEO Checker) :\n"
	            L"  • Panneau SEO : ouvre le tableau à droite\n"
	            L"  • Analyser le fichier : contenu courant\n"
	            L"  • Analyser une URL : télécharge et analyse une page\n"
	            L"  • Rapport détaillé : page HTML dans le navigateur\n"
	            L"  • Insérer l'en-tête SEO\n\n"
	            L"Contrôles : title, meta description, structure Hn, présence et densité du mot-clé,\n"
	            L"lisibilité, nombre de mots, ratio texte/code, liens internes/externes, images sans alt.",
	            L"SEO Checker — aide", MB_ICONINFORMATION);
}

static void cmdAPropos() {
	MessageBoxW(g_npp._nppHandle, L"SEO Checker 1.0\nAnalyse SEO on-page dans Notepad++ : fichier local ou URL.",
	            NOM_PLUGIN, MB_ICONINFORMATION);
}

// ---- Menu / exports ---------------------------------------------------------

static ShortcutKey skPanneau = {true, true, true, 'E'};
static ShortcutKey skAnalyser = {true, true, true, 'A'};
static ShortcutKey skUrl = {true, true, true, 'U'};
static ShortcutKey skRapport = {true, true, true, 'R'};

struct Cmd { const wchar_t *nom; PFUNCPLUGINCMD f; ShortcutKey *sk; };
static const Cmd CMDS[] = {
    {L"Panneau SEO", cmdPanneau, &skPanneau},
    {L"Analyser le fichier courant", cmdAnalyser, &skAnalyser},
    {L"Analyser une URL…", cmdUrl, &skUrl},
    {L"Rapport détaillé (HTML)", cmdRapport, &skRapport},
    {L"", nullptr, nullptr},
    {L"Insérer l'en-tête SEO", cmdEntete, nullptr},
    {L"", nullptr, nullptr},
    {L"Aide", cmdAide, nullptr},
    {L"À propos", cmdAPropos, nullptr},
};
static const int NB = sizeof(CMDS) / sizeof(CMDS[0]);
static FuncItem g_funcs[NB];
static void cocherMenu() { SendMessage(g_npp._nppHandle, NPPM_SETMENUITEMCHECK, g_funcs[g_cmdPanneau]._cmdID, g_visible); }

BOOL APIENTRY DllMain(HINSTANCE h, DWORD raison, LPVOID) {
	if (raison == DLL_PROCESS_ATTACH) g_hInst = h;
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData d) {
	g_npp = d;
	for (int i = 0; i < NB; ++i) {
		lstrcpynW(g_funcs[i]._itemName, CMDS[i].nom, menuItemSize);
		g_funcs[i]._pFunc = CMDS[i].f;
		g_funcs[i]._init2Check = false;
		g_funcs[i]._pShKey = CMDS[i].sk;
	}
	g_cmdPanneau = 0;
}

extern "C" __declspec(dllexport) const wchar_t *getName() { return NOM_PLUGIN; }
extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *n) { *n = NB; return g_funcs; }

extern "C" __declspec(dllexport) void beNotified(SCNotification *n) {
	if (n->nmhdr.code == NPPN_SHUTDOWN && (HWND)n->nmhdr.hwndFrom == g_npp._nppHandle) {
		if (g_panneau) DestroyWindow(g_panneau);
		if (g_police) DeleteObject(g_police);
		g_panneau = nullptr;
		g_police = nullptr;
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
