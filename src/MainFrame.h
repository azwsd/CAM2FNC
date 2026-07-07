#pragma once
#include "CamTypes.h"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/gauge.h>
#include <vector>
#include <string>

class MainFrame : public wxFrame {
public:
    MainFrame();

private:
    // Controls on the main window like the file list, buttons, and status bar.
    wxListCtrl*  m_list;
    wxStaticText* m_dirLabel;
    wxButton*    m_exportSelBtn;
    wxButton*    m_exportAllBtn;
    wxButton*    m_removeBtn;
    wxGauge*     m_progress;
    wxStatusBar* m_status;

    // Loaded files and export settings.
    std::vector<CamFile> m_files;
    std::string          m_exportDir;
    FncSettings          m_settings;

    // Button click and list event handlers.
    void OnLoad(wxCommandEvent&);
    void OnRemove(wxCommandEvent&);
    void OnClearAll(wxCommandEvent&);
    void OnBrowseDir(wxCommandEvent&);
    void OnExportSelected(wxCommandEvent&);
    void OnExportAll(wxCommandEvent&);
    void OnSettings(wxCommandEvent&);
    void OnListSelection(wxListEvent&);
    void OnDropFiles(wxDropFilesEvent&);

    // Load a file, refresh the list, export files, and save/load settings.
    void LoadFile(const std::string& path);
    void RefreshListRow(long row);
    // ExportFile: exports one CAM file using shared nestCounter, returns success
    bool ExportFile(const CamFile& cam, const std::string& dir, int& nestCounter);
    // RunExport: handles all 4 modes (combined/profile combinations)
    void RunExport(const std::vector<int>& indices);
    void SetStatusText(const std::string& msg, bool isError = false);
    void SavePrefs();
    void LoadPrefs();

    // Number of columns in the file list table.
    enum Col {
        COL_FILE = 0, COL_PROJECT, COL_PROFILE, COL_TYPE,
        COL_PIECES, COL_BARS, COL_STOCKS, COL_HOLES, COL_STATUS,
        COL_COUNT
    };

    wxDECLARE_EVENT_TABLE();
};
