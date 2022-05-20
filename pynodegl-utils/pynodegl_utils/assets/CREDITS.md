# Assets credits and sources

## `cat.mp4`

- Downloaded from https://pixabay.com/videos/cat-feline-animal-mammal-kitten-65438/
- Author: `Kmeel_Stock`
- Reencoded with: `ffmpeg -i Original-Cat.mp4 -vf scale=640:360 -preset placebo -crf 30 -y cat.mp4`
- License: [Pixabay License](https://pixabay.com/service/license/)

## `mire-h264.mp4`

Generated with `ffmpeg -f lavfi -i testsrc=d=15:r=60:s=hd720 -preset placebo -c:v h264 mire-h264.mp4`

## `mire-hevc.mp4`

Generated with `ffmpeg -f lavfi -i testsrc2=d=30:r=30 -preset placebo -c:v hevc mire-hevc.mp4`

## `Unsplash-Michael_Anfang-Rooster-cropped.jpg`

Cropped from original picture by Michael Anfang:
- https://unsplash.com/photos/4v_HuMArJDY
- https://unsplash.com/@manfang

License: [Unsplash](https://unsplash.com/license)

## `Unsplash-Romane_Gautun-Red_Panda-cropped.jpg`

Cropped from original picture by Romane Gautun:
- https://unsplash.com/photos/I30tUSDgTZc
- https://unsplash.com/@oramen

License: [Unsplash](https://unsplash.com/license)

## `OpenMoji-1F342-Fallen_Leaf.png`

OpenMoji project: https://openmoji.org/

License: CC BY-SA 4.0

## `OpenMoji-1F439-Hamster.png`

OpenMoji project: https://openmoji.org/

License: CC BY-SA 4.0
