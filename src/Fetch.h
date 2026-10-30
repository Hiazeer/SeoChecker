// Téléchargement d'une page web (WinHTTP). Renvoie le HTML décodé en UTF-16.
#pragma once
#include <string>

namespace seo {

struct Telechargement {
	bool ok = false;
	std::wstring html;   // contenu décodé si ok
	std::wstring erreur; // message si échec
	int statut = 0;      // code HTTP
};

Telechargement telecharger(const std::wstring &url);

} // namespace seo
