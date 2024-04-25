#!/usr/bin/env python3
"""Utility to rectify the stereo pairs of a dataset.

The rectified frames produced by this script can then be feed to some
stereo matching algorithm or neural network.

The script uses the calibration data created with stereo-calibrate.py.

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.
"""
import argparse
from pathlib import Path
import sys

import cv2
import numpy as np


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
    "-c", "--calibration", type=Path, default=Path("calibration.npz")
)
parser.add_argument(
    "-i", "--interpolation", choices=interpolations.keys(), default="lanczos4"
)
parser.add_argument("directory", type=Path, default=Path("."))
args = parser.parse_args()


def list_frames(side):
    return {
        f.name.split("-")[0]: f
        for f in (args.directory / f"ir-{side}").glob("*.png")
    }


left = list_frames("left")
right = list_frames("right")
if set(left.keys()) != set(right.keys()):
    sys.exit("Some frames are not paired!")
    sys.exit(1)
maps = np.load(args.calibration)
interp = interpolations[args.interpolation]

dir_l = args.directory / "rectified-left"
dir_l.mkdir(exist_ok=True)
dir_r = args.directory / "rectified-right"
dir_r.mkdir(exist_ok=True)

for k, l in left.items():
    r = right[k]
    im_l = cv2.imread(str(l))
    im_r = cv2.imread(str(r))
    re_l = cv2.remap(im_l, maps["map_l_x"], maps["map_l_y"], interp)
    re_r = cv2.remap(im_r, maps["map_r_x"], maps["map_r_y"], interp)
    cv2.imwrite(str(dir_l / l.name), re_l)
    cv2.imwrite(str(dir_r / r.name), re_r)
