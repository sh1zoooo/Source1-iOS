# Agent and animation audit of the supplied ClientMod archive

Source: https://disk.yandex.ru/d/bBscvB6EWB1xog, Clientmod Rec1.4.rar.
SHA256: 75b56b7eaf6b495b1234b3de2aaf7a3cf5eee3b590223fcfac88627a31aac973.
Re-downloaded and verified on 2026-10-08. Complete RAR index: 893 regular files, 7,093,429,433 expanded bytes. Nine VPK directories, 74,646 VPK entries inspected. No loose MDL/ANI files exist outside the VPKs.

## Actual player resources

The cstrike/madstray_lox/clientmod VPK contains these eight player bodies under standard Source paths:

| File under models/player | MDL version | Bones |
| --- | --- | --- |
| t_phoenix.mdl | 44 | 50 |
| t_leet.mdl | 44 | 50 |
| t_guerilla.mdl | 44 | 50 |
| t_arctic.mdl | 44 | 50 |
| ct_urban.mdl | 44 | 50 |
| ct_sas.mdl | 44 | 50 |
| ct_gsg9.mdl | 44 | 49 |
| ct_gign.mdl | 44 | 50 |

Each body has one local animation/sequence, referencing shared animation data. cs_player_shared.mdl has 49 bones, 1,382 local animations, and 723 local sequences. ak_anims_t.mdl has 67 animations/32 sequences; ak_anims_ct.mdl has 68/33. Extracted MDL payloads were CRC32-checked against their VPK directory entries.

Additional models/characters assets include four hostages and counterterrorist.mdl (MDL44, 49 bones); player_animations.mdl there has 56 bones and two sequences. The full inventory contains no separate CS:GO-style custom_player, ctm_*, or tm_* agent model files. This is an inventory finding, not proof of an asset's appearance from its filename alone. No distinct CS:GO agent package was identified in this supplied archive.

## What the port currently uses

SourcePlayerSDK.cpp calls original CCSPlayer HandleCommand_JoinClass(0) and RoundRespawn. CCSPlayer::Spawn calls SetModelFromClass, which chooses the eight standard player paths. The visual bridge loads the resulting path through the mounted user cache and uses original server SetupBones. These bodies are imported resources, not replacement characters bundled into the IPA. Using Source engine code does not by itself provide CS:GO meshes, textures or animation graphs.

The Skinpack VPK contains 31 weapon viewmodels, but no player-body MDLs. It overlaps the cm_resources VPK on 30 paths. The supplied cm/gameinfo.txt gives custom, extras, cm_resources, and cstrike/madstray_lox resource priority. The existing recursive alphabetical mount policy does not fully implement this order; this is a separate ClientMod resource-compatibility item to address with tests.

## Required input for the requested agents

To reproduce the default agents and third-person animations seen in a different ClientMod installation, supply that installation's actual agent/animation/material resources or its complete game cache. Do not silently label the current standard-path models as CS:GO agents or manufacture a matching claim from this archive.

## Verified gameplay

On the restored complete cache, code commit 76e86269314e1c411830c4a29b8e65c26da8b950 passed both manual gameplay cycles at 60/120 display updates, including the newly added nonzero server-punch assertion, movement, collision, crouch bone changes, jump/landing, firing, reserve-to-magazine reload, visible 1P/3P meshes, pause input reset, and restart. Both used 50 player bones and 57 AWP viewmodel bones. This does not validate exact ClientMod animation behavior or physical-iPhone appearance.
