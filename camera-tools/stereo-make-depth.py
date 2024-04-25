#!/usr/bin/env python3
"""Utility to convert disparity to depth and register color data.

Often, in RGBD sensors color and depth data are captured with phisically
different sensors. Therefore, they have a different view.
This program aligns the color data to the depth data.
It is thought for stereo sensors that use disparity (such as the Intel
RealSense D400 series), but it could be easily modified to start from
the depth data.
Indeed, this script does not require the RealSense SDK, but only OpenCV,
Numpy, and Pillow.

It uses PIL to save images rather than cv2.imwrite because PIL allows to
add text to PNG images, which we use to use a per-frame scale factor,
which in turn allows us to use the whole 16-bit space and reduce the
quantization error.

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.
"""
import argparse
import json
from pathlib import Path
import sys

import cv2
import numpy as np
from PIL import Image, PngImagePlugin


interpolations = {
    "nearest": cv2.INTER_NEAREST,
    "linear": cv2.INTER_LINEAR,
    "cubic": cv2.INTER_CUBIC,
    "area": cv2.INTER_AREA,
    "lanczos4": cv2.INTER_LANCZOS4,
    "linear-exact": cv2.INTER_LINEAR_EXACT,
    "nearest-exact": cv2.INTER_NEAREST_EXACT,
}

parser = argparse.ArgumentParser()
parser.add_argument(
    "-i",
    "--interpolation",
    choices=interpolations.keys(),
    default="lanczos4",
)
parser.add_argument(
    "-c",
    "--calibration",
    type=Path,
    help="The path to a calibration file produced with stereo-calibrate.py.",
)
parser.add_argument(
    "--keep-invalid-color",
    action="store_true",
    help="Do not set depth to 0 if color data is not available.",
)
parser.add_argument(
    "-s",
    "--scale",
    type=float,
    help="The scale factor to use. If not specified, every frame will use its "
    "own, whose value will be saved as a PNG comment.",
)
parser.add_argument(
    "-f", "--color-format", choices=["jpg", "png", "bmp"], default="png"
)
parser.add_argument(
    "--optimize",
    action=argparse.BooleanOptionalAction,
    default=True,
    help="Use a higher compression, at the expense of a higher save time.",
)
parser.add_argument(
    "source",
    type=Path,
    help="A dataset directory with color frames and disparity data.",
)
parser.add_argument(
    "destination",
    type=Path,
    help="A destination directory where the depth and aligned color data will be saved. "
    "The program will exit if it already exists.",
)
args = parser.parse_args()

destination = args.destination
if destination.exists():
    print("The destination directory already exists.", file=sys.stderr)
    sys.exit(1)
destination.mkdir()
color_dest = destination / "rgb"
color_dest.mkdir()
depth_dest = destination / "depth"
depth_dest.mkdir()

calibration = np.load(
    args.calibration if args.calibration else args.source / "calibration.npz"
)
width, height = int(calibration["im_size"][0]), int(calibration["im_size"][1])
intrinsic = calibration["intrinsic"]
with (destination / "camera.json").open("w") as f:
    json.dump(
        {
            "width": width,
            "height": height,
            "fx": intrinsic[0, 0],
            "fy": intrinsic[1, 1],
            "ppx": intrinsic[0, 2],
            "ppy": intrinsic[1, 2],
            "scale": args.scale if args.scale else 1.0,
        },
        f,
        indent=2,
    )

disparities = {
    fn.stem.split("-")[0]: fn
    for fn in args.source.glob("disparity/*_disp.pfm")
}
frames = []
for fn in args.source.glob("rgb/*"):
    if fn.suffix not in [".png", ".bmp", ".jpg"]:
        continue
    idx = fn.name.split("-")[0]
    if idx in disparities:
        frames.append((disparities.pop(idx), fn))
    else:
        print(f"Unmatched color frame {fn}.")
for fn in disparities.values():
    print(f"Unmatched disparity {fn}.")

for disp_fn, color_fn in frames:
    disp = cv2.imread(str(disp_fn), cv2.IMREAD_UNCHANGED)
    if disp.shape != (height, width):
        print(f"Unexpected size for {disp_fn}")
        continue
    color = cv2.imread(str(color_fn))
    if color.shape[:2] != (height, width):
        print(f"Unexpected size for {color_fn}")
        continue

    points = cv2.reprojectImageTo3D(disp, calibration["Q"])
    rep, _ = cv2.projectPoints(
        points.reshape((-1, 3)),
        calibration["color_r"],
        calibration["color_t"],
        calibration["color_intrinsic"],
        calibration["color_distortion"],
    )

    rep = rep.reshape((disp.shape[0], disp.shape[1], 2))
    reg_depth = np.zeros(disp.shape, np.float32)
    reg_depth[disp > 0] = calibration["Bf"] / disp[disp > 0]
    if not args.keep_invalid_color:
        reg_depth[rep[:, :, 0] < 0] = 0
        reg_depth[rep[:, :, 1] < 0] = 0
        reg_depth[rep[:, :, 0] > color.shape[1]] = 0
        reg_depth[rep[:, :, 1] > color.shape[0]] = 0
    scale = args.scale if args.scale else (65535 / reg_depth.max())
    reg_depth = (reg_depth * scale).astype(np.uint16)
    reg_color = cv2.remap(color, rep, None, cv2.INTER_LANCZOS4)

    # -9 is to remove _disp.pfm.
    reg_depth_fn = depth_dest / (disp_fn.name[:-9] + ".png")
    reg_image = Image.fromarray(reg_depth)
    pnginfo = PngImagePlugin.PngInfo()
    pnginfo.add_text("depth-scale", str(scale))
    reg_image.save(reg_depth_fn, pnginfo=pnginfo, optimize=args.optimize)
    reg_color_fn = (color_dest / color_fn.name).with_suffix(
        "." + args.color_format
    )
    Image.fromarray(reg_color[:, :, ::-1]).save(
        reg_color_fn, optimize=args.optimize
    )
    print(f"Saved {reg_color_fn.name}, {reg_depth_fn.name}.")
