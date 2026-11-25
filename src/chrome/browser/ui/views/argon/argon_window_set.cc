// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_window_set.h"

#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/argon/argon_spaces.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// The sidebar's width and collapsed state are per window; the window coming on
// screen takes them from the one it replaces.
void CopySidebarState(BrowserWindowInterface* from, BrowserWindowInterface* to) {
  const auto* from_state = tabs::VerticalTabStripStateController::From(from);
  auto* to_state = tabs::VerticalTabStripStateController::From(to);
  if (!from_state || !to_state) {
    return;
  }
  to_state->SetUncollapsedWidth(from_state->GetUncollapsedWidth());
  if (to_state->IsCollapsed() != from_state->IsCollapsed()) {
    to_state->RequestCollapse(from_state->IsCollapsed());
  }
}

}  // namespace

// static
ArgonWindowSet* ArgonWindowSet::Get() {
  static base::NoDestructor<ArgonWindowSet> instance;
  return instance.get();
}

ArgonWindowSet::ArgonWindowSet() {
  observation_.Observe(GlobalBrowserCollection::GetInstance());
}

ArgonWindowSet::~ArgonWindowSet() = default;

std::vector<BrowserWindowInterface*> ArgonWindowSet::Members() {
  std::vector<BrowserWindowInterface*> members;
  std::set<const Profile*> seen;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        const Profile* profile = browser->GetProfile();
        if (browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
            !browser->IsDeleteScheduled() && !profile->IsOffTheRecord() &&
            seen.insert(profile).second) {
          members.push_back(browser);
        }
        return true;
      },
      BrowserCollection::Order::kCreation);
  return members;
}

bool ArgonWindowSet::IsMember(BrowserWindowInterface* browser) {
  for (BrowserWindowInterface* member : Members()) {
    if (member == browser) {
      return true;
    }
  }
  return false;
}

BrowserWindowInterface* ArgonWindowSet::MemberFor(
    const base::FilePath& profile_path) {
  for (BrowserWindowInterface* member : Members()) {
    if (member->GetProfile()->GetPath() == profile_path) {
      return member;
    }
  }
  return nullptr;
}

std::vector<ArgonWindowSet::Space> ArgonWindowSet::List(
    BrowserWindowInterface* from) {
  std::vector<BrowserWindowInterface*> windows = {from};
  if (IsMember(from)) {
    windows = Members();
  }
  std::vector<Space> spaces;
  for (BrowserWindowInterface* browser : windows) {
    for (const tab_groups::TabGroupId& id :
         ArgonSpaces::ListIn(browser->GetTabStripModel())) {
      spaces.push_back({browser, id});
    }
  }
  return spaces;
}

void ArgonWindowSet::Switch(BrowserWindowInterface* from, const Space& space) {
  const auto it = spaces_.find(space.browser);
  if (it == spaces_.end()) {
    return;
  }
  it->second->Switch(space.id);
  if (space.browser == from) {
    return;
  }
  ui::BaseWindow* shown = from->GetWindow();
  ui::BaseWindow* target = space.browser->GetWindow();
  // ponytail: no swap out of fullscreen, which owns a macOS Space; the other
  // window just comes forward.
  if (!shown->IsFullscreen()) {
    target->SetBounds(shown->GetBounds());
    CopySidebarState(from, space.browser);
  }
  // Show the target first, so there's no gap and AppKit doesn't bring up a
  // third window.
  target->Show();
  target->Activate();
  if (!shown->IsFullscreen()) {
    Hide(from);
  }
  NotifyChanged(space.browser);
}

base::CallbackListSubscription ArgonWindowSet::Subscribe(
    ChangedCallback callback) {
  return on_changed_.Add(std::move(callback));
}

void ArgonWindowSet::NotifyChanged(BrowserWindowInterface* browser) {
  on_changed_.Notify(browser);
}

void ArgonWindowSet::Register(BrowserWindowInterface* browser,
                              ArgonSpaces* spaces) {
  spaces_[browser] = spaces;
}

void ArgonWindowSet::Unregister(BrowserWindowInterface* browser) {
  spaces_.erase(browser);
}

void ArgonWindowSet::OnBrowserCreated(BrowserWindowInterface* browser) {
  NotifyChanged(browser);
}

void ArgonWindowSet::Hide(BrowserWindowInterface* browser) {
  hidden_.insert(browser->GetSessionID());
  browser->GetWindow()->Hide();
}

void ArgonWindowSet::OnBrowserClosed(BrowserWindowInterface* browser) {
  hidden_.erase(browser->GetSessionID());
  // When the last member this set didn't hide closes, close the hidden ones
  // too, so the app isn't left with only invisible windows. (Not by asking
  // windows whether they're visible: a closing window hides itself first, and
  // minimized or ⌘H-hidden ones aren't visible either.)
  if (!hidden_.empty()) {
    const std::vector<BrowserWindowInterface*> members = Members();
    std::vector<SessionID> ids;
    bool all_hidden = true;
    for (BrowserWindowInterface* member : members) {
      if (member == browser) {
        continue;
      }
      all_hidden &= hidden_.contains(member->GetSessionID());
      ids.push_back(member->GetSessionID());
    }
    // Not from inside the close notification.
    if (all_hidden && !ids.empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ArgonWindowSet::CloseWindows,
                                    base::Unretained(this), std::move(ids)));
    }
  }
  NotifyChanged(browser);
}

void ArgonWindowSet::CloseWindows(std::vector<SessionID> ids) {
  for (const SessionID& id : ids) {
    if (BrowserWindowInterface* browser =
            BrowserWindowInterface::FromSessionID(id)) {
      browser->GetWindow()->Close();
    }
  }
}

void ArgonWindowSet::OnBrowserActivated(BrowserWindowInterface* browser) {
  hidden_.erase(browser->GetSessionID());
  // Anything can bring a member forward (session restore, links from other
  // apps, the profile picker). A task later, once it's on screen, it takes the
  // frame of the member it replaces and that one is hidden. This also stacks
  // the set into one window at launch.
  if (IsMember(browser)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ArgonWindowSet::HideOtherMembers,
                       base::Unretained(this), browser->GetSessionID()));
  }
}

void ArgonWindowSet::HideOtherMembers(SessionID shown_id) {
  BrowserWindowInterface* shown = BrowserWindowInterface::FromSessionID(shown_id);
  if (!shown || !IsMember(shown) || !shown->GetWindow()->IsVisible() ||
      shown->GetWindow()->IsFullscreen()) {
    return;
  }
  bool moved = false;
  for (BrowserWindowInterface* member : Members()) {
    ui::BaseWindow* window = member->GetWindow();
    if (member == shown || !window->IsVisible() || window->IsFullscreen()) {
      continue;
    }
    if (!moved) {
      shown->GetWindow()->SetBounds(window->GetBounds());
      CopySidebarState(member, shown);
      moved = true;
    }
    Hide(member);
  }
  if (moved) {
    NotifyChanged(shown);
  }
}
