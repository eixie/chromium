// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "extensions/common/extension.h"

LauncherItemController::LauncherItemController(
    Type type,
    const std::string& app_id,
    ChromeLauncherController* launcher_controller)
    : type_(type),
      app_id_(app_id),
      shelf_id_(0),
      launcher_controller_(launcher_controller),
      locked_(0),
      image_set_by_controller_(false) {
}

LauncherItemController::~LauncherItemController() {
}

const std::string& LauncherItemController::app_id() const {
  return app_id_;
}

base::string16 LauncherItemController::GetAppTitle() const {
  if (app_id_.empty())
    return base::string16();
  const extensions::Extension* extension =
      launcher_controller_->profile()->GetExtensionService()->
      GetInstalledExtension(app_id_);
  return extension ? base::UTF8ToUTF16(extension->name()) : base::string16();
}

ash::ShelfItemType LauncherItemController::GetShelfItemType() const {
  switch (type_) {
    case LauncherItemController::TYPE_SHORTCUT:
    case LauncherItemController::TYPE_WINDOWED_APP:
      return ash::TYPE_APP_SHORTCUT;
    case LauncherItemController::TYPE_APP:
      return ash::TYPE_PLATFORM_APP;
    case LauncherItemController::TYPE_APP_PANEL:
      return ash::TYPE_APP_PANEL;
  }
  NOTREACHED();
  return ash::TYPE_APP_SHORTCUT;
}
