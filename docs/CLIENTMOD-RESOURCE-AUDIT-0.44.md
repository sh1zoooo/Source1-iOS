# ClientMod 1.4 resource audit (2026-10-08)

Re-read the user-supplied RAR from its public Yandex link. The archive contains 893 regular files. VPK directory inventory now includes the previously missed self-contained `cm/extras/!hud.vpk`: 75,378 entries across 10 archives (counts include duplicate paths).

## Packages

| Package | Entries |
| --- | ---: |
| `cm/clientmod_base/cm_resources_dir.vpk` | 30031 |
| `cm/custom/Skinpack_dir.vpk` | 1074 |
| `cm/extras/!hud.vpk` | 732 |
| `cm/extras/extras_dir.vpk` | 59 |
| `cstrike/madstray_lox/clientmod_dir.vpk` | 18563 |
| `hl2/hl2_misc_dir.vpk` | 16324 |
| `hl2/hl2_pak_dir.vpk` | 211 |
| `hl2/hl2_sound_misc_dir.vpk` | 2970 |
| `hl2/hl2_textures_dir.vpk` | 5021 |
| `platform/platform_misc_dir.vpk` | 393 |

## ClientMod maps

- `aim_map_csgo`
- `aimbotz`
- `awp_lego_2`
- `de_ancient_cmgofinal`
- `de_anubis_cs2mix`
- `de_cache_csgo`
- `de_dust2_csgo_new_v2`
- `de_inferno_cs2mix`
- `de_mirage_go`
- `de_nuke_cs2mix`
- `de_overpass_go`
- `de_season`
- `de_train_cs2mix`
- `de_vertigo_csgo_v34_fix_cs2remastered`

## HUD package

`!hud.vpk` contains 732 entries: 277 VTF textures, 270 VMT materials, 145 RES panels, 24 TTF fonts, 11 TXT scripts/localizations, 2 RESB files, one shader, one cache and one old file. It includes health/armor/ammo/money layouts, scoreboard, team counter, round-win panel, spectator and freeze panels, loadout, buy menus, individual weapon/equipment panels and Stratum/Noto Sans/icon fonts. These are resources; the C++ HUD/VGUI behavior is not supplied as source.

Exact non-texture HUD package paths:

