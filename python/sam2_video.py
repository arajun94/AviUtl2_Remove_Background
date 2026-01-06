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

base_dir = os.path.dirname(os.path.abspath(__file__)) # .../Python
cache_path = os.path.join(base_dir, "Cache")

args = sys.argv[1:]
if len(args) < 5:
    raise NotImplementedError("引数が足りません")

original_video_path = args[0]
original_video_name, original_video_ext = os.path.splitext(os.path.basename(original_video_path))
input_video_path = os.path.join(cache_path,"input"+original_video_ext)
output_mask_path = os.path.join(cache_path,"output_mask.mp4")

scale = float(args[1])
start_sec = float(args[2])
end_sec = float(args[3])
model_name = args[4]

original_mask_path = os.path.join(os.path.dirname(original_video_path),original_video_name + f"_mask_{start_sec:.3f}_{end_sec:.3f}.mp4")
i=0
while(os.path.isfile(original_mask_path)):
    original_mask_path = os.path.join(os.path.dirname(original_video_path),original_video_name + f"_mask_{start_sec:.3f}_{end_sec:.3f}_{i}.mp4")
    i+=1

print("入力動画: " + original_video_path)
print("スケール: " + str(scale))
print("開始秒数: " + str(start_sec))
print("終了秒数: " + str(end_sec))
print("モデル名: " + model_name)

if not model_name in model_cfg.keys():
    raise ValueError("不正なモデル名です")
    exit(1)

model_filename = model_name + ".pt"
model_path = os.path.join(base_dir, model_filename)
model_url = 'https://dl.fbaipublicfiles.com/segment_anything_2/092824/' + model_filename


def video_to_frames(video_path, frame_folder, start, end, scale):
    os.makedirs(frame_folder, exist_ok=True)

    cap = cv2.VideoCapture(video_path)
    cap.set(cv2.CAP_PROP_POS_FRAMES, start)

    for frame_id in range(0, end-start):
        ret, frame = cap.read()
        if not ret:
            break

        frame = cv2.resize(frame, None, fx = scale, fy = scale)

        filename = os.path.join(frame_folder, f"{frame_id:05d}.jpg")
        cv2.imwrite(filename, frame)

        frame_id+=1
    cap.release()


def main():
    print("キャッシュフォルダを初期化中...")
    if os.path.isdir(cache_path):
        shutil.rmtree(cache_path)
    os.makedirs(cache_path)

    shutil.copy(original_video_path, input_video_path)
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
    
    print("動画をフレームへ分割中...")

    cap = cv2.VideoCapture(input_video_path)
    frame_num = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    start_frame_idx = int(start_sec*fps)
    end_frame_idx = int(end_sec*fps)

    print("モデルをロード中")

    if not os.path.isfile(model_path):
        urlData = requests.get(model_url).content
        with open(model_path ,mode='wb') as f:
            f.write(urlData)

    predictor = build_sam2_video_predictor(model_cfg[model_name], model_path, device=device)

    frame_folder = os.path.join(cache_path, "frames")
    video_to_frames(input_video_path, frame_folder, start_frame_idx, end_frame_idx, scale)

    print("フレーム読み込み中...")

    start_frame_path = os.path.join(frame_folder, "00000.jpg")

    torch.set_grad_enabled(False)

    inference_state = predictor.init_state(video_path=frame_folder)
    predictor.reset_state(inference_state)

    print("別ウィンドウで点をプロットして下さい")

    # プロット
    ok = False
    plot_points = np.empty((0,3))
    while(ok==False):
        plot_points = plot.plotter(start_frame_path, plot_points)

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

        ok = plot.previewer(start_frame_path, out_mask_logits, out_obj_ids, points, labels)

    print("マスクを生成中...")
    amp_device = "cuda" if device.type == "cuda" else None
    if amp_device == "cuda":
        autocast_ctx = torch.autocast("cuda", dtype=torch.bfloat16)
    else:
        from contextlib import nullcontext
        autocast_ctx = nullcontext()

    with autocast_ctx, torch.no_grad():
        video_segments = {}
        for out_frame_idx, out_obj_ids, out_mask_logits in predictor.propagate_in_video(
            inference_state,
            start_frame_idx=0,
            max_frame_num_to_track=end_frame_idx-start_frame_idx-1
            ):
            # GPU上にあるテンソルをすぐにCPUに移す
            frame_dict = {}
            for i, out_obj_id in enumerate(out_obj_ids):
                mask = (out_mask_logits[i] > 0.0).cpu().numpy().astype(np.uint8)
                frame_dict[out_obj_id] = mask
            video_segments[out_frame_idx] = frame_dict

            # GPUメモリを明示的に掃除
            del out_mask_logits
            torch.cuda.empty_cache()

    fourcc = cv2.VideoWriter_fourcc(*'mp4v') 
    writer_mask = cv2.VideoWriter(output_mask_path, fourcc, fps, (frame_width, frame_height))
    for out_frame_idx in range(0, frame_num):

        frame_result = np.zeros((frame_height, frame_width, 3), dtype=np.uint8)
        frame_result = cv2.resize(frame_result, None, fx = scale, fy = scale)
        #frame_result = np.full((h, w, 3), (0, 255, 0), dtype=np.uint8)

        if out_frame_idx in range(start_frame_idx, end_frame_idx):
            for out_obj_id, out_mask in video_segments[out_frame_idx-start_frame_idx].items():

                mask_2d = out_mask.squeeze() if out_mask.ndim == 3 else out_mask

                cv2_mask = mask_2d.astype(np.uint8) * 255
                kernel = np.ones((kernel_size, kernel_size), np.uint8)
                cv2_mask = cv2.morphologyEx(cv2_mask, cv2.MORPH_CLOSE, kernel)
                cv2_mask = cv2.cvtColor(cv2_mask, cv2.COLOR_GRAY2BGR)

                frame_result = cv2.bitwise_or(frame_result, cv2_mask)
                # frame_result = cv2.bitwise_and(frame_result, cv2.bitwise_not(cv2_mask)) + cv2.bitwise_and(frame, cv2_mask)
        
        # マスクフレームを描画
        frame_result = cv2.resize(frame_result, (frame_width, frame_height))
        writer_mask.write(frame_result)
    shutil.rmtree(frame_folder)
    writer_mask.release()

    shutil.copy(output_mask_path, original_mask_path)

if __name__ == "__main__":
    main()