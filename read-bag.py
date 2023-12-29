#!/usr/bin/env python3
import argparse
import json
import os

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
args = parser.parse_args()
if not args.input:
    print("No input paramater have been given.")
    print("For help type --help")
    exit()
thresh = args.threshold if args.threshold and args.threshold > 0 else 0.0

os.makedirs("rgb", exist_ok=True)
os.makedirs("depth", exist_ok=True)
os.makedirs("combined", exist_ok=True)
os.makedirs("overlapped", exist_ok=True)

# When aligning to depth, invalid depth points will be blacked also in
# the RGB frame.
# align_to = rs.stream.depth
align_to = rs.stream.color

pipeline = rs.pipeline()
config = rs.config()
rs.config.enable_device_from_file(config, args.input)
config.enable_stream(rs.stream.color, rs.format.rgb8, 30)
config.enable_stream(rs.stream.depth, rs.format.z16, 30)

profile = pipeline.start(config)
device = profile.get_device()
playback = device.as_playback()
playback.set_real_time(False)

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
with open("camera.json", "w") as f:
    json.dump(intr, f)

align = rs.align(align_to)
spatial = rs.spatial_filter()
temporal = rs.temporal_filter()

prev = -1
i = 0
while True:
    frames = pipeline.wait_for_frames()
    if (playback.get_position() - prev) < 1e6:
        print(f"Skipping frame {i}: {playback.get_position() - prev}")
        continue
    aligned_frames = align.process(frames)
    color_frame = np.asarray(aligned_frames.get_color_frame().get_data())
    color_frame = cv2.cvtColor(color_frame, cv2.COLOR_BGR2RGB)
    cv2.imwrite(f"rgb/{i}.jpg", color_frame)
    depth = aligned_frames.get_depth_frame()
    # depth = spatial.process(depth)
    # depth = temporal.process(depth)
    depth_frame = np.asarray(depth.get_data())
    cv2.imwrite(f"depth/{i}.png", depth_frame)
    trunc = depth_frame.max() / 1000.0 if thresh <= 0.0 else thresh
    depth_m = depth_frame / 1000.0
    depth_m[depth_m > trunc] = 0.0
    depth8 = np.clip((depth_m / trunc) * 255.0, 0, 255.0).astype(np.uint8)
    depth_cm = cv2.applyColorMap(depth8, cv2.COLORMAP_PLASMA)
    adj = cv2.hconcat([color_frame, depth_cm])
    over = (color_frame * 0.5 + depth_cm * 0.5).astype(np.uint8)
    cv2.imwrite(f"combined/{i}.png", adj)
    cv2.imwrite(f"overlapped/{i}.png", over)
    cv2.imshow("Combined", adj)
    cv2.imshow("Overlapped", over)
    cv2.waitKey(1)
    print(
        i,
        playback.get_position() * 1e-9,
        (playback.get_position() - prev) * 1e-9,
    )
    if playback.get_position() < prev:
        break
    prev = playback.get_position()
    i += 1