- `sound/sound.cache`
- `shaders/fxc/fade_blur_ps20b.vcs`
- `resource/ui/classmenu_ter.resb`
- `resource/ui/classmenu_ct.resb`
- `scripts/weapon_molotov.txt`
- `scripts/weapon_incgrenade.txt`
- `scripts/mod_textures.txt`
- `scripts/hudanimations_cs.txt`
- `scripts/hudanimations.txt`
- `resource/rec_scoreboard_spectators_russian.txt`
- `resource/rec_scoreboard_spectators_english.txt`
- `resource/rec_scoreboard_russian.txt`
- `resource/rec_scoreboard_english.txt`
- `resource/rec_molotov_russian.txt`
- `resource/rec_molotov_english.txt`
- `resource/new_fonts/stratumno2black.ttf`
- `resource/new_fonts/stratumno1medium.ttf`
- `resource/new_fonts/stratum2monodigit-bold.ttf`
- `resource/new_fonts/stratum2hudnumbers-bold.ttf`
- `resource/new_fonts/stratum2dollar.ttf`
- `resource/new_fonts/stratum2bold_monodigit.ttf`
- `resource/new_fonts/stratum2-medium.ttf`
- `resource/new_fonts/stratum2-bold.ttf`
- `resource/new_fonts/starticons.ttf`
- `resource/new_fonts/null-text.ttf`
- `resource/new_fonts/notosans-regular.ttf`
- `resource/new_fonts/notosans-italic.ttf`
- `resource/new_fonts/notosans-bolditalic.ttf`
- `resource/new_fonts/notosans-bold.ttf`
- `resource/new_fonts/newmenu.ttf`
- `resource/new_fonts/mainmenu.ttf`
- `resource/new_fonts/lucon.ttf`
- `resource/new_fonts/equipments.ttf`
- `resource/new_fonts/csd_icons.ttf`
- `resource/cstrike_icons.ttf`
- `resource/cstrike.ttf`
- `resource/csd_icons.ttf`
- `resource/csd2.ttf`
- `resource/csd.ttf`
- `resource/csd.old`
- `scripts/hudlayoutgy.res`
- `scripts/hudlayout.res`
- `resource/ui/win_round.res`
- `resource/ui/textwindow.res`
- `resource/ui/teammenu.res`
- `resource/ui/spectator.res`
- `resource/ui/scoreboard.res`
- `resource/ui/mainbuymenu.res`
- `resource/ui/loadout.res`
- `resource/ui/freezepanel_basic.res`
- `resource/ui/freezepanelcallout.res`
- `resource/ui/csmatchstatsdialog.res`
- `resource/ui/classmenu_ter.res`
- `resource/ui/classmenu_ct.res`
- `resource/ui/classmenu.res`
- `resource/ui/buysubmachineguns_ter.res`
- `resource/ui/buysubmachineguns_ct.res`
- `resource/ui/buyrifles_ter.res`
- `resource/ui/buyrifles_ct.res`
- `resource/ui/buypistols_ter.res`
- `resource/ui/buypistols_ct.res`
- `resource/ui/buymenu_ter.res`
- `resource/ui/buymenu_ct.res`
- `resource/ui/buymenu.res`
- `resource/ui/buyheavy_ter.res`
- `resource/ui/buyheavy_t.res`
- `resource/ui/buyheavy_ct.res`
- `resource/ui/buygrenades_ter.res`
- `resource/ui/buygrenades_ct.res`
- `resource/ui/buyequipment_ter.res`
- `resource/ui/buyequipment_ct.res`
- `resource/ui/blackmarket_bargains.res`
- `resource/ui/base_x.res`
- `resource/ui/base_selection.res`
- `resource/ui/base_num_6.res`
- `resource/ui/base_num_5.res`
- `resource/ui/base_num_4.res`
- `resource/ui/base_num_3.res`
- `resource/ui/base_main.res`
- `resource/ui/base_clear.res`
- `resource/ui/base_cbr.res`
- `resource/ui/base_button_main.res`
- `resource/ui/base_button.res`
- `resource/hud/teamcounter.res`
- `resource/sourcescheme.res`
- `resource/optionssubvoice.res`
- `resource/modevents.res`
- `resource/hudcolor.res`
- `resource/gamemenu.res`
- `resource/clientmodmenuavatar.res`
- `resource/c4panel.res`
- `classes/x_panel.res`
- `classes/xm1014_ct.res`
- `classes/xm1014.res`
- `classes/weapons_panel.res`
- `classes/ump45_ct.res`
- `classes/ump45.res`
- `classes/tec9_cz75_slct.res`
- `classes/tec9_cz75.res`
- `classes/submachineguns_ct.res`
- `classes/submachineguns.res`
- `classes/ssg08_ct.res`
- `classes/ssg08.res`
- `classes/smokegrenade_ct.res`
- `classes/smokegrenade.res`
- `classes/sg556.res`
- `classes/scar20.res`
- `classes/rifles_ct.res`
- `classes/rifles.res`
- `classes/player_image_x_ct.res`
- `classes/player_image_x.res`
- `classes/player_image_ct.res`
- `classes/player_image.res`
- `classes/pistols_ct.res`
- `classes/pistols.res`
- `classes/p90_ct.res`
- `classes/p90.res`
- `classes/p250_ct.res`
- `classes/p250.res`
- `classes/nova_ct.res`
- `classes/nova.res`
- `classes/not_available.res`
- `classes/nightvision.res`
- `classes/mp9.res`
- `classes/mp7_mp5sd_t_slct.res`
- `classes/mp7_mp5sd_t.res`
- `classes/mp7_mp5sd_ct_slct.res`
- `classes/mp7_mp5sd_ct.res`
- `classes/molotov.res`
- `classes/mac10.res`
- `classes/m4a4_m4a1_slct.res`
- `classes/m4a4_m4a1.res`
- `classes/m249_ct.res`
- `classes/m249.res`
- `classes/kevlar_helmet_ct.res`
- `classes/kevlar_helmet.res`
- `classes/kevlar_ct.res`
- `classes/kevlar.res`
- `classes/incgrenade.res`
- `classes/icon_tr.res`
- `classes/icon_ct.res`
- `classes/hkp2000_usp_slct.res`
- `classes/hkp2000_usp.res`
- `classes/hegrenade_ct.res`
- `classes/hegrenade.res`
- `classes/heavy_ct.res`
- `classes/heavy.res`
- `classes/grenades_ct.res`
- `classes/grenades.res`
- `classes/glock18.res`
- `classes/galilar.res`
- `classes/g3sg1.res`
- `classes/flashbang_ct.res`
- `classes/flashbang.res`
- `classes/fiveseven_cz75_slct.res`
- `classes/fiveseven_cz75.res`
- `classes/famas.res`
- `classes/equipment_ct.res`
- `classes/equipment.res`
- `classes/elites_ct.res`
- `classes/elites.res`
- `classes/defuser_slct.res`
- `classes/defuser.res`
- `classes/deagle_revolver_t_slct.res`
- `classes/deagle_revolver_t.res`
- `classes/deagle_revolver_ct_slct.res`
- `classes/deagle_revolver_ct.res`
- `classes/base_x_panel_tr.res`
- `classes/base_x_panel_ct.res`
- `classes/base_weapon_small.res`
- `classes/base_weapon_ct.res`
- `classes/base_weapons_panel_tr.res`
- `classes/base_weapons_panel_ct.res`
- `classes/base_weapon.res`
- `classes/base_status.res`
- `classes/base_loadout.res`
- `classes/base_grenade_ct.res`
- `classes/base_grenade.res`
- `classes/base_equipment_image_ct.res`
- `classes/base_equipment_image.res`
- `classes/base_equipment.res`
- `classes/awp_ct.res`
- `classes/awp.res`
- `classes/aug.res`
- `classes/ak47.res`

