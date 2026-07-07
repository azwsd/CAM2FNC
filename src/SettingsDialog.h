#pragma once
#include "CamTypes.h"
#include <wx/wx.h>
#include <wx/spinctrl.h>

class SettingsDialog : public wxDialog {
public:
    explicit SettingsDialog(wxWindow* parent, const FncSettings& current);
    FncSettings GetSettings() const;

private:
    wxChoice*    m_drillType;
    wxCheckBox*  m_removeHoles;
    wxCheckBox*  m_removeSlots;
    wxTextCtrl*  m_constraintMat;
    wxCheckBox*  m_includePrf;
    wxCheckBox*  m_includeBars;
    wxCheckBox*  m_exportByProfile;
    wxCheckBox*  m_combinedOutput;
    wxSpinCtrl*  m_nestStart;

    // Automatic marking controls
    wxCheckBox*  m_enableAutoMark;
    wxChoice*    m_markFace;
    wxTextCtrl*  m_markX;
    wxTextCtrl*  m_markY;
    wxTextCtrl*  m_markAngle;
};
