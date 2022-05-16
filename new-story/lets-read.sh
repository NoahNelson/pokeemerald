#! /bin/bash
# Usage: lets-read.sh <youtube url> <output file without extension> <optional scene change threshold>

if [ ! -e ${2}.mp4 ]; then
    youtube-dl $1 -o $2
    ffmpeg -i ${2}.mkv -codec copy ${2}.mp4
    rm ${2}.mkv
fi

threshold=${3:-1}

ffmpeg -i ${2}.mp4 -vf "fps=${threshold}" -vsync vfr ${2}-frame-%6d.jpg

convert ${2}-frame-*.jpg ${2}.pdf

# rm ${2}-frame-*.jpg