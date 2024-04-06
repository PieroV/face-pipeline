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
    : mApp(app) {
  if (indices.empty()) {
    throw std::invalid_argument("indices must not be empty.");
  }
  const Scene &scene = app.getScene();
  mTextures.reserve(indices.size());
  for (size_t idx : indices) {
    mTextures.push_back(std::make_unique<TextureData>(scene, idx, mUseMask));
    // We take for granted it'll never return a nullptr, but throw
    // std::bad_alloc instead.
    assert(mTextures.back());
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
    bool shouldUpdate = false;
    for (auto &tex : mTextures) {
      shouldUpdate =
          ImGui::Checkbox(tex->name.c_str(), &tex->active) || shouldUpdate;
    }
    if (shouldUpdate) {
      update();
    }

    ImGui::InputDouble("Search radius", &mRadius);
    if (ImGui::Button("Update")) {
      update();
    }
    if (ImGui::Checkbox("Use mask if available", &mUseMask)) {
      const Scene &scene = mApp.getScene();
      for (auto &d : mTextures) {
        d->updateTree(scene, mUseMask);
      }
      update();
    }
    if (ImGui::ColorEdit3("Default color", mDefaultColor.data())) {
      update();
    }
    if (ImGui::Button("Load mesh...")) {
      ImGui::OpenPopup("Load mesh");
    }
    if (mHasMeshes) {
      if (ImGui::Button("Save mesh...")) {
        ImGui::OpenPopup("Save mesh");
      }
      ImGui::SameLine();
      if (ImGui::Button("Export texture...")) {
        ImGui::OpenPopup("Export texture");
      }
    }
    if (!mMesh.IsEmpty()) {
      ImGui::Text("%zu vertices, %zu triangles", mMesh.vertices_.size(),
                  mMesh.triangles_.size());
    }
    if (mHasMeshes) {
      ImGui::Text("%u not textured triangles",
                  static_cast<uint32_t>(mNotTextured / 3));
    }
    if (ImGui::Button("Close")) {
      mApp.setState(std::make_unique<EditorState>(mApp));
    }

    fileModal("Load mesh", "Load", mLoadFilename,
              std::bind(&TextureLabState::loadMesh, this));
    fileModal("Save mesh", "Save", mSaveFilename,
              std::bind(&TextureLabState::saveMesh, this));
    fileModal("Export texture", "Export", mTextureFilename,
              std::bind(&TextureLabState::exportTexture, this));
  }
  ImGui::End();
}

