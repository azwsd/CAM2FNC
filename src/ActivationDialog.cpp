#include "ActivationDialog.h"

#include "ActivationManager.h"

#include <wx/clipbrd.h>
#include <wx/utils.h>

#include <cstring>

enum {
    ID_ACTIVATE    = wxID_HIGHEST + 100,
    ID_COPY_EMAIL,
    ID_COPY_FP,
    ID_EMAIL_REQ
};

namespace {

wxString UriEncodeComponent(const wxString& value) {
    static const char* safe = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    wxString encoded;
    const wxScopedCharBuffer utf8 = value.utf8_str();
    encoded.reserve(utf8.length() * 3);

    for (size_t i = 0; i < utf8.length(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(utf8[i]);
        if (std::strchr(safe, static_cast<char>(ch))) {
            encoded += static_cast<char>(ch);
        } else {
            encoded += wxString::Format("%%%02X", ch);
        }
    }
    return encoded;
}

} // namespace

ActivationDialog::ActivationDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "CAM2FNC Activation",
               wxDefaultPosition, wxSize(480, 340),
               wxDEFAULT_DIALOG_STYLE)
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, "CAM2FNC Converter - Activation Required");
    wxFont f = title->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(f);
    root->Add(title, 0, wxALL, 10);

    root->Add(new wxStaticText(this, wxID_ANY,
        "Send your Machine ID to the contact email below to receive a license key."),
        0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* emailSizer = new wxBoxSizer(wxHORIZONTAL);
    emailSizer->Add(new wxStaticText(this, wxID_ANY, "Contact:"),
                    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_email = new wxTextCtrl(this, wxID_ANY, kLicenseEmail,
        wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    emailSizer->Add(m_email, 1, wxEXPAND);
    emailSizer->Add(new wxButton(this, ID_COPY_EMAIL, "Copy"), 0, wxLEFT, 4);
    root->Add(emailSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* fpSizer = new wxBoxSizer(wxHORIZONTAL);
    fpSizer->Add(new wxStaticText(this, wxID_ANY, "Machine ID:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_fingerprint = new wxTextCtrl(this, wxID_ANY,
        wxString(ActivationManager::getMachineFingerprint()),
        wxDefaultPosition, wxDefaultSize,
        wxTE_READONLY);
    fpSizer->Add(m_fingerprint, 1, wxEXPAND);
    fpSizer->Add(new wxButton(this, ID_COPY_FP, "Copy"), 0, wxLEFT, 4);
    fpSizer->Add(new wxButton(this, ID_EMAIL_REQ, "Email Request"), 0, wxLEFT, 4);
    root->Add(fpSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* keySizer = new wxBoxSizer(wxHORIZONTAL);
    keySizer->Add(new wxStaticText(this, wxID_ANY, "License Key:"),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_keyInput = new wxTextCtrl(this, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize);
    keySizer->Add(m_keyInput, 1, wxEXPAND);
    root->Add(keySizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(new wxButton(this, ID_ACTIVATE, "Activate"), 0, wxRIGHT, 6);
    btnSizer->Add(new wxButton(this, wxID_CANCEL,  "Exit"),    0);
    root->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizerAndFit(root);
    SetMinSize(wxSize(480, -1));

    Bind(wxEVT_BUTTON, &ActivationDialog::OnActivate,        this, ID_ACTIVATE);
    Bind(wxEVT_BUTTON, &ActivationDialog::OnCopyEmail,      this, ID_COPY_EMAIL);
    Bind(wxEVT_BUTTON, &ActivationDialog::OnCopyFingerprint, this, ID_COPY_FP);
    Bind(wxEVT_BUTTON, &ActivationDialog::OnEmailRequest,   this, ID_EMAIL_REQ);
}

void ActivationDialog::OnActivate(wxCommandEvent&) {
    std::string key = m_keyInput->GetValue().Trim().ToStdString();
    if (key.empty()) {
        wxMessageBox("Please enter a license key.", "Activation", wxOK | wxICON_WARNING, this);
        return;
    }
    if (ActivationManager::validateKey(key)) {
        ActivationManager::saveKey(key);
        m_activated = true;
        wxMessageBox("Activation successful! Thank you.", "Activation",
                     wxOK | wxICON_INFORMATION, this);
        EndModal(wxID_OK);
    } else {
        wxMessageBox("Invalid license key. Please check and try again.",
                     "Activation Failed", wxOK | wxICON_ERROR, this);
    }
}

void ActivationDialog::OnCopyEmail(wxCommandEvent&) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_email->GetValue()));
        wxTheClipboard->Close();
        wxMessageBox("Email address copied to clipboard.", "Copied",
                     wxOK | wxICON_INFORMATION, this);
    }
}

void ActivationDialog::OnCopyFingerprint(wxCommandEvent&) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_fingerprint->GetValue()));
        wxTheClipboard->Close();
        wxMessageBox("Machine ID copied to clipboard.", "Copied",
                     wxOK | wxICON_INFORMATION, this);
    }
}

void ActivationDialog::OnEmailRequest(wxCommandEvent&) {
    const wxString subject = "CAM2FNC License Request";
    const wxString body = wxString::Format(
        "Hello,\r\n\r\n"
        "Please send a license key for the following Machine ID:\r\n\r\n"
        "%s\r\n\r\n"
        "Thank you.",
        m_fingerprint->GetValue());

    const wxString mailto = wxString::Format(
        "mailto:%s?subject=%s&body=%s",
        kLicenseEmail,
        UriEncodeComponent(subject),
        UriEncodeComponent(body));

    if (!wxLaunchDefaultBrowser(mailto)) {
        wxMessageBox("Could not open your email application.\n"
                     "Please copy the Machine ID and email it manually.",
                     "Email Error", wxOK | wxICON_WARNING, this);
    }
}
