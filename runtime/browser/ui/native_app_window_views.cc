// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/ui/native_app_window_views.h"

#include "content/public/browser/context_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "xwalk/runtime/browser/ui/top_view_layout_views.h"
#include "xwalk/runtime/browser/ui/xwalk_views_delegate.h"
#include "xwalk/runtime/common/xwalk_notification_types.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/wm/test/wm_test_helper.h"
#else
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

#if defined(OS_WIN)
#include "ui/views/window/native_frame_view.h"
#endif

#if defined(OS_TIZEN)
#include "xwalk/runtime/browser/ui/native_app_window_tizen.h"
#endif

#if defined(OS_CHROMEOS)
wm::WMTestHelper* wm_test_helper_ = NULL;
#endif

namespace xwalk {

const int kDefaultTestWindowWidthDip = 800;
const int kDefaultTestWindowHeightDip = 600;

NativeAppWindowViews::NativeAppWindowViews(
    const NativeAppWindow::CreateParams& create_params)
  : create_params_(create_params),
    delegate_(create_params.delegate),
    web_contents_(create_params.web_contents),
    web_view_(NULL),
    window_(NULL),
    is_fullscreen_(false),
    minimum_size_(create_params.minimum_size),
    maximum_size_(create_params.maximum_size),
    resizable_(create_params.resizable) {}

NativeAppWindowViews::~NativeAppWindowViews() {}

// Two-step initialization is needed here so that calls done by views::Widget to
// its delegate during views::Widget::Init() reach the correct implementation --
// e.g. ViewHierarchyChanged().
void NativeAppWindowViews::Initialize() {
  CHECK(!window_);
#if defined(OS_CHROMEOS)
  window_ = views::Widget::CreateWindowWithContextAndBounds(
      this,
      wm_test_helper_->GetDefaultParent(NULL, NULL, gfx::Rect()),
      gfx::Rect(0, 0, kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip));

  // Call ShowRootWindow on RootWindow created by WMTestHelper without
  // which XWindow owned by RootWindow doesn't get mapped.
  window_->GetNativeWindow()->GetHost()->Show();
  window_->Show();

  // TODO(vignati): essentially Content Shell's SetWebContents. Need to find a
  // better way to place this in face of the ViewHierarchyChanged
  {
    TopViewLayout* layout = new TopViewLayout();
    SetLayoutManager(layout);

    web_view_ = new views::WebView(web_contents_->GetBrowserContext());
    web_view_->SetWebContents(web_contents_);
    web_view_->SetPreferredSize(
        gfx::Size(kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip));
    web_contents_->Focus();
    AddChildView(web_view_);
    layout->set_content_view(web_view_);

    // Resize the widget, keeping the same origin.
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds.set_size(GetWidget()->GetRootView()->GetPreferredSize());
    GetWidget()->SetBounds(bounds);

    // Resizing a widget on chromeos doesn't automatically resize the root, need
    // to explicitly do that.
    GetWidget()->GetNativeWindow()->GetHost()->SetBounds(bounds);
  }
#else
  window_ = new views::Widget;

  views::Widget::InitParams params;
  params.delegate = this;
  params.remove_standard_frame = false;
  params.use_system_default_icon = true;
  params.show_state = create_params_.state;
  params.parent = create_params_.parent;
#if defined(OS_TIZEN_MOBILE)
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  // On Tizen apps are sized to the work area.
  gfx::Rect bounds =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
  params.bounds = bounds;
#else
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.bounds = create_params_.bounds;
#endif
  params.net_wm_pid = create_params_.net_wm_pid;

  window_->Init(params);

#if defined(OS_TIZEN_MOBILE)
  // Set the bounds manually to avoid inset.
  window_->SetBounds(bounds);
#elif !defined(USE_OZONE)
  window_->CenterWindow(create_params_.bounds.size());
#endif
#endif

  if (create_params_.state == ui::SHOW_STATE_FULLSCREEN)
    SetFullscreen(true);

  // TODO(hmin): Need to configure the maximum and minimum size of this window.
  window_->AddObserver(this);
}

gfx::NativeWindow NativeAppWindowViews::GetNativeWindow() const {
  return window_->GetNativeWindow();
}

void NativeAppWindowViews::UpdateIcon(const gfx::Image& icon) {
  icon_ = icon;
  window_->UpdateWindowIcon();
}

void NativeAppWindowViews::UpdateTitle(const base::string16& title) {
  title_ = title;
  window_->UpdateWindowTitle();
}

gfx::Rect NativeAppWindowViews::GetRestoredBounds() const {
  return window_->GetRestoredBounds();
}

gfx::Rect NativeAppWindowViews::GetBounds() const {
  return window_->GetWindowBoundsInScreen();
}

void NativeAppWindowViews::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

void NativeAppWindowViews::Focus() {
  // window_->Focus();
}

void NativeAppWindowViews::Show() {
  window_->Show();
}

void NativeAppWindowViews::Hide() {
  window_->Hide();
}

void NativeAppWindowViews::Maximize() {
  window_->Maximize();
}

void NativeAppWindowViews::Minimize() {
  window_->Minimize();
}

void NativeAppWindowViews::SetFullscreen(bool fullscreen) {
  views::ViewsDelegate::views_delegate->SetShouldShowTitleBar(!fullscreen);

  if (is_fullscreen_ == fullscreen)
    return;
  is_fullscreen_ = fullscreen;
  window_->SetFullscreen(is_fullscreen_);

  content::NotificationService::current()->Notify(
      xwalk::NOTIFICATION_FULLSCREEN_CHANGED,
      content::Source<NativeAppWindow>(this),
      content::NotificationService::NoDetails());
}

void NativeAppWindowViews::Restore() {
  window_->Restore();
}

void NativeAppWindowViews::FlashFrame(bool flash) {
  window_->FlashFrame(flash);
}

void NativeAppWindowViews::Close() {
  window_->Close();
}

bool NativeAppWindowViews::IsActive() const {
  return window_->IsActive();
}

bool NativeAppWindowViews::IsMaximized() const {
  return window_->IsMaximized();
}

bool NativeAppWindowViews::IsMinimized() const {
  return window_->IsMinimized();
}

bool NativeAppWindowViews::IsFullscreen() const {
  return is_fullscreen_;
}

TopViewLayout* NativeAppWindowViews::top_view_layout() {
  return static_cast<TopViewLayout*>(GetLayoutManager());
}

////////////////////////////////////////////////////////////
// WidgetDelegate implementation
////////////////////////////////////////////////////////////
views::View* NativeAppWindowViews::GetInitiallyFocusedView() {
  return web_view_;
}

views::View* NativeAppWindowViews::GetContentsView() {
  return this;
}

views::Widget* NativeAppWindowViews::GetWidget() {
  return window_;
}

const views::Widget* NativeAppWindowViews::GetWidget() const {
  return window_;
}

base::string16 NativeAppWindowViews::GetWindowTitle() const {
  return title_;
}

void NativeAppWindowViews::DeleteDelegate() {
  window_->RemoveObserver(this);
  delegate_->OnWindowDestroyed();
  delete this;
}
gfx::ImageSkia NativeAppWindowViews::GetWindowAppIcon() {
  return GetWindowIcon();
}

gfx::ImageSkia NativeAppWindowViews::GetWindowIcon() {
  return *icon_.ToImageSkia();
}

bool NativeAppWindowViews::ShouldShowWindowTitle() const {
  return true;
}

void NativeAppWindowViews::SaveWindowPlacement(const gfx::Rect& bounds,
                                               ui::WindowShowState show_state) {
  // TODO(hmin): views::WidgetDelegate::SaveWindowPlacement(bounds, show_state);
}

bool NativeAppWindowViews::GetSavedWindowPlacement(const views::Widget* widget,
    gfx::Rect* bounds, ui::WindowShowState* show_state) const {
  // TODO(hmin): Get the saved window placement.
  return false;
}

bool NativeAppWindowViews::CanResize() const {
  return resizable_ &&
      (maximum_size_.IsEmpty() || minimum_size_ != maximum_size_);
}

bool NativeAppWindowViews::CanMaximize() const {
  return resizable_ && maximum_size_.IsEmpty();
}

#if defined(OS_WIN)
views::NonClientFrameView* NativeAppWindowViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return new views::NativeFrameView(widget);
}
#endif

