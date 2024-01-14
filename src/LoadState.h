#include <memory>
#include <string>
#include <vector>

#include "Application.h"
#include "Scene.h"

class LoadState : public AppState {
public:
  LoadState(Application &app, std::unique_ptr<Scene> &scene,
            const char *dataDirectory);
  void createGui() override;

private:
  void load();
  void switchToEditor();

  Application &mApp;
  std::unique_ptr<Scene> &mScene;

  std::string mDirectory;
  std::string mError;
  std::vector<std::string> mWarnings;
};
