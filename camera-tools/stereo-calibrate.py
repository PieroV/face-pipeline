#!/usr/bin/env python3
"""Utility to calibrate a stereo camera.

This utility allows to create the calibration data to rectify stereo
pairs.

It works with chessboard pattern.
Please check OpenCV's documentation on how to obtain one:
https://docs.opencv.org/4.x/da/d0d/tutorial_camera_calibration_pattern.html.

While this script does not depend on the Intel RealSense SDK, it has
been designed around RealSense cameras, which as a matter of fact have
two stereo pairs: two IR cameras, one one IR camera with the color
camera. Therefore, it outputs the maps to rectify the IR pairs, and the
extrinsic parameters of the color camera to then reproject the color
data on the view of the left IR camera.

This script might be modified only to produce the color-depth alignment
data also for RGBD sensors that use other technologies.

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.
"""
import argparse
from collections import namedtuple
from pathlib import Path

import cv2
import numpy as np


Calibration = namedtuple(
    "Calibration", ["intrinsic", "distortion", "rotations", "translations"]
)


class Calibrator:
    def __init__(self, shape, edge_size, directory, debug=False):
        self.shape = shape
        self.edge_size = edge_size
        self.dir = directory

        self.channels = ["ir-left", "ir-right", "rgb"]
        self.points = {}
        self.calibrations = {}

        self.list_frames()
        self.make_grid()

        for ch in self.channels:
            self.calibrate_channel(ch, debug)
        self.stereo_ir = self.calibrate_stereo(
            self.channels[0], self.channels[1]
        )
        print(f"IR error: {self.stereo_ir[0]}")
        self.rectify_ir()

        self.stereo_color = self.calibrate_stereo(
            self.channels[0], self.channels[2]
        )
        print(f"Color error: {self.stereo_color[0]}")
        # Transpose to invert, since it is a rotation.
        self.color_r = self.stereo_color[5].dot(self.rectify[0].T)
        self.color_t = self.stereo_color[6]

        self.save()

    def list_frames(self):
        self.frames = {}
        for s in self.channels:
            for fn in (self.dir / s).glob("*.png"):
                key = fn.name.split("-")[0]
                if key not in self.frames:
                    self.frames[key] = {}
                self.frames[key][s] = fn
        if not self.frames:
            raise ValueError(f"No frames found in {self.dir}")
        for k, v in self.frames.items():
            if len(v) != 3:
                raise ValueError(f"Key {k} not used in all channels.")
        # Reusing v is a little bit hacky ^_^
        im = cv2.imread(str(v[self.channels[0]]))
        self.im_size = im.shape[1], im.shape[0]

    def make_grid(self):
        y, x = np.meshgrid(range(shape[0]), range(shape[1]))
        points = np.zeros((shape[0] * shape[1], 3), dtype=np.float32)
        points[:, 0] = x.reshape((-1,)) * self.edge_size
        points[:, 1] = y.reshape((-1,)) * self.edge_size
        self.object_points = np.array([points for _ in self.frames])

    def calibrate_channel(self, ch, show=False):
        win_size = (11, 11)
        zero_zone = (-1, -1)
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TermCriteria_COUNT, 40, 0.001)

        points = []
        for fnames in self.frames.values():
            fname = fnames[ch]
            img = cv2.imread(str(fname), cv2.IMREAD_GRAYSCALE)
            if img.shape[::-1] != self.im_size:
                raise RuntimeError(f"Wrong size for {fname}")
            ret, corners = cv2.findChessboardCorners(img, self.shape, None)
            if not ret:
                raise RuntimeError(
                    f"Could not detect a chessboard in {fname}."
                )
            corners2 = cv2.cornerSubPix(
                img, corners, win_size, zero_zone, criteria
            )
            if show:
                color = cv2.cvtColor(img, cv2.COLOR_GRAY2RGB)
                result = cv2.drawChessboardCorners(color, shape, corners2, ret)
                cv2.imshow("Corners", result)
                cv2.waitKey(-1)
            idx = fname.stem.split("-")[0]
            if idx not in self.points:
                self.points[idx] = {}
            self.points[idx][ch] = corners2.reshape((-1, 2))
            points.append(self.points[idx][ch])

        calibration = cv2.calibrateCamera(
            self.object_points,
            points,
            self.im_size,
            None,
            None,
        )
        if not calibration[0]:
            raise RuntimeError(f"Calibration for {ch} failed.")
        self.calibrations[ch] = Calibration._make(calibration[1:])

    def calibrate_stereo(self, left, right):
        point_pairs = [[], []]
        for points in self.points.values():
            point_pairs[0].append(points[left])
            point_pairs[1].append(points[right])
        return cv2.stereoCalibrate(
            self.object_points,
            point_pairs[0],
            point_pairs[1],
            self.calibrations[left].intrinsic,
            self.calibrations[left].distortion,
            self.calibrations[right].intrinsic,
            self.calibrations[right].distortion,
            self.im_size,
        )

    def rectify_ir(self):
        self.rectify = cv2.stereoRectify(
            *self.stereo_ir[1:5],
            self.im_size,
            self.stereo_ir[5],
            self.stereo_ir[6],
        )
        self.map_l_x, self.map_l_y = cv2.initUndistortRectifyMap(
            self.stereo_ir[1],
            self.stereo_ir[2],
            self.rectify[0],
            self.rectify[2],
            self.im_size,
            cv2.CV_16SC2,
        )
        self.map_r_x, self.map_r_y = cv2.initUndistortRectifyMap(
            self.stereo_ir[3],
            self.stereo_ir[4],
            self.rectify[1],
            self.rectify[3],
            self.im_size,
            cv2.CV_16SC2,
        )

    def save(self):
        np.savez_compressed(
            self.dir / "calibration.npz",
            im_size=np.array(self.im_size),
            intrinsic=self.rectify[2][:, :3],
            Q=self.rectify[4],
            Bf=-self.rectify[3][0, 3],
            map_l_x=self.map_l_x,
            map_l_y=self.map_l_y,
            map_r_x=self.map_r_x,
            map_r_y=self.map_r_y,
            color_intrinsic=self.calibrations["rgb"].intrinsic,
            color_distortion=self.calibrations["rgb"].distortion,
            color_r=self.color_r,
            color_t=self.color_t,
        )


parser = argparse.ArgumentParser()
parser.add_argument(
    "-d",
    "--directory",
    type=Path,
    default=Path("."),
    help="Directory with the dataset of frames to use for the calibration.",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="If set, the various frames will be shown with the detected corners.",
)
parser.add_argument(
    "chessboard_size",
    type=int,
    nargs=2,
    help="Number of squares in the chessboard.",
)
parser.add_argument(
    "edge_size",
    type=float,
    help="The size of each square. Its unit is arbitrary, but it will "
    "influcence the output matrices.",
)
args = parser.parse_args()

shape = (args.chessboard_size[0] - 1, args.chessboard_size[1] - 1)
if shape[0] <= 0 or shape[1] <= 0:
    raise ValueError("Supplied invalid chessboard shape.")
c = Calibrator(shape, args.edge_size, args.directory, args.debug)
