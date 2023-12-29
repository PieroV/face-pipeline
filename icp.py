#!/usr/bin/env python3
import json
import sys

import numpy as np
import open3d as o3d

with open("data.json") as f:
    pcd_data = json.load(f)


def load_pcd(p):
    bin_data = np.fromfile(p["filename"], np.float32).reshape((-1, 6))
    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(bin_data[:, :3])
    pcd.colors = o3d.utility.Vector3dVector(bin_data[:, 3:])
    pcd = pcd.transform(np.array(p["matrix"]).reshape((4, 4)).T)
    pcd.estimate_normals(
        search_param=o3d.geometry.KDTreeSearchParamHybrid(
            radius=0.1, max_nn=30
        )
    )
    color = np.array(p["color"])
    pcd = pcd.paint_uniform_color(color)
    pcd = pcd.voxel_down_sample(voxel_size=0.005)
    return pcd


# pcds = []
# downsampled = []
# for p in pcd_data:
#     pcds.append(load_pcd(p))
# o3d.visualization.draw_geometries(pcds)

try:
    reference_idx = int(sys.argv[1])
    # target = downsampled[reference_idx]
    target = load_pcd(pcd_data[reference_idx])
except ValueError:
    target = o3d.io.read_point_cloud(sys.argv[1])

change_idx = int(sys.argv[2])
print(f"Transforming {pcd_data[change_idx]['filename']}")
# source = downsampled[change_idx]
source = load_pcd(pcd_data[change_idx])
o3d.visualization.draw_geometries([source, target])
threshold = 0.01
reg = o3d.pipelines.registration.registration_icp(
    source,
    target,
    threshold,
    np.eye(4),
    o3d.pipelines.registration.TransformationEstimationPointToPlane(),
)
source = source.transform(reg.transformation)
# downsampled[change_idx] = (
#     downsampled[change_idx].transform(reg.transformation)
# )
# pcds[change_idx] = pcds[change_idx].transform(reg.transformation)

T = np.array(pcd_data[change_idx]["matrix"]).reshape((4, 4)).T
pcd_data[change_idx]["matrix"] = (
    (reg.transformation.dot(T)).T.reshape((16,)).tolist()
)
pcd_data[change_idx]["rawMatrix"] = True
with open("data.json", "w") as f:
    json.dump(pcd_data, f, indent=2)

# o3d.visualization.draw_geometries([downsampled[change_idx], target])
o3d.visualization.draw_geometries([source, target])
# o3d.visualization.draw_geometries(pcds)
