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
  void start() override;
  void createGui() override;
  void render(const glm::mat4 &pv) override;
  bool keyCallback(int key, int scancode, int action, int mods) override;

private:
  enum class Transformation {
    Translation,
    Rotation,
  };

  void createMain();
  void createEdit();
  void createMultiEdit();
  glm::mat4 multiTransformUi();
  void beginEdit(size_t idx);
  void beginMultiEdit();
  void refreshBuffer();

  Application &mApp;
  Scene &mScene;

  std::string mPcdFilename;
  // Use set instead of unordered_set to make the order in the alignment
  // predictable. In practice, the difference in performance will probably not
  // matter a lot.
  std::set<size_t> mSelected;

  bool mPaintUniform = false;
  bool mVoxelDown = false;
  double mVoxelSize = 0.005;

  bool mEditing = false;
  size_t mEditIndex = 0;
  glm::mat4 mEditMatrix;

  bool mMultiEditing = false;
  std::unordered_map<size_t, glm::mat4> mMultiEditMatrices;

  std::vector<std::pair<Transformation, glm::vec3>> mTransformations;
};
