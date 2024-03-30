/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "Application.h"

#include <set>

#include "open3d/geometry/KDTreeFlann.h"
#include "open3d/geometry/TriangleMesh.h"

#include "Texture.h"

class TextureLabState : public AppState {
public:
  struct TextureData {
    TextureData(const Scene &scene, size_t idx, bool useMask);
    void updateTree(const Scene &scene, bool useMask);
    std::optional<Eigen::Vector2f> findPointUv(const Eigen::Vector3d &point,
                                               double radius) const;

    size_t index;
    std::string name;
    Texture texture;
    std::optional<open3d::geometry::KDTreeFlann> tree;
    std::vector<Eigen::Vector2f> uv;
    bool active = true;
    GLsizei triangles = 0;
  };

  TextureLabState(Application &app, const std::set<size_t> &indices);
  void start() override;
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  void update();
  void fileModal(const char *title, const char *button, std::string &filename,
                 const std::function<bool()> &func);
  bool loadMesh();
  bool saveMesh();
  bool exportTexture();

  Application &mApp;
  open3d::geometry::TriangleMesh mMesh;

  // unique_ptr is a workaround for the lack of copy constructor on Texture.
  std::vector<std::unique_ptr<TextureData>> mTextures;
  GLsizei mNotTextured = 0;

  double mRadius = 0.005;
  bool mUseMask = true;
  Eigen::Vector3f mDefaultColor{0.0f, 0.0f, 0.0f};
  bool mHasMeshes = false;
  std::string mLoadFilename;
  std::string mSaveFilename;
  std::string mTextureFilename;
  std::string mErrorDesc;
};
