# ASTC fallback decoder

Vendored from Perfare/AssetStudio, commit
`d158e864b556b5970709c2a52e47944d53aa98a2`, `Texture2DDecoderNative`.
The MIT license is in LICENSE; the bundled FP16 headers carry their own MIT
notices. Upstream files are unmodified.

Only used to decode ClientMod 2.0 ASTC 4x4/6x6 when GLES does not report the
specific compressed format, and for Source's explicit image conversions.
Native uploads retain the original compressed bytes. decodeBGRA validates
dimensions and byte lengths and pads the final compressed block for word reads.
