#ifndef _WIPE_TOWER_DIALOG_H_
#define _WIPE_TOWER_DIALOG_H_

#include "GUI_Utils.hpp"

#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/msgdlg.h>
class Button;
class Label;

class WipingPanel : public wxPanel {
public:
    // BBS
    WipingPanel(wxWindow* parent, const std::vector<float>& matrix, const std::vector<float>& extruders, size_t cur_extruder_id,
        const std::vector<std::string>& extruder_colours, Button* calc_button,
        const std::vector<std::vector<int>>& extra_flush_volume, const std::vector<float>& flush_multiplier, size_t nozzle_nums);
    std::vector<float> read_matrix_values();
    std::vector<float> read_extruders_values();
    void toggle_advanced(bool user_action = false);
    void create_panels(wxWindow* parent, const int num);
    void calc_flushing_volumes();
    void msw_rescale();
    wxBoxSizer* create_calc_btn_sizer(wxWindow* parent);

    float get_flush_multiplier()
    {
        if (m_flush_multiplier_ebox == nullptr)
            return 1.f;

        return wxAtof(m_flush_multiplier_ebox->GetValue());
    }

    std::vector<float> get_flush_multiplier_vector() { return m_flush_multiplier; }

private:
    void on_select_extruder(wxCommandEvent &evt);
    void generate_display_matrix(); // generate display_matrix frem matrix
    void back_matrix();
    void update_table();  // if matrix is modified update the table
    void fill_in_matrix();
    bool advanced_matches_simple();
    int calc_flushing_volume(const wxColour& from, const wxColour& to,int min_flush_volume);
    void update_warning_texts();

    std::vector<wxSpinCtrl*> m_old;
    std::vector<wxSpinCtrl*> m_new;
    std::vector<std::vector<wxTextCtrl*>> edit_boxes;
    std::vector<wxColour> m_colours;
    unsigned int m_number_of_extruders  = 0;
    bool m_advanced                     = false;
	wxPanel*	m_page_simple = nullptr;
	wxPanel*	m_page_advanced = nullptr;
    wxPanel* header_line_panel = nullptr;
    wxBoxSizer*	m_sizer = nullptr;
    wxBoxSizer* m_sizer_simple = nullptr;
    wxBoxSizer* m_sizer_advanced = nullptr;
    wxGridSizer* m_gridsizer_advanced = nullptr;
    wxButton* m_widget_button     = nullptr;
    Label* m_tip_message_label = nullptr;

    std::vector<wxButton *> icon_list1;
    std::vector<wxButton *> icon_list2;

    const std::vector<std::vector<int>> m_min_flush_volume;
    const int m_max_flush_volume;

    wxTextCtrl* m_flush_multiplier_ebox = nullptr;
    wxStaticText* m_min_flush_label = nullptr;

    std::vector<float> m_flush_multiplier;
    std::vector<float> m_matrix;
    std::vector<float> m_display_matrix;
    size_t             m_cur_extruder_id;
    size_t             m_nozzle_nums;
};





class WipingDialog : public Slic3r::GUI::DPIDialog
{
public:
    WipingDialog(wxWindow* parent, const std::vector<float>& matrix, const std::vector<float>& extruders, const std::vector<std::string>& extruder_colours,
        const std::vector<std::vector<int>>&extra_flush_volume, const std::vector<float>& flush_multiplier, size_t nozzle_nums);
    std::vector<float> get_matrix() const    { return m_output_matrix; }
    std::vector<float> get_extruders() const { return m_output_extruders; }
    wxBoxSizer* create_btn_sizer(long flags);

    float get_flush_multiplier()
    {
        if (m_panel_wiping == nullptr)
            return 1.f;

        return m_panel_wiping->get_flush_multiplier();
    }

    std::vector<float> get_flush_multiplier_vector() const {
        if (m_panel_wiping == nullptr)
            return {1.f, 1.f};
        return m_panel_wiping->get_flush_multiplier_vector();
    }

    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    WipingPanel*  m_panel_wiping  = nullptr;
    std::vector<float> m_output_matrix;
    std::vector<float> m_output_extruders;
    std::unordered_map<int, Button *> m_button_list;
};

#endif  // _WIPE_TOWER_DIALOG_H_