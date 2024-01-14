#pragma once

#include "Application.h"
#include "Scene.h"

class EditorState : public AppState {
public:
  EditorState(Application &app);
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  void createMain();
  void createEdit();

  Application &mApp;
  Scene &mScene;

  bool mEditing = false;
  size_t mEditIndex = 0;
  std::string mPcdFilename;
};
