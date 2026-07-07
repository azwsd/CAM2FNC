#include "MainFrame.h"
#include "ActivationManager.h"
#include "CamParser.h"
#include "FncGenerator.h"
#include "SettingsDialog.h"

#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/config.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>
#include <wx/stdpaths.h>
#include <fstream>
#include <algorithm>
#include <set>

// Button and menu IDs used in the main frame.
enum {
    ID_LOAD = wxID_HIGHEST + 1,
    ID_REMOVE,
    ID_CLEAR,
    ID_BROWSE_DIR,
    ID_EXPORT_SEL,
    ID_EXPORT_ALL,
    ID_SETTINGS
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON   (ID_LOAD,        MainFrame::OnLoad)
    EVT_BUTTON   (ID_REMOVE,      MainFrame::OnRemove)
    EVT_BUTTON   (ID_CLEAR,       MainFrame::OnClearAll)
    EVT_BUTTON   (ID_BROWSE_DIR,  MainFrame::OnBrowseDir)
    EVT_BUTTON   (ID_EXPORT_SEL,  MainFrame::OnExportSelected)
    EVT_BUTTON   (ID_EXPORT_ALL,  MainFrame::OnExportAll)
    EVT_BUTTON   (ID_SETTINGS,    MainFrame::OnSettings)
    EVT_LIST_ITEM_SELECTED  (wxID_ANY, MainFrame::OnListSelection)
    EVT_LIST_ITEM_DESELECTED(wxID_ANY, MainFrame::OnListSelection)
    EVT_DROP_FILES(MainFrame::OnDropFiles)
wxEND_EVENT_TABLE()

// Setup the main window layout and controls.
MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "CAM2FNC Converter",
              wxDefaultPosition, wxSize(980, 620))
{
    SetMinSize(wxSize(800, 500));
    DragAcceptFiles(true);   // enable file drop

    LoadPrefs();

    // Menu bar
    auto* menuFile = new wxMenu;
    menuFile->Append(ID_LOAD,       "&Load CAM Files\tCtrl+O");
    menuFile->AppendSeparator();
    menuFile->Append(ID_EXPORT_ALL, "Export &All\tCtrl+E");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT,     "E&xit");

    auto* menuTools = new wxMenu;
    menuTools->Append(ID_SETTINGS,  "&Settings\tCtrl+,");

    auto* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT, "&About CAM2FNC");

    auto* menuBar = new wxMenuBar;
    menuBar->Append(menuFile,  "&File");
    menuBar->Append(menuTools, "&Tools");
    menuBar->Append(menuHelp,  "&Help");
    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, [&](wxCommandEvent&){ Close(); }, wxID_EXIT);
    Bind(wxEVT_MENU, [&](wxCommandEvent& e){ OnLoad(e);        }, ID_LOAD);
    Bind(wxEVT_MENU, [&](wxCommandEvent& e){ OnExportAll(e);   }, ID_EXPORT_ALL);
    Bind(wxEVT_MENU, [&](wxCommandEvent& e){ OnSettings(e);    }, ID_SETTINGS);
    Bind(wxEVT_MENU, [&](wxCommandEvent&){
        wxMessageBox(
            "CAM2FNC Converter\n"
            "Version 1.0\n\n"
            "An application used to convert CAM files to FNC format.\n\n"
            "Made by Ahmed Mohamed Ragab\n"
            "ah_ra_2010@hotmail.com",
            "About CAM2FNC Converter",
            wxOK | wxICON_INFORMATION, this);
    }, wxID_ABOUT);

    // Status bar at the bottom
    m_status = CreateStatusBar(2);
    m_status->SetStatusWidths(2, new int[]{ -1, 160 });
    SetStatusText("Ready, load CAM files to begin.");

    // Main layout with toolbar, file list, and export options
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* panel = new wxPanel(this);
    auto* pRoot = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(pRoot);

    // Toolbar row with action buttons
    auto* toolRow = new wxBoxSizer(wxHORIZONTAL);

    auto makeBtn = [&](int id, const wxString& label, const wxString& tip) -> wxButton* {
        auto* b = new wxButton(panel, id, label);
        b->SetToolTip(tip);
        return b;
    };

    auto* loadBtn      = makeBtn(ID_LOAD,       "Load CAM Files...",
                                 "Open .CAM files (or drag & drop onto the list)");
    m_removeBtn        = makeBtn(ID_REMOVE,     "Remove Selected",
                                 "Remove selected files from the list");
    auto* clearBtn     = makeBtn(ID_CLEAR,      "Clear All",
                                 "Remove all files from the list");
    auto* settingsBtn  = makeBtn(ID_SETTINGS,   "Settings",
                                 "Configure FNC export options");

    m_removeBtn->Enable(false);

    toolRow->Add(loadBtn,     0, wxRIGHT, 4);
    toolRow->Add(m_removeBtn, 0, wxRIGHT, 4);
    toolRow->Add(clearBtn,    0, wxRIGHT, 4);
    toolRow->AddStretchSpacer();
    toolRow->Add(settingsBtn, 0);
    pRoot->Add(toolRow, 0, wxEXPAND | wxALL, 6);

    // File list showing loaded CAM files
    m_list = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_HRULES | wxLC_VRULES);

    const struct { const char* label; int width; int align; } cols[] = {
        { "File",         200, wxLIST_FORMAT_LEFT   },
        { "Project",      110, wxLIST_FORMAT_LEFT   },
        { "Profile",       90, wxLIST_FORMAT_LEFT   },
        { "Type",          50, wxLIST_FORMAT_CENTER },
        { "Pieces",        55, wxLIST_FORMAT_CENTER },
        { "Bars",          45, wxLIST_FORMAT_CENTER },
        { "Stocks",        55, wxLIST_FORMAT_CENTER },
        { "Holes",         50, wxLIST_FORMAT_CENTER },
        { "Status",       100, wxLIST_FORMAT_LEFT   },
    };
    for (int i = 0; i < COL_COUNT; ++i)
        m_list->InsertColumn(i, cols[i].label, cols[i].align, cols[i].width);

    pRoot->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);

    // Export directory selector
    auto* dirBox   = new wxStaticBoxSizer(
        new wxStaticBox(panel, wxID_ANY, "Export Location"), wxHORIZONTAL);
    m_dirLabel = new wxStaticText(panel, wxID_ANY, m_exportDir,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxST_ELLIPSIZE_MIDDLE | wxST_NO_AUTORESIZE);
    auto* browseBtn = new wxButton(panel, ID_BROWSE_DIR, "Browse\u2026",
                                   wxDefaultPosition, wxSize(80, -1));
    dirBox->Add(m_dirLabel, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    dirBox->Add(browseBtn,  0, wxALIGN_CENTER_VERTICAL);
    pRoot->Add(dirBox, 0, wxEXPAND | wxALL, 6);

    // Export buttons and progress bar
    auto* exportRow = new wxBoxSizer(wxHORIZONTAL);
    m_exportSelBtn  = makeBtn(ID_EXPORT_SEL, "Export Selected",
                              "Export selected files to FNC");
    m_exportAllBtn  = makeBtn(ID_EXPORT_ALL, "Export All",
                              "Export all loaded files to FNC");
    m_exportSelBtn->Enable(false);
    m_exportAllBtn->Enable(false);

    m_progress = new wxGauge(panel, wxID_ANY, 100,
                             wxDefaultPosition, wxSize(-1, 20));
    m_progress->Hide();

    exportRow->Add(m_exportSelBtn, 0, wxRIGHT, 6);
    exportRow->Add(m_exportAllBtn, 0, wxRIGHT, 6);
    exportRow->Add(m_progress,     1, wxALIGN_CENTER_VERTICAL);
    pRoot->Add(exportRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    // Finish window setup
    root->Add(panel, 1, wxEXPAND);
    SetSizer(root);
    Centre();
}

// Handle files dropped onto the window.
void MainFrame::OnDropFiles(wxDropFilesEvent& e) {
    int n = e.GetNumberOfFiles();
    wxString* files = e.GetFiles();
    for (int i = 0; i < n; ++i) {
        wxFileName fn(files[i]);
        if (fn.GetExt().CmpNoCase("CAM") == 0)
            LoadFile(files[i].ToStdString());
    }
}

// Load a single CAM file into the list.
void MainFrame::LoadFile(const std::string& path) {
    for (auto& f : m_files) {
        if (f.filePath == path) {
            SetStatusText("Already loaded: " + path, true);
            return;
        }
    }

    CamFile cam = CamParser::parse(path);
    if (cam.pieces.empty() && cam.bars.empty()) {
        SetStatusText("Could not parse (or empty): " + cam.fileName, true);
        return;
    }

    m_files.push_back(cam);
    long row = m_list->GetItemCount();
    
    // Insert the actual filename in the first column
    m_list->InsertItem(row, cam.fileName); 
    
    // Refresh the rest of the columns (Project, Profile, Status)
    RefreshListRow(row);

    m_exportAllBtn->Enable(true);
    SetStatusText("Loaded: " + cam.fileName +
                  "  (" + std::to_string(cam.pieces.size()) + " pieces, " +
                  std::to_string(cam.bars.size()) + " bars)");
}

// Refresh the file list row after loading or parsing.
void MainFrame::RefreshListRow(long row) {
    if (row < 0 || row >= (long)m_files.size()) return;
    const CamFile& f = m_files[row];

    // Collect unique section types
    std::set<std::string> types;
    for (auto& p : f.pieces) types.insert(p.sectionType);
    std::string typeStr;
    for (auto& t : types) { if (!typeStr.empty()) typeStr += " "; typeStr += t; }

    m_list->SetItem(row, COL_FILE,    f.fileName);
    m_list->SetItem(row, COL_PROJECT, f.projectName());
    m_list->SetItem(row, COL_PROFILE, f.profileSummary());
    m_list->SetItem(row, COL_TYPE,    typeStr);
    m_list->SetItem(row, COL_PIECES,  std::to_string(f.pieces.size()));
    m_list->SetItem(row, COL_BARS,    std::to_string(f.bars.size()));
    m_list->SetItem(row, COL_STOCKS,  std::to_string(f.stocks.size()));
    m_list->SetItem(row, COL_HOLES,   std::to_string(f.totalHoles()));
    m_list->SetItem(row, COL_STATUS,  "Ready");
}

// Remove characters that are not safe for filenames.
static std::string sanitiseProfile(std::string prof) {
    for (char& c : prof)
        if (c == '/' || c == '\\' || c == '*' || c == '?' ||
            c == ':' || c == '<'  || c == '>'  || c == '|' || c == '"')
            c = '_';
    return prof;
}

// Export one file and return true on success.
bool MainFrame::ExportFile(const CamFile& cam, const std::string& outDir, int& nestCounter) {
    try {
        std::vector<std::string> warnings;
        auto collector = [&](const std::string& msg) { warnings.push_back(msg); };

        auto showWarnings = [&](const std::string& fileName) {
            if (!warnings.empty()) {
                std::set<std::string> wUnique(warnings.begin(), warnings.end());
                std::string summary = "The following items were skipped in " + fileName + ":\n\n";
                for (const auto& w : wUnique) summary += "- " + w + "\n";
                wxMessageBox(summary, "Export Warnings", wxOK | wxICON_WARNING, this);
                warnings.clear();
            }
        };

        if (m_settings.exportByProfile) {
            // Collect unique profiles from this file
            std::set<std::string> uniqueProfiles;
            for (const auto& p : cam.pieces) if (!p.profile.empty()) uniqueProfiles.insert(p.profile);
            for (const auto& b : cam.bars)   if (!b.profile.empty()) uniqueProfiles.insert(b.profile);
            if (uniqueProfiles.empty()) return false;

            bool allOk = true;
            for (const auto& prof : uniqueProfiles) {
                CamFile pf = cam;
                pf.pieces.clear(); pf.bars.clear();
                for (const auto& p : cam.pieces) if (p.profile == prof) pf.pieces.push_back(p);
                for (const auto& b : cam.bars)   if (b.profile == prof) pf.bars.push_back(b);

                std::string fncData = FncGenerator::generate(pf, m_settings, collector, &nestCounter);
                showWarnings(cam.fileName + " [" + prof + "]");

                wxFileName fn(outDir, cam.fileName);
                wxFileName outFn(outDir, fn.GetName() + "_" + sanitiseProfile(prof));
                outFn.SetExt("fnc");

                std::ofstream ofs(outFn.GetFullPath().ToStdString());
                if (!ofs.is_open()) { allOk = false; continue; }
                ofs << fncData;
            }
            return allOk;
        } else {
            std::string fncData = FncGenerator::generate(cam, m_settings, collector, &nestCounter);
            showWarnings(cam.fileName);

            wxFileName fn(outDir, cam.fileName);
            fn.SetExt("fnc");
            std::ofstream ofs(fn.GetFullPath().ToStdString());
            if (!ofs.is_open()) return false;
            ofs << fncData;
            return true;
        }
    } catch (...) {
        return false;
    }
}

// Run export for the selected files. Handles all four modes: combined/profile splits.
void MainFrame::RunExport(const std::vector<int>& indices) {
    if (indices.empty()) return;

    if (!ActivationManager::verifyRuntime()) {
        wxMessageBox("License validation failed. Please activate the application.",
                     "Export Blocked", wxOK | wxICON_ERROR, this);
        return;
    }

    wxFileName checkDir(m_exportDir, "");
    if (!checkDir.DirExists()) {
        wxMessageBox("Export directory does not exist:\n" + m_exportDir,
                     "Export Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Shared nest counter across all files in this run
    int nestCounter = m_settings.nestCounterStart;

    m_progress->SetRange((int)indices.size());
    m_progress->Show();

    int ok = 0, fail = 0;

    // ── Combined mode ────────────────────────────────────────────
    if (m_settings.combinedOutput) {
        if (m_settings.exportByProfile) {
            // Combined + by profile: one output file per unique profile
            // across all selected inputs
            std::set<std::string> allProfiles;
            for (int idx : indices) {
                const CamFile& cam = m_files[idx];
                for (const auto& p : cam.pieces) if (!p.profile.empty()) allProfiles.insert(p.profile);
                for (const auto& b : cam.bars)   if (!b.profile.empty()) allProfiles.insert(b.profile);
            }

            for (const auto& prof : allProfiles) {
                // Build a merged CamFile for this profile
                CamFile merged;
                merged.fileName = "combined_" + sanitiseProfile(prof);
                for (int idx : indices) {
                    const CamFile& cam = m_files[idx];
                    for (const auto& p : cam.pieces) if (p.profile == prof) merged.pieces.push_back(p);
                    for (const auto& b : cam.bars)   if (b.profile == prof) merged.bars.push_back(b);
                }

                std::vector<std::string> warnings;
                auto collector = [&](const std::string& msg) { warnings.push_back(msg); };
                std::string fncData = FncGenerator::generate(merged, m_settings, collector, &nestCounter);

                if (!warnings.empty()) {
                    std::set<std::string> wUnique(warnings.begin(), warnings.end());
                    std::string summary = "Skipped items for profile " + prof + ":\n\n";
                    for (const auto& w : wUnique) summary += "- " + w + "\n";
                    wxMessageBox(summary, "Export Warnings", wxOK | wxICON_WARNING, this);
                }

                wxFileName outFn(m_exportDir, "combined_" + sanitiseProfile(prof));
                outFn.SetExt("fnc");
                std::ofstream ofs(outFn.GetFullPath().ToStdString());
                if (ofs.is_open()) { ofs << fncData; ++ok; }
                else ++fail;
            }

            // Mark all selected rows
            for (int i = 0; i < (int)indices.size(); ++i) {
                m_list->SetItem(indices[i], COL_STATUS, ok > 0 ? "Exported [OK]" : "Error [FAIL]");
                m_progress->SetValue(i + 1);
                wxYield();
            }
        } else {
            // Combined + no profile: merge everything into one file
            CamFile merged;
            merged.fileName = "combined";
            for (int idx : indices) {
                const CamFile& cam = m_files[idx];
                for (const auto& p : cam.pieces) merged.pieces.push_back(p);
                for (const auto& b : cam.bars)   merged.bars.push_back(b);
            }

            std::vector<std::string> warnings;
            auto collector = [&](const std::string& msg) { warnings.push_back(msg); };
            std::string fncData = FncGenerator::generate(merged, m_settings, collector, &nestCounter);

            if (!warnings.empty()) {
                std::set<std::string> wUnique(warnings.begin(), warnings.end());
                std::string summary = "Skipped items in combined export:\n\n";
                for (const auto& w : wUnique) summary += "- " + w + "\n";
                wxMessageBox(summary, "Export Warnings", wxOK | wxICON_WARNING, this);
            }

            wxFileName outFn(m_exportDir, "combined");
            outFn.SetExt("fnc");
            std::ofstream ofs(outFn.GetFullPath().ToStdString());
            bool success = false;
            if (ofs.is_open()) { ofs << fncData; success = true; }

            for (int i = 0; i < (int)indices.size(); ++i) {
                m_list->SetItem(indices[i], COL_STATUS, success ? "Exported [OK]" : "Error [FAIL]");
                m_progress->SetValue(i + 1);
                wxYield();
            }
            success ? ++ok : ++fail;
        }
    } else {
        // ── Normal per-file mode ──────────────────────────────────
        for (int i = 0; i < (int)indices.size(); ++i) {
            int r = indices[i];
            bool success = ExportFile(m_files[r], m_exportDir, nestCounter);
            m_list->SetItem(r, COL_STATUS, success ? "Exported [OK]" : "Error [FAIL]");
            success ? ++ok : ++fail;
            m_progress->SetValue(i + 1);
            wxYield();
        }
    }

    m_progress->Hide();

    // ── Update nest counter so next export continues from here ────
    m_settings.nestCounterStart = nestCounter;
    SavePrefs();

    SetStatusText("Export complete: " + std::to_string(ok) + " ok, " +
                  std::to_string(fail) + " failed.  -> " + m_exportDir);
}

void MainFrame::SetStatusText(const std::string& msg, bool /*isError*/) {
    m_status->SetStatusText(msg, 0);
}

// Save preferences to the system config (registry on Windows).
void MainFrame::SavePrefs() {
    wxConfig cfg("CAM2FNC");
    cfg.Write("exportDir",       wxString(m_exportDir));
    cfg.Write("drillType",       wxString(m_settings.drillType));
    cfg.Write("removeHoles",     m_settings.removeHoles);
    cfg.Write("removeSlots",     m_settings.removeSlots);
    cfg.Write("includePrf",      m_settings.includePrfHeader);
    cfg.Write("includeBars",     m_settings.includeBarNests);
    cfg.Write("exportByProfile", m_settings.exportByProfile);
    cfg.Write("combinedOutput",  m_settings.combinedOutput);
    cfg.Write("constraintMat",   wxString(m_settings.constraintMaterial));
    cfg.Write("nestStart",       (long)m_settings.nestCounterStart);

    // Automatic marking settings
    cfg.Write("enableAutoMark",  m_settings.enableAutoMark);
    cfg.Write("markFace",       wxString(m_settings.markFace));
    cfg.Write("markX",          m_settings.markX);
    cfg.Write("markY",          m_settings.markY);
    cfg.Write("markAngle",      m_settings.markAngle);
}

// Load preferences from the system config.
void MainFrame::LoadPrefs() {
    wxConfig cfg("CAM2FNC");
    wxString dir;
    cfg.Read("exportDir",  &dir,
             wxStandardPaths::Get().GetDocumentsDir());
    m_exportDir = dir.ToStdString();

    wxString dt; cfg.Read("drillType", &dt, "Drill");
    m_settings.drillType = dt.ToStdString();
    cfg.Read("removeHoles",  &m_settings.removeHoles,      false);
    cfg.Read("removeSlots",  &m_settings.removeSlots,      true);
    cfg.Read("includePrf",   &m_settings.includePrfHeader, true);
    cfg.Read("includeBars",  &m_settings.includeBarNests,  true);
    cfg.Read("exportByProfile", &m_settings.exportByProfile, false);
    cfg.Read("combinedOutput",  &m_settings.combinedOutput,  false);
    wxString cm; cfg.Read("constraintMat", &cm, "");
    m_settings.constraintMaterial = cm.ToStdString();
    long ns = 0; cfg.Read("nestStart", &ns, (long)GetDefaultNestNumber());
    // If saved value is the old default (1), use new date-based default instead
    if (ns == 1) ns = GetDefaultNestNumber();
    m_settings.nestCounterStart = (int)ns;

    // Automatic marking settings
    cfg.Read("enableAutoMark",  &m_settings.enableAutoMark, false);
    wxString mf; cfg.Read("markFace", &mf, "DA");
    m_settings.markFace = mf.ToStdString();
    double mx = 0, my = 0, ma = 0;
    cfg.Read("markX", &mx, 0.0);
    cfg.Read("markY", &my, 0.0);
    cfg.Read("markAngle", &ma, 0.0);
    m_settings.markX = mx;
    m_settings.markY = my;
    m_settings.markAngle = ma;
}

// Button click and other event handlers.
void MainFrame::OnLoad(wxCommandEvent&) {
    wxFileDialog dlg(this, "Open CAM Files", "", "",
                     "CAM Files (*.CAM;*.cam)|*.CAM;*.cam|All Files (*.*)|*.*",
                     wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    wxArrayString paths;
    dlg.GetPaths(paths);
    for (auto& p : paths)
        LoadFile(p.ToStdString());
}

void MainFrame::OnRemove(wxCommandEvent&) {
    std::vector<long> rows;
    long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    while (item != wxNOT_FOUND) {
        rows.push_back(item);
        item = m_list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    }
    // Remove from back to front to keep indices valid
    std::sort(rows.rbegin(), rows.rend());
    for (long r : rows) {
        m_list->DeleteItem(r);
        m_files.erase(m_files.begin() + r);
    }
    m_exportAllBtn->Enable(!m_files.empty());
    m_exportSelBtn->Enable(false);
    m_removeBtn->Enable(false);
    SetStatusText(std::to_string(rows.size()) + " file(s) removed.");
}

void MainFrame::OnClearAll(wxCommandEvent&) {
    if (m_files.empty()) return;
    if (wxMessageBox("Remove all loaded files from the list?",
                     "Clear All", wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
    m_files.clear();
    m_list->DeleteAllItems();
    m_exportAllBtn->Enable(false);
    m_exportSelBtn->Enable(false);
    m_removeBtn->Enable(false);
    SetStatusText("Cleared.");
}

void MainFrame::OnBrowseDir(wxCommandEvent&) {
    wxDirDialog dlg(this, "Select Export Directory", m_exportDir,
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    m_exportDir = dlg.GetPath().ToStdString();
    m_dirLabel->SetLabel(m_exportDir);
    SavePrefs();
}

void MainFrame::OnExportSelected(wxCommandEvent&) {
    std::vector<int> indices;
    long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    while (item != wxNOT_FOUND) {
        indices.push_back((int)item);
        item = m_list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    }
    RunExport(indices);
}

void MainFrame::OnExportAll(wxCommandEvent&) {
    if (m_files.empty()) return;
    std::vector<int> indices;
    for (int i = 0; i < (int)m_files.size(); ++i) indices.push_back(i);
    RunExport(indices);
}

void MainFrame::OnSettings(wxCommandEvent&) {
    SettingsDialog dlg(this, m_settings);
    if (dlg.ShowModal() == wxID_OK) {
        m_settings = dlg.GetSettings();
        SavePrefs();
        SetStatusText("Settings saved.");
    }
}

void MainFrame::OnListSelection(wxListEvent&) {
    bool any = (m_list->GetSelectedItemCount() > 0);
    m_exportSelBtn->Enable(any);
    m_removeBtn->Enable(any);
}
