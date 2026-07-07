#include "SettingsDialog.h"
#include <wx/statline.h>

SettingsDialog::SettingsDialog(wxWindow* parent, const FncSettings& cur)
    : wxDialog(parent, wxID_ANY, "Export Settings",
               wxDefaultPosition, wxSize(420, -1),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto makeBox = [&](const wxString& label) -> wxStaticBoxSizer* {
        return new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, label), wxVERTICAL);
    };
    auto addRow = [&](wxSizer* sizer, const wxString& lbl, wxWindow* ctrl) {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, lbl), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 8);
        row->Add(ctrl, 1, wxEXPAND);
        sizer->Add(row, 0, wxEXPAND|wxALL, 4);
    };

    // Hole and drill settings.
    auto* drillBox = makeBox("Holes / Drilling");
    wxArrayString drillChoices;
    drillChoices.Add("Drill"); drillChoices.Add("Punch"); drillChoices.Add("Tap");
    m_drillType   = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, drillChoices);
    m_drillType->SetStringSelection(cur.drillType);
    addRow(drillBox, "Drill type:", m_drillType);

    m_removeHoles = new wxCheckBox(this, wxID_ANY, "Remove all holes from FNC output");
    m_removeHoles->SetValue(cur.removeHoles);
    drillBox->Add(m_removeHoles, 0, wxALL, 4);

    m_removeSlots = new wxCheckBox(this, wxID_ANY, "Skip slotted holes (type 4)");
    m_removeSlots->SetValue(cur.removeSlots);
    drillBox->Add(m_removeSlots, 0, wxALL, 4);
    root->Add(drillBox, 0, wxEXPAND|wxALL, 6);

    // Force all pieces to use the same material.
    auto* matBox = makeBox("Material Override");
    m_constraintMat = new wxTextCtrl(this, wxID_ANY, cur.constraintMaterial);
    m_constraintMat->SetHint("Leave empty to use piece material");
    addRow(matBox, "Force material:", m_constraintMat);
    root->Add(matBox, 0, wxEXPAND|wxALL, 6);

    // Choose what sections appear in the FNC output.
    auto* outBox = makeBox("Output Structure");
    m_includePrf = new wxCheckBox(this, wxID_ANY,
        "Include [[PRF]] / [[MAT]] header blocks");
    m_includePrf->SetValue(cur.includePrfHeader);
    outBox->Add(m_includePrf, 0, wxALL, 4);

    m_includeBars = new wxCheckBox(this, wxID_ANY,
        "Include [[BAR]] nesting plan at end of file");
    m_includeBars->SetValue(cur.includeBarNests);
    outBox->Add(m_includeBars, 0, wxALL, 4);

    m_exportByProfile = new wxCheckBox(this, wxID_ANY,
        "Export each unique profile to a separate file");
    m_exportByProfile->SetValue(cur.exportByProfile);
    outBox->Add(m_exportByProfile, 0, wxALL, 4);

    m_combinedOutput = new wxCheckBox(this, wxID_ANY,
        "Combine all input files into one FNC output");
    m_combinedOutput->SetValue(cur.combinedOutput);
    outBox->Add(m_combinedOutput, 0, wxALL, 4);

    // Start number for nesting plan IDs.
    {
        m_nestStart = new wxSpinCtrl(this, wxID_ANY,
            wxString::Format("%d", cur.nestCounterStart),
            wxDefaultPosition, wxSize(120, -1), wxSP_ARROW_KEYS, 1, 999999999,
            cur.nestCounterStart);

        auto* autoBtn = new wxButton(this, wxID_ANY, "Auto",
                                     wxDefaultPosition, wxSize(50, -1));
        autoBtn->SetToolTip("Reset to date-based nest number");
        autoBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            m_nestStart->SetValue(GetDefaultNestNumber());
        });

        auto* nestRow = new wxBoxSizer(wxHORIZONTAL);
        nestRow->Add(new wxStaticText(this, wxID_ANY, "First nest number:"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        nestRow->Add(m_nestStart, 1, wxEXPAND);
        nestRow->Add(autoBtn,    0, wxLEFT, 4);
        outBox->Add(nestRow, 0, wxEXPAND | wxALL, 4);
    }

    root->Add(outBox, 0, wxEXPAND|wxALL, 6);

    // Settings for automatic marking on each piece.
    auto* markBox = makeBox("Automatic Marking");
    m_enableAutoMark = new wxCheckBox(this, wxID_ANY, "Enable automatic marking on each piece");
    m_enableAutoMark->SetValue(cur.enableAutoMark);
    markBox->Add(m_enableAutoMark, 0, wxALL, 4);

    // Face dropdown
    wxArrayString faceChoices;
    faceChoices.Add("DA"); faceChoices.Add("DB"); faceChoices.Add("DC"); faceChoices.Add("DD");
    m_markFace = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, faceChoices);
    if (!cur.markFace.empty()) {
        m_markFace->SetStringSelection(cur.markFace);
    } else {
        m_markFace->SetSelection(0);
    }
    addRow(markBox, "Face:", m_markFace);

    // X coordinate
    m_markX = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.2f", cur.markX));
    m_markX->SetHint("X coordinate");
    addRow(markBox, "X:", m_markX);

    // Y coordinate
    m_markY = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.2f", cur.markY));
    m_markY->SetHint("Y coordinate");
    addRow(markBox, "Y:", m_markY);

    // Angle
    m_markAngle = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.2f", cur.markAngle));
    m_markAngle->SetHint("Angle");
    addRow(markBox, "Angle:", m_markAngle);

    root->Add(markBox, 0, wxEXPAND|wxALL, 6);

    // OK / Cancel buttons.
    root->Add(new wxStaticLine(this), 0, wxEXPAND|wxLEFT|wxRIGHT, 6);
    root->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND|wxALL, 8);

    SetSizerAndFit(root);
    Centre();
}

FncSettings SettingsDialog::GetSettings() const {
    FncSettings s;
    s.drillType          = m_drillType->GetStringSelection().ToStdString();
    s.removeHoles        = m_removeHoles->GetValue();
    s.removeSlots        = m_removeSlots->GetValue();
    s.includePrfHeader   = m_includePrf->GetValue();
    s.includeBarNests    = m_includeBars->GetValue();
    s.exportByProfile    = m_exportByProfile->GetValue();
    s.combinedOutput     = m_combinedOutput->GetValue();
    s.constraintMaterial = m_constraintMat->GetValue().Trim().ToStdString();
    s.nestCounterStart   = m_nestStart->GetValue();

    // Automatic marking settings
    s.enableAutoMark     = m_enableAutoMark->GetValue();
    s.markFace           = m_markFace->GetStringSelection().ToStdString();
    double xVal, yVal, aVal;
    m_markX->GetValue().ToDouble(&xVal);
    m_markY->GetValue().ToDouble(&yVal);
    m_markAngle->GetValue().ToDouble(&aVal);
    s.markX              = xVal;
    s.markY              = yVal;
    s.markAngle          = aVal;

    return s;
}
