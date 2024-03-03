/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <set>

#include "Application.h"
#include "Scene.h"

class EditorState : public AppState {
public:
  EditorState(Application &app);
  void createGui() override;
  void render(const glm::mat4 &pv) override;
  bool keyCallback(int key, int scancode, int action, int mods) override;

private:
  void createMain();
  void createEdit();

  Application &mApp;
  Scene &mScene;

  bool mEditing = false;
  size_t mEditIndex = 0;
  std::string mPcdFilename;
  // Use set instead of unordered_set to make the order in the alignment
  // predictable. In practice, the difference in performance will probably not
  // matter a lot.
  std::set<size_t> mSelected;
};
