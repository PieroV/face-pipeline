#!/usr/bin/env python3
import argparse
from collections import namedtuple
import json
from pathlib import Path

import numpy as np
import open3d as o3d
from PIL import Image


SourceCloud = namedtuple("SourceCloud", ["pcd", "uvs", "kdtree", "offset"])

parser = argparse.ArgumentParser()
parser.add_argument(
    "-t",
    "--threshold",
    help="Threshold on the depth for the combined file",
    type=float,
)
parser.add_argument(
    "-v",
    "--voxel-length",
    help="The size of a voxel",
    type=float,
    required=True,
)
parser.add_argument(
    "-s",
    "--volume-size",
    help="The size the TSDF volume",
    type=float,
)
parser.add_argument(
    "-p",
    "--export-pcd",
    help="File to which export the merged PCD",
)
parser.add_argument(
    "-m",
    "--export-model",
    help="File to which export the merged model",
)
parser.add_argument(
    "--export-npz",
    help="Export the merged model as numpy data",
)
args = parser.parse_args()
trunc = args.threshold if args.threshold and args.threshold > 0 else 6.0
with open("camera.json") as f:
    j = json.load(f)
intr = o3d.camera.PinholeCameraIntrinsic()
intr.set_intrinsics(
    j["width"], j["height"], j["fx"], j["fy"], j["ppx"], j["ppy"]
)

offsets = {
    "32": [0, 0],
    "756": [768, 0],
    "480": [0, 664],
    "521": [768, 664],
    "811": [0, 1328],
    "581": [768, 1328],
}

if args.volume_size:
    hsize = args.volume_size * 0.5
    volume = o3d.pipelines.integration.UniformTSDFVolume(
        args.volume_size,
        int(args.volume_size / args.voxel_length),
        sdf_trunc=0.04,
        color_type=o3d.pipelines.integration.TSDFVolumeColorType.RGB8,
        origin=np.array([-hsize, -hsize, -hsize]),
    )
else:
    volume = o3d.pipelines.integration.ScalableTSDFVolume(
        args.voxel_length,
        sdf_trunc=0.04,
        color_type=o3d.pipelines.integration.TSDFVolumeColorType.RGB8,
    )

pcds = []
with open("data.json") as f:
    pcd_data = json.load(f)
for p in pcd_data:
    filename = Path(p["filename"])
    if p["hidden"]:
        print(f"Skipping {filename}")
        continue
    print(f"Adding {filename}")
    base_path = filename.parent
    num = filename.stem
    T = np.array(p["matrix"]).reshape((4, 4)).T
    mask = base_path / f"mask/{num}.png"
    if mask.exists():
        color = np.array(Image.open(mask, "r"))
        mask = (color[:, :, 3] > 127).astype(np.uint16)
        depth = np.array(Image.open(base_path / f"depth/{num}.png", "r"))
        depth = depth.astype(np.uint16) * mask
        valid = np.logical_and(depth != 0, depth < trunc * 1000)
        uvs = np.where(valid)
        uvs = np.array((uvs[1], uvs[0])).T
        coords = (uvs - np.array((j["ppx"], j["ppy"]))) / np.array(
            (j["fx"], j["fy"])
        )
        coords = np.append(
            coords, depth[valid].copy().reshape((-1, 1)) / 1000.0, axis=1
        )
        coords[:, 0] *= coords[:, 2]
        coords[:, 1] *= coords[:, 2]
        pcd = o3d.geometry.PointCloud(
            o3d.utility.Vector3dVector(coords)
        ).transform(T)
        pcd.colors = o3d.utility.Vector3dVector(
            color[valid, :3].reshape((-1, 3)) / 255.0
        )
        tree = o3d.geometry.KDTreeFlann(pcd)
        pcds.append(SourceCloud(pcd, uvs, tree, np.array(offsets[num])))

        color = o3d.geometry.Image(np.array(color[:, :, :3]))
        depth = o3d.geometry.Image(depth)
    else:
        color = o3d.io.read_image(str(base_path / f"rgb/{num}.jpg"))
        depth = o3d.io.read_image(str(base_path / f"depth/{num}.png"))
    rgbd = o3d.geometry.RGBDImage.create_from_color_and_depth(
        color, depth, 1000.0, trunc, False
    )
    volume.integrate(rgbd, intr, np.linalg.inv(T))
pcd = volume.extract_point_cloud()
o3d.visualization.draw_geometries([pcd])
if args.export_pcd:
    o3d.io.write_point_cloud(args.export_pcd, pcd)
mesh = volume.extract_triangle_mesh()
o3d.visualization.draw_geometries([mesh])
if args.export_model:
    triangles = np.asarray(mesh.triangles)
    vertices = np.asarray(mesh.vertices)
    uvs = np.zeros((triangles.shape[0] * 3, 2))
    for idx, vidx in enumerate(triangles):
        min_dist = 1000
        min_neighs = None
        min_source = None
        for source in pcds:
            tot_dist = 0
            neighs = []
            for vi in vidx:
                v = vertices[vi]
                search = source.kdtree.search_knn_vector_3d(v, 1)
                neighs.append(search[1][0])
                tot_dist += search[2][0]
            if tot_dist < min_dist:
                min_dist = tot_dist
                min_neighs = neighs
                min_source = source
            # TODO: Maximize also the area of the covered texture
            if tot_dist < args.voxel_length:
                break
        uvs[idx * 3] = min_source.offset + min_source.uvs[min_neighs[0]]
        uvs[idx * 3 + 1] = min_source.offset + min_source.uvs[min_neighs[1]]
        uvs[idx * 3 + 2] = min_source.offset + min_source.uvs[min_neighs[2]]
    uvs /= 2048
    uvs[:, 1] = 1 - uvs[:, 1]
    mesh.triangle_uvs = o3d.utility.Vector2dVector(uvs)

    o3d.io.write_triangle_mesh(args.export_model, mesh)
if args.export_npz:
    np.savez(
        args.export_npz,
        vertices=np.asarray(mesh.vertices),
        triangles=np.asarray(mesh.triangles),
    )
# o3d.visualization.draw_geometries(pcds)
