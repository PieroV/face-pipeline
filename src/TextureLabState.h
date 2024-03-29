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
  TextureLabState(Application &app, const std::set<size_t> &indices);
  void start() override;
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  void update();
  void createTree();
  void createMesh();

  Application &mApp;
  open3d::geometry::TriangleMesh mMesh;
  std::unique_ptr<open3d::geometry::KDTreeFlann> mTree;
  std::vector<glm::vec2> mPointTextureMap;
  std::vector<size_t> mIndices;
  std::vector<Texture> mTextures;
  size_t mCurrent = 0;
  double mRadius = 0.005;
  bool mUseMask = true;
  std::string mMeshFilename;
  bool mHasMeshes = false;
  size_t mVerticesWithUVs = 0;
  size_t mTexturedTriangles = 0;
};
