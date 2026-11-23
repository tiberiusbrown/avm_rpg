"""Record deterministic combat fixtures from the running AVM RPG with emulator audio."""

import argparse
from array import array
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
import sys

from elf import symbols


GROUPS = ("movement", "normal", "starter", "signature", "perks")
WIDTH, HEIGHT, FPS = 128, 64, 50


def record(build, raw, group):
    artifacts = build / "artifacts"
    refs = symbols(artifacts / "rpg_qa.elf")
    keys = (
        "game", "frame_marker", "frame_serial", "dialogue_prompt", "world_phase",
        "battle_qa_state", "battle_qa_ready", "dialogue_panel_open", "runtime_workspace",
    )
    addresses = [str(refs[key]["address"]) for key in keys]
    executable = build / ("rpg_device_tests.exe" if sys.platform == "win32" else "rpg_device_tests")
    directory = raw / group
    directory.mkdir(exist_ok=True)
    with (raw / f"{group}.log").open("w", encoding="utf-8") as log:
        subprocess.run(
            [executable, artifacts / "rpg_qa.arduboy", *addresses[:3], directory,
             "battle-recordings", *addresses[3:]],
            env={**os.environ, "RPG_BATTLE_REVIEW_GROUP": group},
            stdout=log, stderr=subprocess.STDOUT, check=True,
        )


def encode(ffmpeg, description, clips):
    title, frames, rate = description.read_text().splitlines()
    frames, rate = int(frames), int(rate)
    video = description.with_suffix(".gray")
    pcm = description.with_suffix(".s16")
    if video.stat().st_size != WIDTH * HEIGHT * frames:
        raise ValueError(f"{video}: incomplete frame data")
    samples = array("h", pcm.read_bytes())
    if len(samples) != frames * rate // FPS or max(samples) - min(samples) < 64:
        raise ValueError(f"{pcm}: missing audible samples")

    headline, detail = title.split(" | ", 1)
    # Text files avoid escaping labels into FFmpeg's filter syntax. Paths are relative to cwd.
    stem = description.stem
    description.with_suffix(".title").write_text(headline)
    description.with_suffix(".detail").write_text(detail)
    video_filter = (
        "scale=768:384:flags=neighbor,pad=768:480:0:76:color=0x171a1e,"
        f"drawtext=textfile={stem}.title:fontcolor=white:fontsize=26:x=18:y=9,"
        f"drawtext=textfile={stem}.detail:fontcolor=0xd6dde4:fontsize=18:x=18:y=44"
    )
    clip = clips / f"{stem}.mp4"
    command = [
        ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
        "-filter_threads", "1",
        "-f", "rawvideo", "-pixel_format", "gray", "-video_size", f"{WIDTH}x{HEIGHT}",
        "-framerate", str(FPS), "-i", video,
        "-f", "s16le", "-ar", str(rate), "-ac", "1", "-i", pcm,
        "-vf", video_filter, "-af", "highpass=f=35,volume=7,aresample=48000",
        "-c:v", "libx264", "-preset", "veryfast", "-threads", "2", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "96k",
        "-movflags", "+faststart", clip,
    ]
    subprocess.run(command, cwd=description.parent, check=True)
    return title, frames, clip


def chapter_text(title):
    for char in ("\\", "=", ";", "#"):
        title = title.replace(char, "\\" + char)
    return title


def reel(ffmpeg, group, raw, clips, output):
    descriptions = sorted((raw / group).glob(f"{group}-*.txt"),
                          key=lambda item: int(item.stem.rsplit("-", 1)[1]))
    if not descriptions:
        raise ValueError(f"no recordings for {group}")
    sections = [encode(ffmpeg, description, clips) for description in descriptions]
    metadata = [";FFMETADATA1"]
    chapters = []
    files = []
    start = 0
    for title, frames, clip in sections:
        end = start + frames * 1000 // FPS
        metadata.extend(["[CHAPTER]", "TIMEBASE=1/1000", f"START={start}", f"END={end}",
                         f"title={chapter_text(title)}"])
        files.append(f"file clips/{clip.name}")
        time = f"{start // 60000}:{start // 1000 % 60:02d}"
        chapters.append(f"| {time} | {title.replace(' | ', ' — ')} | [MP4](clips/{clip.name}) |")
        start = end
    concat = output / f"{group}.concat"
    concat.write_text("\n".join(files) + "\n")
    chapters_file = output / f"{group}.ffmetadata"
    chapters_file.write_text("\n".join(metadata) + "\n")
    subprocess.run(
        [ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
         "-f", "concat", "-safe", "1", "-i", concat, "-i", chapters_file,
         "-map", "0:v", "-map", "0:a", "-map_metadata", "1", "-map_chapters", "1",
         "-c", "copy", "-movflags", "+faststart", output / f"{group}.mp4"],
        check=True,
    )
    concat.unlink()
    chapters_file.unlink()
    print(f"{group}: {len(sections)} clips", flush=True)
    return chapters


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--group", choices=GROUPS, action="append")
    parser.add_argument("--encode-only", action="store_true",
                        help="reuse the raw recordings in the output directory")
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    raw, clips = output / "raw", output / "clips"
    raw.mkdir(parents=True, exist_ok=True)
    clips.mkdir(exist_ok=True)
    groups = args.group or GROUPS
    with ThreadPoolExecutor(max_workers=len(groups)) as executor:
        if not args.encode_only:
            list(executor.map(lambda group: record(build, raw, group), groups))
        chapters = list(executor.map(lambda group: reel(args.ffmpeg, group, raw, clips, output),
                                     groups))
    index = ["# Battle animation and sound review", "",
             "The clips use deterministic review boards. Music is muted so the game’s sound "
             "effects are easier to hear. Videos are captured from the emulator at 50 fps.", ""]
    for group, entries in zip(groups, chapters):
        index.extend([f"## [{group.title()}]({group}.mp4)", "", "| Time in reel | Situation | Clip |",
                      "| --- | --- | --- |", *entries, ""])
    (output / "index.md").write_text("\n".join(index), encoding="utf-8")


if __name__ == "__main__":
    main()