////////////////////////////////////////////////////////////
// views::View implementation
////////////////////////////////////////////////////////////

void NativeAppWindowViews::ChildPreferredSizeChanged(views::View* child) {
  // We only re-layout when the top view changes its preferred size (and notify
  // us by calling its own function PreferredSizeChanged()). We don't react to
  // changes in preferred size for the content view since we are currently
  // setting its bounds to the maximum size available.
  if (child == top_view_layout()->top_view())
    Layout();
}

void NativeAppWindowViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
#if !defined(OS_CHROMEOS)
  // TODO(vignatti): this initialization code actually can't reside here. Gotta
  // find a smarter way to do this just like Content Shell's SetWebContents
  if (details.is_add && details.child == this) {
    TopViewLayout* layout = new TopViewLayout();
    SetLayoutManager(layout);

    web_view_ = new views::WebView(NULL);
    web_view_->SetWebContents(web_contents_);
    AddChildView(web_view_);
    layout->set_content_view(web_view_);
  }
#endif
}

void NativeAppWindowViews::OnFocus() {
  web_view_->RequestFocus();
}

gfx::Size NativeAppWindowViews::GetMaximumSize() const {
  return maximum_size_;
}

gfx::Size NativeAppWindowViews::GetMinimumSize() const {
  return minimum_size_;
}

////////////////////////////////////////////////////////////
// views::WidgetObserver implementation
////////////////////////////////////////////////////////////
void NativeAppWindowViews::OnWidgetClosing(views::Widget* widget) {
}
void NativeAppWindowViews::OnWidgetCreated(views::Widget* widget) {
}
void NativeAppWindowViews::OnWidgetDestroying(views::Widget* widget) {
}
void NativeAppWindowViews::OnWidgetDestroyed(views::Widget* widget) {
}
void NativeAppWindowViews::OnWidgetBoundsChanged(views::Widget* widget,
    const gfx::Rect& new_bounds) {
}

// static
NativeAppWindow* NativeAppWindow::Create(
    const NativeAppWindow::CreateParams& create_params) {
  NativeAppWindowViews* window;
#if defined(OS_TIZEN)
  window = new NativeAppWindowTizen(create_params);
#else
  window = new NativeAppWindowViews(create_params);
#endif
  window->Initialize();
  return window;
}

// static
void NativeAppWindow::Initialize() {
  CHECK(!views::ViewsDelegate::views_delegate);
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Initialize();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                 aura::TestScreen::Create(gfx::Size()));
  gfx::Size default_window_size =
      gfx::Size(kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip);
  //TODO(vignatti): I leak these following objects
  wm_test_helper_ = new wm::WMTestHelper(default_window_size,
                                         content::GetContextFactory());
#else
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, views::CreateDesktopScreen());
#endif
  views::ViewsDelegate::views_delegate = new XWalkViewsDelegate();
}

}  // namespace xwalk
