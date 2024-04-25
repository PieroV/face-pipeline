#!/usr/bin/env python3
"""Utility to extract frames from a RealSense bag.

Bag files are probably the best way to create datasets from an Intel
RealSense, because they include all the possible metadata and allow
additional post-processing that would not be possible by saving only the
frame contents.

However, they are not very practical to work with, therefore this script
allows to convert them to standard image files.
It offers several options through command line arguments.

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.
"""
import argparse
import json
import queue
from pathlib import Path
import threading

import cv2
import numpy as np
import pyrealsense2 as rs


parser = argparse.ArgumentParser()
parser.add_argument("input", type=str, help="Path to the bag file")
parser.add_argument(
    "-t",
    "--threshold",
    type=float,
    help="Threshold on the depth for the combined file",
)
parser.add_argument(
    "-d",
    "--output-directory",
    type=Path,
    help="Destination directory for the data",
)
parser.add_argument(
    "-f", "--color-format", choices=["jpg", "png", "bmp"], default="jpg"
)
parser.add_argument(
    "--save", action=argparse.BooleanOptionalAction, default=True
)
parser.add_argument(
    "--save-heatmaps", action=argparse.BooleanOptionalAction, default=False
)
parser.add_argument(
    "--save-ir", action=argparse.BooleanOptionalAction, default=False
)
parser.add_argument("-a", "--align-to", choices=["color", "depth", "none"])
parser.add_argument(
    "--spatial-filter", action=argparse.BooleanOptionalAction, default=True
)
parser.add_argument(
    "--temporal-filter", action=argparse.BooleanOptionalAction, default=True
)

args = parser.parse_args()
if not args.input:
    print("No input paramater have been given.")
    print("For help type --help")
    exit()
thresh = args.threshold if args.threshold and args.threshold > 0 else 0.0

dest = args.output_directory
if not dest:
    dest = Path(".")
if args.save:
    dest.mkdir(exist_ok=True)
    (dest / "rgb").mkdir(exist_ok=True)
    (dest / "depth").mkdir(exist_ok=True)
    if args.save_heatmaps:
        (dest / "combined").mkdir(exist_ok=True)
        (dest / "overlapped").mkdir(exist_ok=True)
    if args.save_ir:
        (dest / "ir-left").mkdir(exist_ok=True)
        (dest / "ir-right").mkdir(exist_ok=True)

pipeline = rs.pipeline()
config = rs.config()
rs.config.enable_device_from_file(config, args.input)
config.enable_all_streams()
profile = pipeline.start(config)
device = profile.get_device()
playback = device.as_playback()
playback.set_real_time(False)

align_to = None
if args.align_to == "color":
    align_to = rs.stream.color
elif args.align_to == "depth":
    align_to = rs.stream.depth

if align_to is not None:
    align = rs.align(align_to)
    if args.save:
        vsp = profile.get_stream(align_to).as_video_stream_profile()
        intr = vsp.get_intrinsics()
        intr = {
            "ppx": intr.ppx,
            "ppy": intr.ppy,
            "fx": intr.fx,
            "fy": intr.fy,
            "width": vsp.width(),
            "height": vsp.height(),
            "scale": device.first_depth_sensor().get_depth_scale(),
        }
        with (dest / "camera.json").open("w") as f:
            json.dump(intr, f)

spatial = rs.spatial_filter()
temporal = rs.temporal_filter()

q = queue.Queue()
is_done = False


def io_runner():
    while not is_done:
        try:
            fname, data = q.get(True, 1)
            cv2.imwrite(fname, data)
        except queue.Empty:
            continue


io_threads = []
for _ in range(4 if args.save else 0):
    t = threading.Thread(target=io_runner)
    t.start()
    io_threads.append(t)

prev = -1
i = 0
while True:
    frames = pipeline.wait_for_frames()
    current = playback.get_position()
    print(prev / 1e9, current / 1e9)
    if current < prev:
        break
    prev = current

    if align_to is not None:
        frames = align.process(frames)

    color_frame = frames.get_color_frame()
    color = np.asarray(color_frame.get_data())
    color = cv2.cvtColor(color, cv2.COLOR_BGR2RGB)
    depth_frame = frames.get_depth_frame()
    if args.spatial_filter:
        depth_frame = spatial.process(depth_frame)
    if args.temporal_filter:
        depth_frame = temporal.process(depth_frame)
    depth = np.asarray(depth_frame.get_data())

    if args.save:
        q.put(
            (
                str(
                    dest
                    / f"rgb/{i}-{color_frame.frame_number}.{args.color_format}"
                ),
                color,
            )
        )
        q.put((str(dest / f"depth/{i}-{depth_frame.frame_number}.png"), depth))

    if args.save and args.save_ir:
        # 0 is the default sensor.
        channels = {"left": 1, "right": 2}
        for ch, idx in channels.items():
            ir = frames.get_infrared_frame(idx)
            q.put(
                (
                    str(dest / f"ir-{ch}/{i}-{ir.frame_number}.png"),
                    np.asarray(ir.get_data()),
                )
            )

    trunc = depth.max() / 1000.0 if thresh <= 0.0 else thresh
    depth_m = depth / 1000.0
    depth_m[depth_m > trunc] = 0.0
    depth8 = np.clip((depth_m / trunc) * 255.0, 0, 255.0).astype(np.uint8)
    depth_cm = cv2.applyColorMap(depth8, cv2.COLORMAP_PLASMA)
    adj = cv2.hconcat([color, depth_cm])
    over = (color * 0.5 + depth_cm * 0.5).astype(np.uint8)
    cv2.imshow("Combined", adj)
    cv2.imshow("Overlapped", over)

    if args.save and args.save_heatmaps:
        cv2.imwrite(str(dest / f"combined/{i}.png"), adj)
        cv2.imwrite(str(dest / f"overlapped/{i}.png"), over)

    if cv2.waitKey(1) == 27:
        break

    i += 1

is_done = True
for t in io_threads:
    t.join()
