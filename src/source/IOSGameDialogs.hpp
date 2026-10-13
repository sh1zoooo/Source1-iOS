#pragma once
#ifdef SOURCE_IOS
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
inline void CreateIOSTeamButtons(vgui::Panel *panel) {
    panel->SetScheme(vgui::scheme()->GetScheme("Default"));
    panel->SetProportional(false);
    new vgui::Label(panel, "ios_title", "Choose a team");
    const char *names[] = {"ios_t", "ios_ct", "ios_auto", "ios_spec"};
    const char *labels[] = {"Terrorists", "Counter-Terrorists", "Auto assign", "Spectate"};
    const char *commands[] = {"jointeam 2", "jointeam 3", "jointeam 0", "jointeam 1"};
    for (int i = 0; i < 4; ++i) {
        auto *button = new vgui::Button(panel, names[i], labels[i], panel, commands[i]);
        button->SetButtonActivationType(vgui::Button::ACTIVATE_ONPRESSED);
    }
}
inline void LayoutIOSTeamButtons(vgui::Panel *panel) {
    int width, height;
    vgui::surface()->GetScreenSize(width, height);
    panel->SetBounds((width - 560) / 2, (height - 440) / 2, 560, 440);
    vgui::IScheme *scheme = vgui::scheme()->GetIScheme(panel->GetScheme());
    int row = 0;
    for (int i = 0; i < panel->GetChildCount(); ++i) {
        vgui::Panel *child = panel->GetChild(i);
        bool mobile = !Q_strncmp(child->GetName(), "ios_", 4);
        child->SetVisible(mobile);
        if (!mobile) continue;
        if (auto *button = dynamic_cast<vgui::Button *>(child)) {
            button->SetBounds(24, 72 + row++ * 84, 512, 64);
            button->SetFont(scheme->GetFont("Default", false));
            button->SetDefaultColor(Color(255,255,255,255), Color(48,56,68,255));
            button->SetArmedColor(Color(255,255,255,255), Color(68,90,118,255));
            button->SetDepressedColor(Color(255,255,255,255), Color(30,54,82,255));
            button->SetEnabled(true);
        } else if (auto *label = dynamic_cast<vgui::Label *>(child)) {
            label->SetBounds(24, 16, 512, 44);
            label->SetFont(scheme->GetFont("Default", false));
            label->SetFgColor(Color(255,255,255,255));
        }
    }
}
#endif
