# ffmpeg -i input.MOV -vf fps=24 -s 1920x1080 -q:v 2 -start_number 0 frames/'%03d.jpg'
# python sam2_video.py

# ffmpeg -f concat -safe 0 -i videocombine.txt -c copy output.mp4

# in 動画ファイルパス フレームレート 分割フレーム数
# opencvで点を選択
# out 出力をルダにマスクaviを保存

model_cfg = {
    "sam2.1_hiera_tiny": "configs/sam2.1/sam2.1_hiera_t.yaml",
    "sam2.1_hiera_small": "configs/sam2.1/sam2.1_hiera_s.yaml",
    "sam2.1_hiera_base_plus": "configs/sam2.1/sam2.1_hiera_b+.yaml",
    "sam2.1_hiera_large": "configs/sam2.1/sam2.1_hiera_l.yaml"
}

kernel_size = 4

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
import cv2
import plot
import shutil
import requests

from PIL import Image
import pillow_heif

pillow_heif.register_heif_opener()

base_dir = os.path.dirname(os.path.abspath(__file__)) # .../Python

def main():
    args = sys.argv[1:]
    if len(args) < 2:
        raise NotImplementedError("引数が足りません")
    
    original_image_path = args[0]
    original_image_name, original_video_ext = os.path.splitext(os.path.basename(original_image_path))

    cache_path = os.path.join(base_dir, "Cache")
    cache_image_path = os.path.join(cache_path,"input"+original_video_ext)
    cache_mask_path = os.path.join(cache_path,"output_mask.png")

    model_name = args[1]

    original_mask_path = os.path.join(os.path.dirname(original_image_path), original_image_name + f"_mask.png")
    i=0
    while(os.path.isfile(original_mask_path)):
        original_mask_path = os.path.join(os.path.dirname(original_image_path), original_image_name + f"_mask_{i}.png")
        i+=1

    print("入力画像: " + original_image_path)
    print("モデル名: " + model_name)

    if not model_name in model_cfg.keys():
        raise ValueError("不正なモデル名です")
        exit(1)

    model_filename = model_name + ".pt"
    model_path = os.path.join(base_dir, model_filename)
    model_url = 'https://dl.fbaipublicfiles.com/segment_anything_2/092824/' + model_filename
    print("キャッシュフォルダを初期化中...")
    if os.path.isdir(cache_path):
        shutil.rmtree(cache_path)
    os.makedirs(cache_path)

    shutil.copy(original_image_path, cache_image_path)
    print("ライブラリを読み込み中...")

    import torch
    from sam2.build_sam import build_sam2_video_predictor
    
    if torch.cuda.is_available():
        device = torch.device("cuda")
    elif torch.backends.mps.is_available():
        device = torch.device("mps")
    else:
        device = torch.device("cpu")

    # cudaの場合の最適化設定
    if device.type == "cuda":
        torch.autocast("cuda", dtype=torch.bfloat16).__enter__()

    print("モデルをロード中...")

    if not os.path.isfile(model_path):
        urlData = requests.get(model_url).content
        with open(model_path ,mode='wb') as f:
            f.write(urlData)

    predictor = build_sam2_video_predictor(model_cfg[model_name], model_path, device=device)

    print("フレーム読み込み中...")
    frame_folder = os.path.join(cache_path, "frames")
    os.makedirs(frame_folder, exist_ok=True)

    pil_img = Image.open(cache_image_path).convert("RGB")
    cv_img = np.array(pil_img)
    cv_img = cv_img[:, :, ::-1]
    cv2.imwrite(os.path.join(frame_folder, "00000.jpg"), cv_img)

    frame_path = os.path.join(frame_folder, "00000.jpg")
    frame_height, frame_width = cv_img.shape[:2]

    torch.set_grad_enabled(False)

    inference_state = predictor.init_state(video_path=frame_folder)
    predictor.reset_state(inference_state)

    print("別ウィンドウで点をプロットして下さい")

    # プロット
    ok = False
    plot_points = np.empty((0,3))
    while(ok==False):
        plot_points = plot.plotter(frame_path, plot_points)

        prompts = [
            [
                np.array(plot_points[:,0:2],dtype=np.float32),
                np.array(plot_points[:,2],np.int32)
            ]
        ]
        for ann_obj_id, [points,labels] in enumerate(prompts):
            _, out_obj_ids, out_mask_logits = predictor.add_new_points_or_box(
                inference_state=inference_state,
                frame_idx=0,
                obj_id=ann_obj_id,
                points=points,
                labels=labels,
            )

        ok = plot.previewer(frame_path, out_mask_logits, out_obj_ids, points, labels)

    out_mask = (out_mask_logits[out_obj_ids[0]] > 0.0).cpu().numpy().astype(np.uint8)

    mask_2d = out_mask.squeeze() if out_mask.ndim == 3 else out_mask

    cv2_mask = mask_2d.astype(np.uint8) * 255
    kernel = np.ones((kernel_size, kernel_size), np.uint8)
    cv2_mask = cv2.morphologyEx(cv2_mask, cv2.MORPH_CLOSE, kernel)
    cv2_mask = cv2.cvtColor(cv2_mask, cv2.COLOR_GRAY2BGR)

    frame_result = np.zeros((frame_height, frame_width, 3), dtype=np.uint8)
    frame_result = cv2.bitwise_or(frame_result, cv2_mask)

    cv2.imwrite(cache_mask_path, cv2_mask)
    shutil.copy(cache_mask_path, original_mask_path)

if __name__ == "__main__":
    main()