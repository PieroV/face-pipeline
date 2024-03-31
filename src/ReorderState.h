/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include "Application.h"

class ReorderState : public AppState {
public:
  ReorderState(Application &app);
  void createGui() override;

private:
  void minimizeDistances();

  Application &mApp;
};
