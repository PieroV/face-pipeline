# Pipeline to reconstruct faces from RGBD scans (WIP)

This (work in progress) project is a pipeline to reconstruct a 3D model of a
face starting from RGBD scans.

However, additional steps required to actually use this model (e.g., retopology)
are not part of this project.
The goal of this project is producing a rough model based on acquired data to
use instead of a 3D sculpture.

This project is built on top of [Open3D](https://github.com/isl-org/Open3D) and
is basically an application of their pipeline.

## Steps

### 1. RGBD scans

The first step is to capture the input frames.

The `realsense-read-bag.py` script provides a way to extract the needed data
from a bag file created with the Intel® RealSense™ Viewer or SDK.

The essential files are aligned RGB and depth frames, and a `camera.json` with
intrinsic parameters.
In addition to that, the script creates an overlapped view of the frame for
manual selection of the frames.

### 2. Manual selection of the frames

From my tests, it is better to use just a reduced number of cherry-picked good
frames, rather than trying to automatically deal with all the frames.

The `align` program will load all the frames in memory (and also on the GPU), so
it isn't suited for running with all the frames of a scan.

### 3. Rough manual alignment of the frames

After choosing a few frames, you should align them roughly.
This replaces the global alignment step of Open3D's pipeline.

The advantage of doing it manually is that you'll be able to choose the final
placement already.

### 4. Fine automatic alignment with ICP

When you have an initial alignment, you can refine it with ICP.

You can select the reference point cloud and the one to align with the
checkboxes and then click on the align button.

Only one-to-one alignments are supported currently.

### 5. TSDF

Finally, the point clouds are merged with Truncated Signed Distance Function.

You can choose the frames to merge with the checkboxes, and then click on the
merge button.

You can then export both the merged point cloud and the triangle mesh generated
from it in any format supported by Open3D.

## Dependencies

This project is built upon [Open3D](https://www.open3d.org).

Building it isn't trivial, so you'll have to follow their instructions, or
download their prebuilt binaries, or install it from a package manager.

We use some of Open3D's dependencies directly:

- [Eigen](https://eigen.tuxfamily.org/)
- [GLFW3](https://www.glfw.org/)

It should be possible to configure CMake to look for them from Open3D, but
I haven't tried it, yet (my Open3D build is linked to the libraies provided by
my system, and in general I've built the project only on Debian testing).

In addition to that, the `align` program depends on the following libraries,
that are either included in the tree, or as git submodules:

- [Glad](https://glad.dav1d.de/)
- [GLM](https://glm.g-truc.net/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
- [JSON for Modern C++](https://github.com/nlohmann/json)
- [libigl](https://libigl.github.io/)
- [natsort](https://github.com/sourcefrog/natsort)

If needed, they will be built as static libraries.

While this project’s code is dedicated to the public domain (you can refer to
the [Zero-Clause BSD license](https://opensource.org/license/0bsd/), if needed),
the depdencies have their own licensing terms.
