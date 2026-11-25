// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_pins.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/supports_user_data.h"
#include "base/time/time.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"

namespace argon_pins {

namespace {

const char kUserDataKey[] = "argon_pin";

struct PinData : public base::SupportsUserData::Data {
  explicit PinData(const GURL& pinned_url) : url(pinned_url) {}
  GURL url;
};

void SetPinData(content::WebContents* contents, const GURL& url) {
  if (url.is_valid()) {
    contents->SetUserData(kUserDataKey, std::make_unique<PinData>(url));
  } else {
    contents->RemoveUserData(kUserDataKey);
  }
}

// Writes the pin state to the session (an empty value clears it). Tabs not in
// a window yet get it when their session commands are rebuilt.
void Persist(content::WebContents* contents) {
  SessionService* const session_service = SessionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  if (!session_service) {
    return;
  }
  const SessionID window_id =
      sessions::SessionTabHelper::IdForWindowContainingTab(contents);
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  if (!window_id.is_valid() || !tab_id.is_valid()) {
    return;
  }
  const GURL url = PinnedUrl(contents);
  session_service->AddTabExtraData(window_id, tab_id, kPinUrlKey,
                                   url.is_valid() ? url.spec() : std::string());
}

// Hands activation from the pin `tab` to the most recently used other tab of
// its space; with none, it stays.
void ActivateOtherTabInSpace(TabStripModel* model, tabs::TabInterface* tab) {
  const std::optional<tab_groups::TabGroupId> space = tab->GetGroup();
  int best = TabStripModel::kNoTab;
  base::Time best_time;
  for (int i = 0; i < model->count(); ++i) {
    tabs::TabInterface* other = model->GetTabAtIndex(i);
    if (other == tab || other->GetGroup() != space) {
      continue;
    }
    const base::Time last_active = other->GetContents()->GetLastActiveTime();
    if (best == TabStripModel::kNoTab || last_active > best_time) {
      best = i;
      best_time = last_active;
    }
  }
  if (best != TabStripModel::kNoTab) {
    model->ActivateTabAt(best);
  }
}

}  // namespace

GURL PinnedUrl(content::WebContents* contents) {
  const auto* data =
      static_cast<const PinData*>(contents->GetUserData(kUserDataKey));
  return data ? data->url : GURL();
}

bool IsPin(content::WebContents* contents) {
  return PinnedUrl(contents).is_valid();
}

void SetPin(content::WebContents* contents, const GURL& url) {
  SetPinData(contents, url);
  Persist(contents);
}

void RestoreFromExtraData(
    content::WebContents* contents,
    const std::map<std::string, std::string>& extra_data) {
  const auto it = extra_data.find(kPinUrlKey);
  if (it != extra_data.end() && !it->second.empty()) {
    SetPinData(contents, GURL(it->second));
  }
}

std::map<std::string, std::string> ExtraData(content::WebContents* contents) {
  const GURL url = PinnedUrl(contents);
  if (!url.is_valid()) {
    return {};
  }
  return {{kPinUrlKey, url.spec()}};
}

std::vector<tabs::TabHandle> HandleUserClosingTabs(
    BrowserWindowInterface* browser,
    std::vector<tabs::TabHandle> tab_handles) {
  std::vector<tabs::TabHandle> closing;
  for (const tabs::TabHandle& handle : tab_handles) {
    tabs::TabInterface* const tab = handle.Get();
    content::WebContents* const contents = tab ? tab->GetContents() : nullptr;
    // Global (Chromium-pinned) tabs are Helium's to handle.
    if (!contents || tab->IsPinned() || !IsPin(contents)) {
      closing.push_back(handle);
      continue;
    }
    const GURL url = PinnedUrl(contents);
    // At its pinned URL, also when that redirected (a site root that lands on
    // an inbox, say).
    const content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    const bool at_pinned_url =
        contents->GetLastCommittedURL() == url ||
        (entry && entry->GetOriginalRequestURL() == url);
    if (!at_pinned_url) {
      contents->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                        std::string());
    } else if (tab->IsActivated()) {
      ActivateOtherTabInSpace(browser->GetTabStripModel(), tab);
    }
  }
  return closing;
}

}  // namespace argon_pins
