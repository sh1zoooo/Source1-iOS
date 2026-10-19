# ClientMod 2.0 presentation compatibility (0.46.26)

The client/game DLL remains the pinned public ClientMod implementation, with
explicit adaptations for the supplied 2.0 content. This is not a replacement
ClientMod 2.0 DLL or an implementation of its inventory/glove loadout UI.

## Evidence from cm2.0_release.zip

* `cm/cfg/touch.cfg` still references `vgui/touch/*`, but none of the four VPK
  directories contain these assets. Missing VTF **and** VMT paths receive
  resource-free circular action labels. Existing VMT/VTF assets keep their
  normal loader. Generated images join the existing atlas and draw batch.
* `Resource/ClientModMainMenu.res`, which the old settings code loads, is absent.
  The supported old controls now have an iOS layout independent of that file,
  sized from the actual screen. Apply/Cancel/OK use the original property-dialog
  handlers and cvars. Page-local duplicate buttons are hidden.
* `models/weapons/v_rif_ak47.mdl` has one `ak47` texture and one weapon mesh,
  with no arm mesh. The separate
  `models/weapons/arms/custom/gloves/v_bare_hands.mdl` has 49 bones, all present
  under the same names in the 58-bone weapon, and nonempty glove/arm bodyparts.
  The client now merges those bones and draws the arm model in the owning
  viewmodel pass. Weapons with existing arm/hand/glove/sleeve materials are
  excluded. `ios_viewmodel_hands 0` disables this adaptation. A CHandle owns
  lifetime across map/weapon changes; no independent render-list or world pass.

## Startup

A non-key, non-interactive UIKit window displays a spinner and actual startup
stages before loading engine libraries. Independently loaded modules find the
app's exported callback via `dlsym(RTLD_MAIN_ONLY, ...)`. The overlay disappears
only after the visible menu is initialized and a subsequent framebuffer blit
and present report no GL errors. Steam platform-module readiness is deliberately
not required by this standalone build. No fabricated percentage or fixed
startup timeout is used; no UIKit event loop is nested inside Host_Init.

## Validation and limits

* Runtime and ASTC tests remain unchanged and must pass.
* ASan/UBSan tests cover startup event ordering, repeated signals, eight
  landscape/portrait phone/tablet geometries, 21 generated action assets,
  output bounds and real stb atlas packing across multiple button counts.
* The original game/VGUI translation units are syntax checked with the iOS
  paths enabled, then compiled and linked in the device ARM64 CI build.
* Bundle checks require the exported startup bridge and all original modules.

Actual appearance, animation alignment and touch interaction require validation
on the user's device with the supplied content. This change does not claim a
measured FPS improvement, add a low-resolution render mode, or change lighting.
