// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_WINDOW_SET_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_WINDOW_SET_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"

class ArgonSpaces;

namespace base {
class FilePath;
}  // namespace base
class BrowserWindowInterface;

// Spaces across profiles (docs/notes.md, "Spaces", stage 4). A window holds one
// profile's tabs, so a space with its own profile lives in that profile's
// window. The set is the first normal window of each profile; they're stacked
// at one frame and only one is shown. The sidebar lists every member's spaces,
// and switching to a space in another member shows that window in place of the
// current one. Windows outside the set (a second ⌘N window, incognito) list
// only their own spaces. Nothing is stored: which window a space lives in is
// its profile.
class ArgonWindowSet : public BrowserCollectionObserver {
 public:
  struct Space {
    raw_ptr<BrowserWindowInterface> browser;
    tab_groups::TabGroupId id;
  };

  static ArgonWindowSet* Get();

  ArgonWindowSet(const ArgonWindowSet&) = delete;
  ArgonWindowSet& operator=(const ArgonWindowSet&) = delete;

  // The spaces `from`'s sidebar lists: every member's when `from` is in the
  // set, else `from`'s own. Members in creation order, each in strip order.
  std::vector<Space> List(BrowserWindowInterface* from);
  // `browser`'s ArgonSpaces, or null if it has no sidebar.
  ArgonSpaces* SpacesFor(BrowserWindowInterface* browser);
  // The set's window for the profile at `profile_path`, or null.
  BrowserWindowInterface* MemberFor(const base::FilePath& profile_path);
  // Switches `from` to `space`, swapping windows if it lives in another one.
  void Switch(BrowserWindowInterface* from, const Space& space);

  // Called with the window whose spaces changed; sidebars of other windows
  // redraw (all of them for null).
  using ChangedCallback = base::RepeatingCallback<void(BrowserWindowInterface*)>;
  base::CallbackListSubscription Subscribe(ChangedCallback callback);
  void NotifyChanged(BrowserWindowInterface* browser);

  // Hides a member (it stays open, recorded as hidden by the set).
  void Hide(BrowserWindowInterface* browser);

  // Each window's ArgonSpaces registers itself.
  void Register(BrowserWindowInterface* browser, ArgonSpaces* spaces);
  void Unregister(BrowserWindowInterface* browser);

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

 private:
  friend class base::NoDestructor<ArgonWindowSet>;

  ArgonWindowSet();
  ~ArgonWindowSet() override;

  std::vector<BrowserWindowInterface*> Members();
  bool IsMember(BrowserWindowInterface* browser);
  // Keeps one member on screen: `shown`, at the frame of the member it hides.
  void HideOtherMembers(SessionID shown);
  void CloseWindows(std::vector<SessionID> ids);

  std::map<BrowserWindowInterface*, raw_ptr<ArgonSpaces>> spaces_;
  // The windows this set hid (and not shown since). Always empty with one
  // profile.
  std::set<SessionID> hidden_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)> on_changed_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_WINDOW_SET_H_
