/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <memory>

#include "open3d/geometry/TriangleMesh.h"

#include "BaseApplication.h"
#include "GLObjects.h"
#include "ShaderProgram.h"

#include "Bone.h"

class BonesApplication : public BaseApplication {
public:
  BonesApplication();

  void createGui() override;
  void render() override;
  void mouseClickCallback(int button, int action, int mods) override;

private:
  enum Uniforms : unsigned {
    U_PV,
    U_Bones,
    U_Skinned,
    U_PaintUniform,
    U_UniformColor,
    U_Max,
  };

  enum Mode {
    M_Edit,
    M_Pose,
  };

  void updateHandles();

  void uploadMesh();

  open3d::geometry::TriangleMesh mMesh;
  std::string mMeshPath;
  int mMode = M_Edit;

  std::shared_ptr<Bone> mRootBone;

  std::shared_ptr<Bone> mSelectedBone;
  std::shared_ptr<Bone> mHoveredBone;
  bool mAddingChild = false;

  GLObjects mGlObjects;
  ShaderProgram mShader;
  GLint mUniforms[U_Max];
  GLsizei mNumIndices = 0;
};
