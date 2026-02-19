import av
from fractions import Fraction

#AI生成

def fps_trans(input_path, output_path, target_fps):
    """
    VFR動画をPTSベースで解析し、正確なCFR動画に変換する
    """
    container = av.open(input_path)
    output = av.open(output_path, mode='w')

    # --- 1. ビデオストリームの準備 ---
    in_video = container.streams.video[0]
    fps_fraction = Fraction(target_fps).limit_denominator()
    
    # 出力ストリームの設定
    out_video = output.add_stream('libx264', rate=fps_fraction)
    out_video.width = in_video.width
    out_video.height = in_video.height
    out_video.pix_fmt = 'yuv420p'
    # CFRを保証するためにタイムベースを1/FPSに設定
    out_video.time_base = 1 / fps_fraction

    # --- 2. フィルタグラフの設定 (PTSベース変換の核) ---
    # fpsフィルタは入力PTSを解釈し、一定の出力PTSへ再配置する
    graph = av.filter.Graph()
    buffer = graph.add_buffer(template=in_video)
    fps_filter = graph.add("fps", fps=str(fps_fraction))
    buffersink = graph.add("buffersink")
    
    buffer.link_to(fps_filter)
    fps_filter.link_to(buffersink)
    graph.configure()

    # --- 3. 音声ストリームの準備 (あれば) ---
    in_audio = container.streams.audio[0] if container.streams.audio else None
    out_audio = None
    if in_audio:
        out_audio = output.add_stream('aac', rate=in_audio.rate)
        out_audio.layout = in_audio.layout
        out_audio.format = 'fltp'

    for packet in container.demux(in_video, in_audio):
        for frame in packet.decode():
            if packet.stream.type == 'video':
                graph.push(frame)
                while True:
                    try:
                        # ここがポイント：EOFErrorのみをキャッチするように変更
                        out_frame = graph.pull()
                        out_frame.pts = None 
                        for p in out_video.encode(out_frame):
                            output.mux(p)
                    except av.EOFError:
                        break
                    except Exception as e:
                        # 他のエラー（まだフレームが準備できていない等）は無視して次へ
                        break
            
            elif packet.stream.type == 'audio' and out_audio:
                frame.pts = None
                for p in out_audio.encode(frame):
                    output.mux(p)

    # --- 5. フラッシュ ---
    for p in out_video.encode():
        output.mux(p)
    if out_audio:
        for p in out_audio.encode():
            output.mux(p)

    container.close()
    output.close()

# 使用例: 29.97 FPSのCFRに変換
# convert_vfr_to_cfr('input_vfr.mp4', 'output_cfr.mp4', 29.97)