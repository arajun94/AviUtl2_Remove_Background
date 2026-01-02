# ffmpeg -i input.mp4 -vf fps=24 -s 1920x1080 -q:v 2 -start_number 0 frames/'%03d.jpg'


import os
# if using Apple MPS, fall back to CPU for unsupported ops
os.environ["PYTORCH_ENABLE_MPS_FALLBACK"] = "1"
import numpy as np
import torch
import matplotlib
import matplotlib.pyplot as plt
from PIL import Image
import cv2
import glob


def show_mask(mask, ax, obj_id=None, random_color=False):
    if random_color:
        color = np.concatenate([np.random.random(3), np.array([0.6])], axis=0)
    else:
        cmap = plt.get_cmap("tab10")
        cmap_idx = 0 if obj_id is None else obj_id
        color = np.array([*cmap(cmap_idx)[:3], 0.6])
    h, w = mask.shape[-2:]
    mask_image = mask.reshape(h, w, 1) * color.reshape(1, 1, -1)
    ax.imshow(mask_image)


def show_points(coords, labels, ax, marker_size=200):
    pos_points = coords[labels==1]
    neg_points = coords[labels==0]
    ax.scatter(pos_points[:, 0], pos_points[:, 1], color='green', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)
    ax.scatter(neg_points[:, 0], neg_points[:, 1], color='red', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)


# select the device for computation
if torch.cuda.is_available():
    device = torch.device("cuda")
elif torch.backends.mps.is_available():
    device = torch.device("mps")
else:
    device = torch.device("cpu")
print(f"using device: {device}")

if device.type == "cuda":
    # use bfloat16 for the entire notebook
    torch.autocast("cuda", dtype=torch.bfloat16).__enter__()
    # turn on tfloat32 for Ampere GPUs (https://pytorch.org/docs/stable/notes/cuda.html#tensorfloat-32-tf32-on-ampere-devices)
    if torch.cuda.get_device_properties(0).major >= 8:
        torch.backends.cuda.matmul.allow_tf32 = True
        torch.backends.cudnn.allow_tf32 = True
        torch.backends.cudnn.benchmark = True
elif device.type == "mps":
    print(
        "\nSupport for MPS devices is preliminary. SAM 2 is trained with CUDA and might "
        "give numerically different outputs and sometimes degraded performance on MPS. "
        "See e.g. https://github.com/pytorch/pytorch/issues/84936 for a discussion."
    )

# `video_dir` a directory of JPEG frames with filenames like `<frame_index>.jpg`
video_dir = "./frames"

# scan all the JPEG frame names in this directory
frame_names = [
    p for p in os.listdir(video_dir)
    if os.path.splitext(p)[-1] in [".jpg", ".jpeg", ".JPG", ".JPEG"]
]
frame_names.sort(key=lambda p: int(os.path.splitext(p)[0]))


sam2_checkpoint = "sam2.1_hiera_small.pt"
model_cfg = "configs/sam2.1/sam2.1_hiera_s.yaml"

from sam2.build_sam import build_sam2_video_predictor
predictor = build_sam2_video_predictor(model_cfg, sam2_checkpoint, device=device)
inference_state = predictor.init_state(video_path=video_dir)
predictor.reset_state(inference_state)

'''
#IMG_2004
prompts = [
    [
        np.array([[1100, 1200], [1100, 800], [1100, 1600], [1000, 1300], [1000, 1300], [1000, 1000], [1000, 1500], [1200, 1900], [1230, 1420], [920, 1700], [900, 1750], [950, 1600]], dtype=np.float32), 
        np.array([1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0], np.int32)
    ]
]
'''

'''
#IMG_1970
prompts = [
    [
        np.array([[200, 1000]], dtype=np.float32), 
        np.array([1], np.int32)
    ],
    [
        np.array([[3600, 1600]], dtype=np.float32), 
        np.array([1], np.int32)
    ]
]
'''

'''
#IMG_1970
prompts = [
    [
        np.array([[1400, 1000], [1500, 1500], [1300, 1200], [1300, 1000], [1450, 1400]], dtype=np.float32), 
        np.array([1, 1, 1, 1, 1], np.int32)
    ],
]
'''
#IMG_1970
prompts = [
    [
        np.array([[320, 500]], dtype=np.float32), 
        np.array([1], np.int32)
    ],
]

ann_frame_idx = 0

for ann_obj_id, [points,labels] in enumerate(prompts):
    _, out_obj_ids, out_mask_logits = predictor.add_new_points_or_box(
        inference_state=inference_state,
        frame_idx=ann_frame_idx,
        obj_id=ann_obj_id,
        points=points,
        labels=labels,
    )

# show the results on the current (interacted) frame
plt.figure(figsize=(9, 6))
plt.title(f"frame {ann_frame_idx}")
plt.imshow(Image.open(os.path.join(video_dir, frame_names[ann_frame_idx])))
show_points(points, labels, plt.gca())

frame = cv2.imread(os.path.join(video_dir, frame_names[ann_frame_idx]))
frame = cv2.cvtColor(frame, cv2.COLOR_BGR2BGRA)
frame_result = np.zeros_like(frame)
for i, out_obj_id in enumerate(out_obj_ids):
    out_mask = (out_mask_logits[i] > 0.0).cpu().numpy()
    show_points(*prompts[out_obj_id], plt.gca())
    show_mask(out_mask, plt.gca(), obj_id=out_obj_id)
    mask_2d = out_mask.squeeze() if out_mask.ndim == 3 else out_mask
    # Convert boolean mask to uint8 mask (0 or 255) for OpenCV
    cv2_mask = mask_2d.astype(np.uint8) * 255
    kernel = np.ones((16, 16), np.uint8)
    cv2_mask = cv2.morphologyEx(cv2_mask, cv2.MORPH_CLOSE, kernel)

    frame_result = frame
    frame_result[:, :, 3] = cv2_mask

    cv2.imwrite('./output_mask.png', cv2_mask)
    cv2.imwrite('./output_result.png', frame_result)
plt.show()