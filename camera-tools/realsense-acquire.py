#!/usr/bin/env python3
"""Utility to quickly acquire some frames with a RealSense.

The RealSense viewer and the bag data format should be preferred for
complete scans, since they include all the data you can get from your
sensors and they allow for post-processing.

However, for some purposes such as calibration, you don't need a
continuous acquisition, only some frames are enough. This program should
help for this.

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
import pyrealsense2 as rs

parser = argparse.ArgumentParser()
parser.add_argument(
    "-r", "--resolution", type=int, nargs=2, default=(1280, 720)
)
parser.add_argument("-f", "--fps", type=int, default=30)
parser.add_argument(
    "--color", action=argparse.BooleanOptionalAction, default=True
)
parser.add_argument(
    "--depth", action=argparse.BooleanOptionalAction, default=False
)
parser.add_argument(
    "--projector", action=argparse.BooleanOptionalAction, default=True
)
parser.add_argument("-d", "--directory", type=Path, default=Path("."))
parser.add_argument("--reset", action="store_true")
args = parser.parse_args()

ctx = rs.context()
if len(ctx.devices) == 0:
    print("No devices available.", file=sys.stderr)
    sys.exit(1)

device = ctx.devices[0]
name = device.get_info(rs.camera_info.name)
usb_ver = device.get_info(rs.camera_info.usb_type_descriptor)
print(f"Found {name} (connected through USB {usb_ver}).")

if args.reset:
    device.hardware_reset()

depth_sensor = device.first_depth_sensor()

pipeline = rs.pipeline(ctx)
config = rs.config()
config.enable_device(device.get_info(rs.camera_info.serial_number))
config.enable_stream(
    rs.stream.infrared,
    1,
    args.resolution[0],
    args.resolution[1],
    rs.format.y8,
    args.fps,
)
config.enable_stream(
    rs.stream.infrared,
    2,
    args.resolution[0],
    args.resolution[1],
    rs.format.y8,
    args.fps,
)
if args.color:
    config.enable_stream(
        rs.stream.color,
        args.resolution[0],
        args.resolution[1],
        rs.format.bgr8,
        args.fps,
    )
if args.depth:
    config.enable_stream(
        rs.stream.depth,
        args.resolution[0],
        args.resolution[1],
        rs.format.z16,
        args.fps,
    )
profile = pipeline.start(config)

base_dir = args.directory
base_dir.mkdir(exist_ok=True)
ir_left_dir = base_dir / "ir-left"
ir_left_dir.mkdir(exist_ok=True)
ir_right_dir = base_dir / "ir-right"
ir_right_dir.mkdir(exist_ok=True)
color_dir = base_dir / "rgb"
if args.color:
    color_dir.mkdir(exist_ok=True)
depth_dir = base_dir / "depth"
if args.depth:
    depth_dir.mkdir(exist_ok=True)

stream_info = []
for stream in profile.get_streams():
    intrinsics = stream.as_video_stream_profile().intrinsics
    info = {
        "name": stream.stream_name(),
        "ppx": intrinsics.ppx,
        "ppy": intrinsics.ppy,
        "fx": intrinsics.fx,
        "fy": intrinsics.fy,
        "width": intrinsics.width,
        "height": intrinsics.height,
    }
    if stream.stream_type() == rs.stream.depth:
        info["scale"] = depth_sensor.get_depth_scale()
    stream_info.append(info)
with (base_dir / "info.json").open("w") as f:
    json.dump(stream_info, f, indent=2)

# Notice: for some reason this seems not to work on my Linux system.
# However, it worked with the same RealSense on Windows.
if not args.projector:
    depth_sensor.set_option(rs.option.laser_power, 0.0)
    depth_sensor.set_option(rs.option.emitter_enabled, 0.0)

num = 0
record = False
while True:
    frames = pipeline.wait_for_frames()

    ir_left = frames.get_infrared_frame(1)
    data_left = np.asarray(ir_left.get_data())
    cv2.imshow("IR Left", data_left)
    ir_right = frames.get_infrared_frame(2)
    data_right = np.asarray(ir_right.get_data())
    cv2.imshow("IR Right", data_right)
    if args.color:
        color = frames.get_color_frame()
        data_color = np.asarray(color.get_data())
        cv2.imshow("Color", data_color)
    if args.depth:
        depth = frames.get_depth_frame()
        data_depth = np.asarray(depth.get_data())
        d8 = cv2.normalize(data_depth, None, 0, 255, cv2.NORM_MINMAX).astype(
            np.uint8
        )
        dcm = cv2.applyColorMap(d8, cv2.COLORMAP_VIRIDIS)
        cv2.imshow("Depth", dcm)

    key = cv2.waitKey(1)
    if key == 27:
        break

    # Press "r" to record a sequence, any key to stop.
    if key == ord("r"):
        record = True
    elif record and key != -1:
        record = False

    # Or press any other key for a single shot.
    if key != -1 or record:
        cv2.imwrite(
            str(ir_left_dir / f"{num}-{ir_left.frame_number}.png"),
            data_left,
        )
        cv2.imwrite(
            str(ir_right_dir / f"{num}-{ir_right.frame_number}.png"),
            data_right,
        )
        if args.color:
            cv2.imwrite(
                str(color_dir / f"{num}-{color.frame_number}.png"),
                data_color,
            )
        if args.depth:
            cv2.imwrite(
                str(depth_dir / f"{num}-{depth.frame_number}.png"),
                data_depth,
            )
        num += 1

pipeline.stop()
