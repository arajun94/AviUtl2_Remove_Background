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

image_size = 512
kernel_size = 4
maskmem = 7

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
import cv2
import plot
import shutil
import requests
import fps_trans

base_dir = os.path.dirname(os.path.abspath(__file__)) # .../Python


def video_to_frames(video_path, frame_folder, start, end):
    os.makedirs(frame_folder, exist_ok=True)

    cap = cv2.VideoCapture(video_path)
    cap.set(cv2.CAP_PROP_POS_FRAMES, start)

    for frame_id in range(0, end-start):
        ret, frame = cap.read()
        if not ret:
            break

        filename = os.path.join(frame_folder, f"{frame_id:05d}.jpg")
        cv2.imwrite(filename, frame)
        break

        frame_id+=1
    cap.release()


def load_model(model_name):
    model_filename = model_name + ".pt"
    model_path = os.path.join(base_dir, model_filename)
    model_url = 'https://dl.fbaipublicfiles.com/segment_anything_2/092824/' + model_filename
    if not os.path.isfile(model_path):
        urlData = requests.get(model_url).content
        with open(model_path ,mode='wb') as f:
            f.write(urlData)
    return model_path

def path_duplicate_numbering(file_path):
    base, ext = os.path.splitext(file_path)
    i = 0
    new_file_path = f"{base}_{i}{ext}"
    while os.path.isfile(new_file_path):
        i += 1
        new_file_path = f"{base}_{i}{ext}"
    return new_file_path


