#include "Fetch.h"
#include "Seo.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

namespace seo {

// Détecte l'encodage déclaré (charset=...) pour décoder correctement.
static UINT pageDepuisCharset(const std::string &octets, const std::wstring &enteteType) {
	std::wstring t = enteteType;
	for (auto &c : t) c = towlower(c);
	auto chercher = [](const std::string &s) -> UINT {
		std::string b = s;
		for (auto &c : b) c = (char)tolower((unsigned char)c);
		size_t p = b.find("charset=");
		if (p == std::string::npos) return 0;
		p += 8;
		std::string cs;
		for (; p < b.size() && b[p] != '"' && b[p] != '\'' && b[p] != ';' && b[p] != ' ' && b[p] != '\r' &&
		       b[p] != '\n' && b[p] != '>'; ++p)
			cs += b[p];
		if (cs.find("utf-8") != std::string::npos || cs.find("utf8") != std::string::npos) return CP_UTF8;
		if (cs.find("1252") != std::string::npos) return 1252;
		if (cs.find("8859-1") != std::string::npos || cs.find("8859-15") != std::string::npos) return 28591;
		return 0;
	};
	std::string ent = std::string(t.begin(), t.end());
	UINT cp = chercher(ent);
	if (cp) return cp;
	cp = chercher(octets.substr(0, 2048)); // balise <meta charset>
	return cp ? cp : CP_UTF8;
}

Telechargement telecharger(const std::wstring &url0) {
	Telechargement t;
	std::wstring url = url0;
	if (url.compare(0, 4, L"http") != 0) url = L"https://" + url;

	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	wchar_t hote[256] = {}, chemin[2048] = {};
	uc.lpszHostName = hote;
	uc.dwHostNameLength = 255;
	uc.lpszUrlPath = chemin;
	uc.dwUrlPathLength = 2047;
	if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { t.erreur = L"URL invalide."; return t; }

	HINTERNET sess = WinHttpOpen(L"SEO-Checker/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
	                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!sess) { t.erreur = L"Initialisation réseau impossible."; return t; }
	int to = 15000;
	WinHttpSetTimeouts(sess, to, to, to, to);

	HINTERNET con = WinHttpConnect(sess, hote, uc.nPort, 0);
	HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", chemin, nullptr, WINHTTP_NO_REFERER,
	                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
	                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
	                    : nullptr;
	bool envoye = req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
	              WinHttpReceiveResponse(req, nullptr);

	if (!envoye) {
		DWORD e = GetLastError();
		t.erreur = e == ERROR_WINHTTP_NAME_NOT_RESOLVED ? L"Serveur introuvable (vérifiez l'adresse)."
		         : e == ERROR_WINHTTP_TIMEOUT ? L"Délai dépassé."
		         : e == ERROR_WINHTTP_SECURE_FAILURE ? L"Erreur de certificat HTTPS." : L"Connexion impossible.";
		if (req) WinHttpCloseHandle(req);
		if (con) WinHttpCloseHandle(con);
		WinHttpCloseHandle(sess);
		return t;
	}

	DWORD code = 0, taille = sizeof(code);
	WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &taille, nullptr);
	t.statut = (int)code;

	wchar_t typeC[256] = {};
	DWORD tC = sizeof(typeC);
	WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_TYPE, nullptr, typeC, &tC, nullptr);

	std::string corps;
	DWORD dispo = 0;
	do {
		if (!WinHttpQueryDataAvailable(req, &dispo)) break;
		if (!dispo) break;
		std::string bloc(dispo, '\0');
		DWORD lu = 0;
		if (!WinHttpReadData(req, &bloc[0], dispo, &lu)) break;
		corps.append(bloc.data(), lu);
		if (corps.size() > 8u * 1024 * 1024) break; // garde-fou 8 Mo
	} while (dispo > 0);

	WinHttpCloseHandle(req);
	WinHttpCloseHandle(con);
	WinHttpCloseHandle(sess);

	if (code >= 400) { t.erreur = L"Le serveur a renvoyé le code HTTP " + std::to_wstring(code) + L"."; return t; }
	if (corps.empty()) { t.erreur = L"Réponse vide."; return t; }

	UINT cp = pageDepuisCharset(corps, typeC);
	int n = MultiByteToWideChar(cp, 0, corps.data(), (int)corps.size(), nullptr, 0);
	if (n <= 0) { cp = CP_UTF8; n = MultiByteToWideChar(cp, 0, corps.data(), (int)corps.size(), nullptr, 0); }
	t.html.resize(n);
	MultiByteToWideChar(cp, 0, corps.data(), (int)corps.size(), &t.html[0], n);
	t.ok = true;
	return t;
}

} // namespace seo
