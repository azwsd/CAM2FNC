#pragma once
#include <wx/dialog.h>
#include <wx/wx.h>

class ActivationDialog : public wxDialog {
public:
    ActivationDialog(wxWindow* parent);
    bool IsActivated() const { return m_activated; }

private:
    static constexpr const char* kLicenseEmail = "ah_ra_2010@hotmail.com";

    wxTextCtrl* m_email;
    wxTextCtrl* m_fingerprint;
    wxTextCtrl* m_keyInput;
    bool        m_activated = false;

    void OnActivate(wxCommandEvent&);
    void OnCopyEmail(wxCommandEvent&);
    void OnCopyFingerprint(wxCommandEvent&);
    void OnEmailRequest(wxCommandEvent&);
};
