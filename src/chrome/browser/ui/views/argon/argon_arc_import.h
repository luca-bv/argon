// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_ARC_IMPORT_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_ARC_IMPORT_H_

class BrowserWindowInterface;

// Imports Arc's sidebar (~/Library/Application Support/Arc/StorableSidebar.json;
// format as read by Helium's helium/core/add-arc-importer.patch), copying Arc:
// each Arc space becomes a space, its pinned tabs (folders flattened) become
// the space's pins and its other tabs loose tabs, and each Arc profile's
// Favorites become global pins. Spaces go to the Argon profile matching their
// Arc profile by name (created if missing); Arc's default profile is the
// profile running the import. Tabs aren't loaded until opened. Spaces whose
// name already exists in that window are skipped, so importing twice doesn't
// duplicate. Logins and history don't come over.
void ImportFromArc(BrowserWindowInterface* browser);

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_ARC_IMPORT_H_
