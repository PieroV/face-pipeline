/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <memory>

#include "BaseApplication.h"

#include "Renderer.h"
#include "Scene.h"

class AppState {
public:
  virtual ~AppState() = default;
  virtual void start() {}
  virtual void createGui() {}
  virtual void render(const glm::mat4 &pv) { (void)pv; }
  virtual bool keyCallback(int key, int scancode, int action, int mods) {
    (void)key;
    (void)scancode;
    (void)action;
    (void)mods;
    return false;
  }
};

class Application : public BaseApplication {
public:
  Application();
  int run(const char *dataDirectory);

  void setState(std::unique_ptr<AppState> newState);

  void setTitleDetails(const std::string &details);

  Renderer &getRenderer();
  Scene &getScene();
  const Scene &getScene() const;

  void refreshBuffer(std::optional<double> voxelSize = std::nullopt);
  void renderScene(const glm::mat4 &pv, bool paintUniform = false) const;

protected:
  void beginFrame() override;
  void createGui() override;
  void render() override;
  void keyCallback(int key, int scancode, int action, int mods) override;

  // We need to defer the renderer initialization until we have loaded OpenGL.
  std::optional<Renderer> mRenderer;

  std::unique_ptr<AppState> mCurrentState;
  std::unique_ptr<AppState> mPendingState;
  std::unique_ptr<Scene> mScene;
};