def main():
    args = sys.argv[1:]
    if len(args) < 7:
        raise NotImplementedError("引数が足りません")
    original_video_path = args[0]
    new_fps = float(args[1])
    start_sec = float(args[2])
    end_sec = float(args[3])
    model_name = args[4]
    kernel_size = int(args[5])
    image_size = int(args[6])

    print("入力動画: " + original_video_path)
    print("フレームレート変換: " + str(new_fps))
    print("開始秒数: " + str(start_sec))
    print("終了秒数: " + str(end_sec))
    print("モデル名: " + model_name)

    if not model_name in model_cfg.keys():
        raise ValueError("不正なモデル名です")

    print("キャッシュフォルダを初期化中...")

    cache_path = os.path.join(base_dir, "Cache")
    original_video_name, original_video_ext = os.path.splitext(os.path.basename(original_video_path))
    cache_video_path = os.path.join(cache_path,"input.mp4")
    cache_mask_path = os.path.join(cache_path,"output_mask.mp4")

    if os.path.isdir(cache_path):
        shutil.rmtree(cache_path)
    os.makedirs(cache_path)
    shutil.copy(original_video_path, cache_video_path)

    cap = cv2.VideoCapture(cache_video_path)
    fps = float(cap.get(cv2.CAP_PROP_FPS))
    cap.release()

    if new_fps != 0:
        if fps >= new_fps:
            fps = new_fps

        print("フレームレート変換中...")
        new_cache_video_path = os.path.join(cache_path,"input"+ f"{fps:.3f}" + "fps.mp4")
        fps_trans.fps_trans(cache_video_path, new_cache_video_path, fps)
        cache_video_path = new_cache_video_path

        original_mask_path = os.path.join(os.path.dirname(original_video_path),original_video_name + f"_mask_{start_sec:.3f}_{end_sec:.3f}_{new_fps:.3f}.mp4")
        original_mask_path = path_duplicate_numbering(original_mask_path)
        
        new_original_video_path = os.path.join(os.path.dirname(original_video_path),original_video_name + f"_{new_fps:.3f}" + original_video_ext)
        new_original_video_path = path_duplicate_numbering(new_original_video_path)

    else:
        original_mask_path = os.path.join(os.path.dirname(original_video_path),original_video_name + f"_mask_{start_sec:.3f}_{end_sec:.3f}.mp4")
        original_mask_path = path_duplicate_numbering(original_mask_path)

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

    model_path = load_model(model_name)

    print("動画をフレームへ分割中...")

    cap = cv2.VideoCapture(cache_video_path)
    fps = cap.get(cv2.CAP_PROP_FPS)
    frame_num = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    start_frame_idx = int(start_sec*fps)
    end_frame_idx = int(end_sec*fps)
    cap.release()

    if new_fps !=0:
        end_frame_idx+=1

    frame_folder = os.path.join(cache_path, "frames")
    video_to_frames(cache_video_path, frame_folder, start_frame_idx, end_frame_idx)

    print("フレーム読み込み中...")

    start_frame_path = os.path.join(frame_folder, "00000.jpg")

    hydra_overrides_extra = [
        # dynamically fall back to multi-mask if the single mask is not stable
        "++model.sam_mask_decoder_extra_args.dynamic_multimask_via_stability=true",
        "++model.sam_mask_decoder_extra_args.dynamic_multimask_stability_delta=0.05",
        "++model.sam_mask_decoder_extra_args.dynamic_multimask_stability_thresh=0.98",
        # the sigmoid mask logits on interacted frames with clicks in the memory encoder so that the encoded masks are exactly as what users see from clicking
        "++model.binarize_mask_from_pts_for_mem_enc=true",
        "++num_maskmem=" + str(maskmem),
        "++image_size=" + str(image_size),
        # fill small holes in the low-res masks up to `fill_hole_area` (before resizing them to the original video resolution)
        "++model.fill_hole_area=" + str(kernel_size)
    ]

    torch.set_grad_enabled(False)
    predictor = build_sam2_video_predictor(model_cfg[model_name], model_path, device=device, apply_postprocessing=False, hydra_overrides_extra=hydra_overrides_extra)
    inference_state = predictor.init_state(video_path=cache_video_path, async_loading_frames=False)
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
                frame_idx=start_frame_idx,
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
            start_frame_idx=start_frame_idx,
            max_frame_num_to_track=end_frame_idx-start_frame_idx-1
            ):
            frame_dict = {}
            for i, out_obj_id in enumerate(out_obj_ids):
                mask = (out_mask_logits[i] > 0.0).cpu().numpy().astype(np.uint8)
                frame_dict[out_obj_id] = mask
            video_segments[out_frame_idx] = frame_dict

            del out_mask_logits
            torch.cuda.empty_cache()

    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    writer_mask = cv2.VideoWriter(cache_mask_path, fourcc, fps, (frame_width, frame_height))
    for out_frame_idx in range(0, frame_num):

        frame_result = np.zeros((frame_height, frame_width, 3), dtype=np.uint8)
        #frame_result = np.full((h, w, 3), (0, 255, 0), dtype=np.uint8)

        if out_frame_idx in range(start_frame_idx, end_frame_idx):
            for out_obj_id, out_mask in video_segments[out_frame_idx].items():

                mask_2d = out_mask.squeeze() if out_mask.ndim == 3 else out_mask

                cv2_mask = mask_2d.astype(np.uint8) * 255
                cv2_mask = cv2.cvtColor(cv2_mask, cv2.COLOR_GRAY2BGR)

                frame_result = cv2.bitwise_or(frame_result, cv2_mask)
                # frame_result = cv2.bitwise_and(frame_result, cv2.bitwise_not(cv2_mask)) + cv2.bitwise_and(frame, cv2_mask)
        
        # マスクフレームを描画
        frame_result = cv2.resize(frame_result, (frame_width, frame_height))
        writer_mask.write(frame_result)
    shutil.rmtree(frame_folder)
    writer_mask.release()

    if new_fps != 0:
        if fps < new_fps:
            print("フレームレート変換中...")
            fps_trans.fps_trans(cache_mask_path, original_mask_path, new_fps)
            fps_trans.fps_trans(cache_video_path, new_original_video_path, new_fps)
        else:
            shutil.copy(cache_mask_path, original_mask_path)
            shutil.copy(cache_video_path, new_original_video_path)
    else:
        shutil.copy(cache_mask_path, original_mask_path)

if __name__ == "__main__":
    main()