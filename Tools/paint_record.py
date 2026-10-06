"""Records PaintLab's clips and stills for the MintChoco paint deck.

Every shot runs in a fresh PaintLab held on the command port, one counted
frame at a time, so a clip is the same picture on any machine. Frames go to
BMP, are folded into lossless segments as they come, and end as an mp4.

    python Tools/paint_record.py captures/paintlab/run1
    python Tools/paint_record.py captures/paintlab/run1 --only t1_ t7_splash

Run it from the repository root against a Debug build (the port is Debug
only). Shots are defined in Tools/paint_shots.py.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

import paint_shots

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "build" / "bin" / "PaintLab.exe"
CONVERT = ROOT / "build" / "bin" / "ImageCompareCheck.exe"
FONT = Path("C:/Windows/Fonts/malgun.ttf")
# frames folded into one lossless segment before their BMPs go
BATCH = 60
SIM_FPS = 60


class Port:
    def __init__(self, number):
        self.uri = f"http://127.0.0.1:{number}/rpc"

    def call(self, cmd, args=None, timeout=120):
        body = json.dumps({"cmd": cmd, "args": args or {}}).encode()
        request = urllib.request.Request(
            self.uri, data=body, headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(request, timeout=timeout) as response:
            answer = json.loads(response.read())
        if not answer.get("ok"):
            raise RuntimeError(f"port: {cmd} {args} failed: {answer.get('error')}")
        return answer.get("result")

    def wait(self, seconds=120):
        deadline = time.time() + seconds
        while time.time() < deadline:
            try:
                return self.call("ping", timeout=5)
            except (urllib.error.URLError, ConnectionError, OSError):
                time.sleep(0.25)
        raise RuntimeError("port: PaintLab never answered")

    def drain_captures(self):
        deadline = time.time() + 120
        while True:
            status = self.call("ping")
            if status["capturesPending"] == 0:
                if status["captureFailures"]:
                    raise RuntimeError("port: a capture failed")
                return
            if time.time() > deadline:
                raise RuntimeError("port: captures still pending after 120 s")
            time.sleep(0.05)


def run(cmd, **kw):
    result = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if result.returncode != 0:
        raise RuntimeError(f"{cmd[0]} failed:\n{result.stderr[-2000:]}")
    return result.stdout


def count_frames(path):
    out = run([
        "ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
        "-show_entries", "stream=nb_read_frames", "-of", "csv=p=0", str(path),
    ])
    return int(out.strip())


def bmp_to_png(bmp, png):
    run([str(CONVERT), "--convert", str(bmp), str(png)])


class Recorder:
    def __init__(self, out, port_number):
        self.out = Path(out).resolve()
        self.port_number = port_number
        self.port = Port(port_number)
        self.process = None

    def launch(self, log_stem):
        env = dict(os.environ)
        env["CROWY_COMMAND_PORT"] = str(self.port_number)
        env["CROWY_WINDOW_DISPLAY"] = "1"
        env["CROWY_D3D_DEBUG_BREAK"] = "1"
        env.pop("CROWY_DUMP_FRAME", None)
        logs = self.out / "logs"
        logs.mkdir(parents=True, exist_ok=True)
        self.process = subprocess.Popen(
            [str(EXE), "--hold"],
            cwd=ROOT,
            env=env,
            stdout=open(logs / f"{log_stem}.out.txt", "w"),
            stderr=open(logs / f"{log_stem}.err.txt", "w"),
        )
        self.port.wait()

    def close(self):
        if self.process is None:
            return
        try:
            self.port.call("quit", timeout=10)
            self.process.wait(timeout=30)
        except Exception:
            self.process.kill()
        self.process = None

    def frame(self):
        return self.port.call("ping")["frame"]

    def apply(self, calls):
        for call in calls:
            self.port.call(call["cmd"], call.get("args", {}))

    def advance(self, frames):
        target = self.frame() + frames
        self.port.call("run", {"until": target})
        self.port.call("wait_frame", {"frame": target})

    def prepare(self, shot):
        # surfaces draw from frame 2 and the shaders compile on first use
        self.apply([paint_shots.lab("countFrames", True)])
        self.advance(4)
        self.apply(shot.get("setup", []))
        self.advance(shot.get("settle", 2))

    def record_clip(self, shot):
        name = shot["name"]
        fps = shot.get("fps", 30)
        stride = SIM_FPS // fps
        total = int(round(shot["seconds"] * SIM_FPS))
        work = self.out / "work" / name
        if work.exists():
            shutil.rmtree(work)
        work.mkdir(parents=True)

        self.launch(name)
        try:
            self.prepare(shot)
            events = shot.get("events", {})
            animate = shot.get("animate")
            stills = shot.get("stills", {})
            segments = []
            captured = 0
            batch_start = 0
            for i in range(total):
                self.apply(events.get(i, []))
                if animate:
                    self.apply(animate(i / max(total - 1, 1)) or [])
                upcoming = self.frame() + 1
                if i % stride == 0:
                    path = work / f"{captured:05d}.bmp"
                    self.port.call(
                        "capture_frame", {"path": str(path), "frame": upcoming}
                    )
                    captured += 1
                self.port.call("run", {"until": upcoming})
                self.port.call("wait_frame", {"frame": upcoming})
                if captured - batch_start == BATCH or (
                    i == total - 1 and captured > batch_start
                ):
                    self.port.drain_captures()
                    segments.append(
                        self.fold(work, batch_start, captured, fps, stills, stride)
                    )
                    batch_start = captured
        finally:
            self.close()

        clip = self.finish(shot, work, segments, fps)
        print(f"{name}: {captured} frames at {fps} fps -> {clip}")
        return clip

    def fold(self, work, first, end, fps, stills, stride):
        """the BMPs [first, end) into a lossless segment, keeping stills"""
        stills_dir = self.out / "stills"
        stills_dir.mkdir(parents=True, exist_ok=True)
        for still, sim_frame in stills.items():
            index = sim_frame // stride
            if first <= index < end:
                bmp_to_png(work / f"{index:05d}.bmp", stills_dir / f"{still}.png")
        poster = work / "poster.png"
        bmp_to_png(work / f"{end - 1:05d}.bmp", poster)

        segment = work / f"seg{first:05d}.mkv"
        run([
            "ffmpeg", "-y", "-v", "error", "-framerate", str(fps),
            "-start_number", str(first), "-i", str(work / "%05d.bmp"),
            "-frames:v", str(end - first), "-c:v", "libx264", "-qp", "0",
            "-preset", "ultrafast", "-pix_fmt", "yuv444p", str(segment),
        ])
        if count_frames(segment) != end - first:
            raise RuntimeError(f"{segment} lost frames; its BMPs are kept")
        for index in range(first, end):
            (work / f"{index:05d}.bmp").unlink()
        return segment

    def finish(self, shot, work, segments, fps):
        clips = self.out / "clips"
        clips.mkdir(parents=True, exist_ok=True)
        listing = work / "segments.txt"
        listing.write_text(
            "".join(f"file '{s.name}'\n" for s in segments), encoding="utf-8"
        )

        filters = []
        crop = shot.get("crop")
        if crop:
            filters.append("crop={2}:{3}:{0}:{1}".format(*crop))
        filters += self.captions(shot, work)
        out = clips / f"{shot['name']}.mp4"
        command = [
            "ffmpeg", "-y", "-v", "error", "-f", "concat", "-safe", "0",
            "-i", listing.name,
        ]
        if filters:
            command += ["-vf", ",".join(filters)]
        command += [
            "-c:v", "libx264", "-crf", "18", "-preset", "slow",
            "-pix_fmt", "yuv420p", "-r", str(fps), "-movflags", "+faststart",
            str(out),
        ]
        run(command, cwd=work)

        poster = clips / f"{shot['name']}.png"
        shutil.copyfile(work / "poster.png", poster)
        if crop:
            run([
                "ffmpeg", "-y", "-v", "error", "-i", str(poster), "-vf",
                "crop={2}:{3}:{0}:{1}".format(*crop), str(poster.with_suffix(".tmp.png")),
            ])
            poster.with_suffix(".tmp.png").replace(poster)
        return out

    def captions(self, shot, work):
        """drawtext per caption, the text in a UTF-8 file beside the segments"""
        filters = []
        captions = shot.get("captions", [])
        if not captions:
            return filters
        shutil.copyfile(FONT, work / "caption.ttf")
        total = shot["seconds"]
        for index, (start_frame, text) in enumerate(captions):
            begin = start_frame / SIM_FPS
            end = captions[index + 1][0] / SIM_FPS if index + 1 < len(captions) else total
            (work / f"caption{index}.txt").write_text(text, encoding="utf-8")
            filters.append(
                f"drawtext=fontfile=caption.ttf:textfile=caption{index}.txt"
                f":x=36:y=h-th-36:fontsize={shot.get('caption_size', 40)}"
                ":fontcolor=white:box=1:boxcolor=black@0.5:boxborderw=14"
                f":enable='between(t,{begin:.3f},{end:.3f})'"
            )
        return filters

    def record_stills(self, shot):
        """a still sheet: each entry's calls, then one captured frame"""
        name = shot["name"]
        stills_dir = self.out / "stills" / name
        stills_dir.mkdir(parents=True, exist_ok=True)
        self.launch(name)
        try:
            self.prepare(shot)
            for still, calls in shot["frames"]:
                self.apply(calls)
                upcoming = self.frame() + 1
                bmp = stills_dir / f"{still}.bmp"
                self.port.call("capture_frame", {"path": str(bmp), "frame": upcoming})
                self.port.call("run", {"until": upcoming + 1})
                self.port.call("wait_frame", {"frame": upcoming + 1})
                self.port.drain_captures()
                png = bmp.with_suffix(".png")
                bmp_to_png(bmp, png)
                bmp.unlink()
                crop = shot.get("crop")
                if crop:
                    run([
                        "ffmpeg", "-y", "-v", "error", "-i", str(png), "-vf",
                        "crop={2}:{3}:{0}:{1}".format(*crop), str(png.with_suffix(".c.png")),
                    ])
                    png.with_suffix(".c.png").replace(png)
        finally:
            self.close()
        print(f"{name}: {len(shot['frames'])} stills")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out", help="output directory, e.g. captures/paintlab/run1")
    parser.add_argument("--only", nargs="*", default=[], help="shot name prefixes")
    parser.add_argument("--port", type=int, default=27540)
    parser.add_argument("--list", action="store_true", help="print the shots and exit")
    args = parser.parse_args()

    shots = paint_shots.all_shots()
    if args.list:
        for shot in shots:
            print(shot["name"], shot.get("seconds", "stills"))
        return 0
    chosen = [
        s for s in shots
        if not args.only or any(s["name"].startswith(p) for p in args.only)
    ]
    recorder = Recorder(args.out, args.port)
    for shot in chosen:
        if "frames" in shot:
            recorder.record_stills(shot)
        else:
            recorder.record_clip(shot)
    return 0


if __name__ == "__main__":
    sys.exit(main())
