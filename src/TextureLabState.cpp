/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "TextureLabState.h"

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "open3d/geometry/BoundingVolume.h"
#include "open3d/geometry/TriangleMesh.h"
#include "open3d/io/ImageIO.h"
#include "open3d/io/TriangleMeshIO.h"

#include "EditorState.h"

TextureLabState::TextureLabState(Application &app,
                                 const std::set<size_t> &indices)
    : mApp(app), mIndices(indices.begin(), indices.end()) {
  if (indices.empty()) {
    throw std::invalid_argument("indices must not be empty.");
  }
  const auto &clouds = app.getScene().clouds;
  mTextures.reserve(indices.size());
  for (size_t idx : indices) {
    assert(idx < clouds.size());
    const PointCloud &pcd = clouds[idx];
    const open3d::geometry::RGBDImage &rgbd = pcd.getRgbdImage();
    mTextures.push_back(rgbd.color_);
  }
}

void TextureLabState::start() {
  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
  r.uploadBuffer();
  update();
}

void TextureLabState::createGui() {
  if (ImGui::Begin("Texture lab")) {
    const auto clouds = mApp.getScene().clouds;
    if (ImGui::BeginCombo("Texture", clouds[mCurrent].name.c_str(), 0)) {
      for (size_t idx : mIndices) {
        if (ImGui::Selectable(clouds[idx].name.c_str(), mCurrent == idx)) {
          if (mCurrent != idx) {
            mCurrent = idx;
            update();
          }
        }
        if (mCurrent == idx) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::InputDouble("Search radius", &mRadius);
    if (ImGui::Button("Update")) {
      update();
    }
    if (ImGui::Checkbox("Use mask if available", &mUseMask)) {
      update();
    }
    if (ImGui::Button("Load mesh...")) {
      ImGui::OpenPopup("Load mesh");
    }
    if (!mMesh.IsEmpty()) {
      ImGui::Text("%zu vertices, %zu triangles", mMesh.vertices_.size(),
                  mMesh.triangles_.size());
    }
    if (mHasMeshes) {
      ImGui::Text("%zu vertices with UV coordinates", mVerticesWithUVs);
      ImGui::Text("%zu textured triangles", mTexturedTriangles);
    }
    if (ImGui::Button("Close")) {
      mApp.setState(std::make_unique<EditorState>(mApp));
    }

    if (ImGui::BeginPopupModal("Load mesh", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::InputText("Filename", &mMeshFilename);
      if (ImGui::Button("Load")) {
        open3d::geometry::TriangleMesh mesh;
        if (!open3d::io::ReadTriangleMesh(mMeshFilename, mesh)) {
          ImGui::OpenPopup("Error loading");
        } else {
          ImGui::CloseCurrentPopup();
          mMesh = std::move(mesh);
          update();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::BeginPopupModal("Error loading", nullptr)) {
        ImGui::Text("Could not load %s.", mMeshFilename.c_str());
        if (ImGui::Button("OK")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();
}

void TextureLabState::render(const glm::mat4 &pv) {
  Renderer &r = mApp.getRenderer();
  r.beginRendering(pv);
  if (mHasMeshes) {
    assert(mCurrent < mTextures.size());
    glActiveTexture(GL_TEXTURE0);
    mTextures[mCurrent].bind();
    r.renderIndexedMesh(0, glm::mat4(1.0f), true);
    r.renderIndexedMesh(1, glm::mat4(1.0f), false);
  }
  r.endRendering();
}

void TextureLabState::update() {
  createTree();
  createMesh();
}

void TextureLabState::createTree() {
  mTree.reset();
  mPointTextureMap.clear();

  const auto &clouds = mApp.getScene().clouds;
  assert(mCurrent < clouds.size());
  const auto &pcd = clouds[mCurrent];

  const open3d::camera::PinholeCameraIntrinsic &camera =
      mApp.getScene().getCameraIntrinsic();
  glm::dvec2 principal(camera.GetPrincipalPoint().first,
                       camera.GetPrincipalPoint().second);
  glm::dvec2 focal(camera.GetFocalLength().first,
                   camera.GetFocalLength().second);
  glm::vec2 size(camera.width_, camera.height_);

  glm::dmat4 matrix = pcd.getMatrix();

  // We need to project the point on our own because we want to keep the
  // association between point (3D) and uv (2D) coordinates.
  std::vector<glm::dvec3> points;
  const open3d::geometry::RGBDImage rgbd = mUseMask && pcd.hasMaskedRgbd()
                                               ? *pcd.getMaskedRgbd()
                                               : pcd.getRgbdImage();
  if (rgbd.depth_.bytes_per_channel_ != 4 ||
      rgbd.depth_.num_of_channels_ != 1) {
    throw std::runtime_error("We can use only float depth images.");
  }
  const float *z = reinterpret_cast<const float *>(rgbd.depth_.data_.data());
  for (int y = 0; y < rgbd.depth_.height_; y++) {
    for (int x = 0; x < rgbd.depth_.width_; x++, z++) {
      if (*z <= 0) {
        continue;
      }
      glm::dvec4 point((x - principal.x) * *z / focal.x,
                       (y - principal.y) * *z / focal.y, *z, 1.0);
      points.push_back(matrix * point);
      glm::vec2 uv(x, -y);
      mPointTextureMap.push_back(uv / size + glm::vec2(0, 1.0f));
    }
  }

  if (points.empty()) {
    return;
  }

  using namespace Eigen;
  mTree = std::make_unique<open3d::geometry::KDTreeFlann>(Map<MatrixXd>(
      glm::value_ptr(points[0]), 3, static_cast<Index>(points.size())));
}

void TextureLabState::createMesh() {
  using namespace Eigen;

  if (mMesh.IsEmpty() || !mTree) {
    mHasMeshes = false;
    return;
  }

  open3d::geometry::AxisAlignedBoundingBox bbox =
      mMesh.GetAxisAlignedBoundingBox();
  glm::dvec3 extent = glm::make_vec3(bbox.GetExtent().data());

  Renderer::VertexMatrix vertices;
  size_t n = mMesh.vertices_.size();
  vertices.conservativeResize(static_cast<Index>(n), NoChange);
  std::vector<uint32_t> indexMap;
  indexMap.reserve(n);
  Index withUv = 0;
  Index withoutUv = static_cast<Index>(n);

  for (const Vector3d &point : mMesh.vertices_) {
    Vector3d shifted = point - bbox.min_bound_;
    glm::dvec3 color = glm::make_vec3(shifted.data()) / extent;
    VectorXd vertex{
        {point[0], point[1], point[2], color.x, color.y, color.z, 0.0, 0.0}};
    std::vector<int> indices;
    std::vector<double> distance2;
    if (mTree->SearchRadius(point, mRadius, indices, distance2) > 0) {
      assert(!indices.empty() && indices[0] >= 0);
      size_t idx = static_cast<size_t>(indices[0]);
      assert(idx < mPointTextureMap.size());
      glm::vec2 uv = mPointTextureMap[idx];
      vertex[Renderer::VA_U] = uv.x;
      vertex[Renderer::VA_V] = uv.y;
      vertices.row(withUv) = vertex.cast<float>();
      indexMap.push_back(withUv);
      withUv++;
    } else {
      withoutUv--;
      vertices.row(withoutUv) = vertex.cast<float>();
      indexMap.push_back(withoutUv);
    }
  }
  assert(withUv == withoutUv);
  assert(indexMap.size() == n);

  mTexturedTriangles = 0;
  std::vector<uint32_t> indicesUv, indicesNoUv;
  indicesUv.reserve(mMesh.triangles_.size() * 3);
  indicesNoUv.reserve(mMesh.triangles_.size() * 3);
  for (const Vector3i &tri : mMesh.triangles_) {
    assert(tri[0] >= 0 && tri[1] >= 0 && tri[2] >= 0);
    uint32_t a = indexMap[tri[0]];
    uint32_t b = indexMap[tri[1]];
    uint32_t c = indexMap[tri[2]];
    if (a < withUv && b < withUv && c < withUv) {
      indicesUv.push_back(a);
      indicesUv.push_back(b);
      indicesUv.push_back(c);
      mTexturedTriangles++;
    } else {
      indicesNoUv.push_back(a);
      indicesNoUv.push_back(b);
      indicesNoUv.push_back(c);
    }
  }

  mVerticesWithUVs = static_cast<size_t>(withUv);

  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
  r.addTriangleMesh(vertices.block(0, 0, withUv, vertices.cols()), indicesUv);
  r.addTriangleMesh(vertices, indicesNoUv);
  r.uploadBuffer();

  mHasMeshes = true;
}
