# 0.14.0 / build 17 — embedded MDL animation tracks

The fixture now contains an actual mstudioanimdesc/mstudioanim track and Source
RLE rotation samples. Playback no longer uses a sine-wave pose generated in code.
The bounded reader predecodes supported local tracks into a limited pose cache;
Source mathlib converts Euler samples and blends quaternion frames. This parser
implements Source stream semantics; it does not yet call the full bone_setup
sequence evaluator.

Supported: embedded local tracks, raw Quaternion48/Quaternion64/Vector48,
RLE rotation and translation, inherited bind components, frame interpolation,
loop/clamp, explicit clip selection and pause/resume. Limits: 128 descriptions,
2048 frames per clip, 262144 bone poses total. Frame rate, names, track links,
bone IDs and RLE run bounds are validated. Broken supported tracks reject loading.

External ANI blocks, sectioned/frame animations, delta clips, local hierarchy,
IK and blend sequences are skipped. Their geometry can still load in bind pose;
skipping is not a claim that they play. Root motion is not extracted.

Local Debug runtime passed. Checks cover interpolation at a known half-frame,
loop identity, malformed zero-length RLE rejection, clip selection, pause/resume,
finite skinned geometry and restart. 65 startup checks expected; Actions pending.