## Other ClientMod content

- `custom/hudcolor`: menu, HUD color and multiplayer panel overrides.
- `custom/Skinpack`: 1,074 entries: 31 MDL viewmodels, 31 VVD, 93 VTX, 31 ANI, 91 VTF, 82 VMT, 652 WAV, 33 TXT, 29 CTX and one cache.
- `clientmod_base/cm_resources`: 30,031 entries covering models, materials, animations, UI and sounds.
- `extras/extras_dir`: 59 entries, including touch buttons and custom chat. This differs from the APK extras with 117 entries.
- Loose `resource/clientmodmainmenu.res`, `clientmodmenucrosshair.res`, `clientmodmenugui.res`, `gamemenu.res`, localization and radar overviews.
- `cfg/touch.cfg`, `cfg/highfps.cfg`, `videoconfig_android.cfg` and `glshaders.cfg`.

For player body and shared animation details see CLIENTMOD-AGENTS-AUDIT.md. No separate CS:GO custom-agent package was identified in that earlier complete model inventory.

For the previous APK inventory see ANDROID-CLIENTMOD-1.4.md. The APK attachment is not present in this restored workspace, so it has not been freshly re-inspected in this pass. Android client/GameUI/engine/material/shader libraries and compiled permutations exist according to that recorded inspection; they have not been ported to iOS.

## Changes and limits

Native build and 8/8 CTest checks pass, including new alpha-mask/Patch and custom loose mount regression checks. The full-cache Mirage probe passes movement (339 units), shutdown and a second map load. iPhone visual/performance acceptance is pending.

The mount implementation now includes bounded self-contained VPKs and custom loose directories, with custom then extras then clientmod_base priority from the supplied gameinfo.txt. Opaque VMTs no longer treat texture alpha/specular masks as transparency. Explicit alphatest/translucent flags retain coverage. Tool trigger/clip/invisible surfaces are excluded from the visible BSP while remaining in engine collision. Live render transforms consult the original VPhysics object.

These changes do not complete the graphical ClientMod client. The custom UIKit HUD, incomplete shader families, missing PVS/LOD rendering and device performance still need work. Dropped-body collision and physical iPhone appearance remain unverified; do not interpret parser tests as visual acceptance.
