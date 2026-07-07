#include "App.h"

#include "ActivationDialog.h"
#include "ActivationManager.h"
#include "MainFrame.h"

wxIMPLEMENT_APP(App);

bool App::OnInit() {
    if (!ActivationManager::isActivated()) {
        ActivationDialog dlg(nullptr);
        if (dlg.ShowModal() != wxID_OK || !dlg.IsActivated())
            return false;
        if (!ActivationManager::verifyRuntime())
            return false;
    } else if (!ActivationManager::verifyRuntime()) {
        return false;
    }

    auto* frame = new MainFrame();
    frame->Show(true);
    return true;
}