void TextureLabState::fileModal(const char *title, const char *button,
                                std::string &filename,
                                const std::function<bool()> &func) {
  if (ImGui::BeginPopupModal(title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename", &filename);
    if (ImGui::Button(button)) {
      if (func()) {
        ImGui::CloseCurrentPopup();
      } else {
        ImGui::OpenPopup("Error");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::BeginPopupModal("Error", nullptr)) {
      ImGui::TextUnformatted(mErrorDesc.c_str());
      if (ImGui::Button("OK")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::EndPopup();
  }
}

bool TextureLabState::loadMesh() {
  open3d::geometry::TriangleMesh mesh;
  if (open3d::io::ReadTriangleMesh(mLoadFilename, mesh)) {
    mMesh = std::move(mesh);
    update();
    return true;
  } else {
    mErrorDesc = "Could not load the mesh.";
    return false;
  }
}

bool TextureLabState::saveMesh() {
  if (open3d::io::WriteTriangleMesh(mSaveFilename, mMesh)) {
    return true;
  } else {
    mErrorDesc = "Could not save the mesh.";
    return false;
  }
}

bool TextureLabState::exportTexture() {
  const auto &clouds = mApp.getScene().clouds;

  open3d::geometry::Image texture;
  {
    assert(!clouds.empty());
    const open3d::geometry::Image &color = clouds[0].getRgbdImage().color_;
    texture.Prepare(color.width_, 0, color.num_of_channels_,
                    color.bytes_per_channel_);
  }

  for (const auto &tex : mTextures) {
    assert(tex && tex->index < clouds.size());
    if (!tex->active) {
      continue;
    }

    const open3d::geometry::Image &color =
        clouds[tex->index].getRgbdImage().color_;
    if (color.width_ != texture.width_ ||
        color.num_of_channels_ != texture.num_of_channels_ ||
        color.bytes_per_channel_ != texture.bytes_per_channel_) {
      mErrorDesc =
          tex->name + " is in a format incompatible with the other textures.";
      return false;
    }

    texture.height_ += color.height_;
    texture.data_.insert(texture.data_.end(), color.data_.begin(),
                         color.data_.end());
  }

  if (texture.height_ <= 0) {
    mErrorDesc = "No textures have been selected.";
    return false;
  }

  if (open3d::io::WriteImage(mTextureFilename, texture)) {
    return true;
  } else {
    mErrorDesc = "Could not save the unified texture.";
    return false;
  }
}

void TextureLabState::render(const glm::mat4 &pv) {
  Renderer &r = mApp.getRenderer();
  r.beginRendering(pv);
  if (mHasMeshes) {
    r.renderIndexedMesh(0, glm::mat4(1.0f), false, 0, mNotTextured);
    glActiveTexture(GL_TEXTURE0);
    GLsizei offset = mNotTextured;
    for (const auto &tex : mTextures) {
      assert(tex);
      if (!tex->triangles) {
        continue;
      }
      tex->texture.bind();
      r.renderIndexedMesh(0, glm::mat4(1.0f), true, offset, tex->triangles);
      offset += tex->triangles;
    }
  }
  r.endRendering();
}

void TextureLabState::update() {
  // TODO: This is quite a mess. We should split it (however we might end up
  // creating a completely different UI for choosing textures, and eventually
  // only the tree lookup might remain).

  using namespace Eigen;

  mHasMeshes = false;
  mNotTextured = 0;
  if (mMesh.IsEmpty()) {
    return;
  }

  size_t n = mMesh.triangles_.size();
  Renderer::VertexMatrix vertices;
  vertices.conservativeResize(static_cast<Index>(n * 3), NoChange);
  std::vector<std::vector<uint32_t>> indices(mTextures.size());

  std::vector<Vector2d> uvOffsets(mTextures.size());
  uint32_t activeTextures = 0;
  {
    // At the moment, all frames must have the same size.
    int height = mApp.getScene().getCameraIntrinsic().height_;
    assert(height > 0);
    int heightOffset = 0;
    for (size_t i = 0; i < mTextures.size(); i++) {
      assert(mTextures[i]);
      uvOffsets[i] << 0, heightOffset + height;
      if (mTextures[i]->active) {
        heightOffset += height;
        activeTextures++;
      }
    }
    for (auto &offset : uvOffsets) {
      offset[1] = 1.0 - offset[1] / heightOffset;
    }
  }

  std::vector<uint32_t> flatIndices;
  flatIndices.reserve(n * 3);

  mMesh.triangle_uvs_.resize(n * 3, Vector2d(0.0, 0.0));
  Vector2f triUv[3];
  for (size_t i = 0, vi = 0; i < n; i++) {
    const Vector3i &tri = mMesh.triangles_[i];
    bool valid = false;
    size_t ti = 0;
    for (; ti < mTextures.size(); ti++) {
      assert(mTextures[ti]);
      const TextureData &tex = *mTextures[ti];
      if (!tex.active) {
        continue;
      }
      valid = true;
      for (size_t j = 0; j < 3; j++) {
        std::optional<Eigen::Vector2f> uv =
            tex.findPointUv(mMesh.vertices_[tri[j]], mRadius);
        if (uv) {
          triUv[j] = *uv;
        } else {
          valid = false;
          break;
        }
      }
      if (valid) {
        break;
      }
    }

    for (size_t j = 0; j < 3; j++) {
      if (valid) {
        indices[ti].push_back(static_cast<uint32_t>(vi));
        mMesh.triangle_uvs_[vi]
            << Vector2d(triUv[j][0], triUv[j][1] / activeTextures) +
                   uvOffsets[ti];
      } else {
        flatIndices.push_back(static_cast<uint32_t>(vi));
      }
      Renderer::Vertex v;
      v << mMesh.vertices_[tri[j]].cast<float>(), mDefaultColor, triUv[j];
      vertices.row(vi++) = v;
    }
  }

  mNotTextured = static_cast<GLsizei>(flatIndices.size());
  for (size_t i = 0; i < mTextures.size(); i++) {
    flatIndices.insert(flatIndices.end(), indices[i].begin(), indices[i].end());
    mTextures[i]->triangles = static_cast<GLsizei>(indices[i].size());
  }

  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
  r.addTriangleMesh(vertices, flatIndices);
  r.uploadBuffer();
  mHasMeshes = true;
}

TextureLabState::TextureData::TextureData(const Scene &scene, size_t idx,
                                          bool useMask)
    : index(idx) {
  const PointCloud &pcd = scene.clouds.at(idx);
  name = pcd.name;
  texture = Texture(pcd.getRgbdImage().color_);
  updateTree(scene, useMask);
}

void TextureLabState::TextureData::updateTree(const Scene &scene,
                                              bool useMask) {
  using namespace Eigen;
  tree.reset();
  uv.clear();
  const PointCloud &pcd = scene.clouds.at(index);

  // We need to project the point on our own because we want to keep the
  // association between point (3D) and uv (2D) coordinates.
  auto [points, pixels] = scene.unprojectDepth(pcd, useMask);
  if (points.empty()) {
    return;
  }

  const open3d::camera::PinholeCameraIntrinsic &camera =
      scene.getCameraIntrinsic();
  float size[] = {static_cast<float>(camera.width_),
                  static_cast<float>(camera.height_)};
  uv.reserve(pixels.size());
  for (const auto &px : pixels) {
    uv.emplace_back(px[0] / size[0], 1.0f - px[1] / size[1]);
  }

  tree.emplace(Map<const MatrixXd>(points[0].data(), 3,
                                   static_cast<Index>(points.size())));
}

std::optional<Eigen::Vector2f>
TextureLabState::TextureData::findPointUv(const Eigen::Vector3d &point,
                                          double radius) const {
  std::vector<int> indices;
  std::vector<double> distance2;
  if (tree && tree->SearchRadius(point, radius, indices, distance2) > 0) {
    assert(!indices.empty() && indices[0] >= 0);
    return {uv[static_cast<size_t>(indices[0])]};
  }
  return std::nullopt;
}
