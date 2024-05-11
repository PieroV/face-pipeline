// This file is on its own, because:
// 1. libigl includes adds a long time to the build, as they bring their
//    implementation here;
// 2. this file needs to be built with -Wno-sign-compare because libigl uses
//    that flag;
// 3. I'm not sure of the license: I would have never understood how to use
//    bbw without reading tutorial/403_BoundedBiharmonicWeights/main.cpp
//    (especially the call to boundary_conditions). I don't know if this makes
//    this file a derivative of that tutorial (that does not contain a license
//    header!).

#include "glm/glm.hpp"

#include "igl/bbw.h"
#include "igl/boundary_conditions.h"

#include "open3d/geometry/TriangleMesh.h"

Eigen::MatrixXd computeBoneWeights(const open3d::geometry::TriangleMesh &mesh,
                                   const std::vector<glm::dvec3> &bonePos,
                                   const std::vector<int> &bonePairs) {
  using namespace Eigen;

  if (mesh.IsEmpty() || bonePos.empty() || bonePairs.empty()) {
    throw std::invalid_argument("Inputs cannot be empty.");
  }

  MatrixXd V = Map<const Matrix<double, Dynamic, 3, RowMajor>>(
      mesh.vertices_.front().data(), mesh.vertices_.size(), 3);
  MatrixXi Ele = Map<const Matrix<int, Dynamic, 3, RowMajor>>(
      mesh.triangles_.front().data(), mesh.triangles_.size(), 3);
  Map<const Matrix<double, Dynamic, 3, RowMajor>> C(
      reinterpret_cast<const double *>(bonePos.data()), bonePos.size(), 3);
  Map<const Matrix<int, Dynamic, 2, RowMajor>> BE(bonePairs.data(),
                                                  bonePairs.size() / 2, 2);
  VectorXi b;
  MatrixXd bc;
  MatrixXd W;

  igl::boundary_conditions(V, Ele, C, VectorXi(), BE, MatrixXi(), MatrixXi(), b,
                           bc);
  igl::BBWData bbw_data;
  bbw_data.active_set_params.max_iter = 8;
  bbw_data.verbosity = 2;
  if (!igl::bbw(V, Ele, b, bc, bbw_data, W)) {
    puts("Failed to compute weights!");
  }

  return W;
}
