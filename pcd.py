#!/usr/bin/env python3
import argparse
import json

import numpy as np
import open3d as o3d


parser = argparse.ArgumentParser()
parser.add_argument("number")
parser.add_argument(
    "-t",
    "--threshold",
    type=float,
    help="Threshold on the depth",
)
parser.add_argument("--export-raw", action="store_true")
args = parser.parse_args()
num = args.number
color = o3d.io.read_image(f"rgb/{num}.jpg")
depth = o3d.io.read_image(f"depth/{num}.png")
trunc = args.threshold if args.threshold and args.threshold > 0 else 6.0
rgbd = o3d.geometry.RGBDImage.create_from_color_and_depth(
    color, depth, 1000.0, trunc, False
)
with open("camera.json") as f:
    j = json.load(f)
intr = o3d.camera.PinholeCameraIntrinsic()
intr.set_intrinsics(
    j["width"], j["height"], j["fx"], j["fy"], j["ppx"], j["ppy"]
)
pcd = o3d.geometry.PointCloud.create_from_rgbd_image(rgbd, intr)
o3d.visualization.draw_geometries([pcd])

if args.export_raw:
    points = np.asarray(pcd.points)
    colors = np.asarray(pcd.colors)
    data = np.hstack((points, colors)).astype(np.float32)
    data.tofile(f"{args.number}.dat")
