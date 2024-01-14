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

The `read-bag.py` script provides a way to extract the needed data from a bag
file created with the Intel® RealSense™ Viewer or SDK.

The essential files are aligned RGB and depth frames, and a `camera.json` with
intrinsic parameters.
In addition to that, the script creates an overlapped view of the frame for
manual selection of the frames.

### 2. Manual selection of the frames

From my tests, it is better to use just a reduced number of cherry-picked good
frames, rather than trying to automatically deal with all the frames.

### 3. Rough manual alignment of the frames

After choosing a few frames, you should align them roughly.
This replaces the global alignment step of Open3D's pipeline.

The advantage of doing it manually is that you'll be able to choose the final
placement already.

This is done with the `align` C++ tool.

### 4. Fine alignment with ICP

When you have an initial alignment, you can refine it with ICP.
It's done with the `icp.py` script.

### 5. TSDF

Finally, the point clouds are merged with Truncated Signed Distance Function.

## Dependencies

This project depends on the following libraries, that you should provide on your
own:

- [Open3D](https://www.open3d.org/)
- [GLFW3](https://www.glfw.org/)
- [GLM](https://glm.g-truc.net/)

In addition to that, it depends on the following libraries, that are included
either in the tree, or as git submodules:

- [Glad](https://glad.dav1d.de/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [JSON for Modern C++](https://github.com/nlohmann/json)

While this project’s code is dedicated to the public domain (you can refer to
the [Zero-Clause BSD license](https://opensource.org/license/0bsd/), if needed),
the depdencies have their own licensing terms.
