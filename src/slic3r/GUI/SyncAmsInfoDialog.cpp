#include "SyncAmsInfoDialog.hpp"

#include <thread>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/dcmemory.h>
#include "GUI_App.hpp"
#include "Tab.hpp"
#include "PartPlate.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/TextInput.hpp"
#include "Notebook.hpp"
#include <chrono>
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "CapsuleButton.hpp"
using namespace Slic3r;
using namespace Slic3r::GUI;

#define BUTTON_SIZE             wxSize(FromDIP(58), FromDIP(24))
#define SyncAmsInfoDialogWidth  FromDIP(675)

namespace Slic3r { namespace GUI {
wxDEFINE_EVENT(EVT_CLEAR_IPADDRESS, wxCommandEvent);
wxDEFINE_EVENT(EVT_UPDATE_USER_MACHINE_LIST, wxCommandEvent);
wxDEFINE_EVENT(EVT_PRINT_JOB_CANCEL, wxCommandEvent);
#define SYNC_FLEX_GRID_COL 7
bool SyncAmsInfoDialog::Show(bool show)
{
    if (show) {
        if (m_options_other) { m_options_other->Hide(); }
        if (m_refresh_timer) { m_refresh_timer->Start(LIST_REFRESH_INTERVAL); }
    } else {
        m_refresh_timer->Stop();

        DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev) {
            MachineObject *obj_ = dev->get_selected_machine();
            if (obj_ && obj_->connection_type() == "cloud" /*&& m_print_type == FROM_SDCARD_VIEW*/) {
                if (obj_->is_connected()) { obj_->disconnect(); }
            }
        }

        return DPIDialog::Show(false);
    }

    PresetBundle &preset_bundle  = *wxGetApp().preset_bundle;
    const auto &  project_config = preset_bundle.project_config;

    const t_config_enum_values &     enum_keys_map = ConfigOptionEnum<BedType>::get_enum_values();
    const ConfigOptionEnum<BedType> *bed_type      = project_config.option<ConfigOptionEnum<BedType>>("curr_bed_type");
    std::string                      plate_name;
    for (auto &elem : enum_keys_map) {
        if (elem.second == bed_type->value) plate_name = elem.first;
    }

    if (plate_name.empty()) {
        m_text_bed_type->Hide();
    } else {
        wxString name = format_bed_name(plate_name);
        if (name.length() > 8) {
            m_text_bed_type->SetFont(Label::Body_9);
        } else {
            m_text_bed_type->SetFont(Label::Body_12);
        }
        m_text_bed_type->SetLabelText(name);
    }
    // set default value when show this dialog
    wxGetApp().UpdateDlgDarkUI(this);
    wxGetApp().reset_to_active();
    set_default(true);
    update_user_machine_list();
    { // hide and hide
        m_basic_panel->Hide();
        m_simplebook->Hide();
        m_panel_finish->Hide();
        m_panel_prepare->Hide();
        m_rename_normal_panel->Hide();
        m_rename_switch_panel->Hide();
        m_button_ensure->Hide();
        m_sw_print_failed_info->Hide();
        m_rename_text -> Hide();
        m_rename_button->Hide();
        m_rename_edit_panel->Hide();
        m_rename_input->Hide();
        //print_time->Hide();
        hide_no_use_controls();
    }
    bool dirty_filament = is_dirty_filament();
    if (!m_input_info.connected_printer || m_is_empty_project || dirty_filament) {
        show_color_panel(false);
        m_filament_left_panel->Show(false); // empty_project
        m_filament_right_panel->Show(false);
        m_are_you_sure_title->Show(false);
        if (m_mode_combox_sizer) {
            m_mode_combox_sizer->Show(false);
        }
    }
    if (!m_input_info.connected_printer) {
        m_button_cancel->Hide();
        m_confirm_title->Show();
        m_confirm_title->SetLabel(_L("Printer not connected. Please connect or choose a printer on the Device page and try again."));
        SetMinSize(wxSize(FromDIP(700), -1));
        SetMaxSize(wxSize(FromDIP(700), -1));
       // m_confirm_title->SetForegroundColour(wxColour(250, 10, 10, 255));
    } else if (dirty_filament) {
         m_confirm_title->Show();
         m_confirm_title->SetLabel(_L("Synchronizing AMS filaments will discard your modified but unsaved filament presets.\nAre you sure you want to continue?"));
         SetMinSize(wxSize(FromDIP(700), -1));
         SetMaxSize(wxSize(FromDIP(700), -1));
    } else if (!m_check_dirty_fialment) {
        show_color_panel(true);
        m_filament_left_panel->Show(false); // empty_project
        m_filament_right_panel->Show(false);
        m_are_you_sure_title->Show(true);
        if (m_mode_combox_sizer) {
            m_mode_combox_sizer->Show(true);
        }
        m_confirm_title->SetLabel(_L("After sync, all currently configured filament presets and colors will be discarded."));
        SetMinSize(wxSize(SyncAmsInfoDialogWidth, -1));
        SetMaxSize(wxSize(SyncAmsInfoDialogWidth, -1));
    }
    Layout();
    Fit();
    CenterOnParent();
    return DPIDialog::Show(show);
}

void SyncAmsInfoDialog::updata_ui_data_after_connected_printer() {
    if (!m_input_info.connected_printer) { return; }
    if (is_dirty_filament()) { return; }

    show_sizer(m_sizer_line, true);
    m_two_thumbnail_panel->Show(true);

    m_attention_text->Show();
    m_tip_text->Show();
    //m_specify_color_cluster_title->Show();
    m_button_cancel->Show();
}

void SyncAmsInfoDialog::update_ams_check(MachineObject *obj)
{
    if (!obj) { return; }

    if (!obj->is_enable_np) {
        if (obj->has_ams()) {
            //m_checkbox_list["use_ams"]->Show();
            m_checkbox_list["use_ams"]->setValue("on");
        } else {
            m_checkbox_list["use_ams"]->Hide();
            m_checkbox_list["use_ams"]->setValue("off");
        }
    } else {
        m_checkbox_list["use_ams"]->Hide();
        m_checkbox_list["use_ams"]->setValue("on");
    }
}

void SyncAmsInfoDialog::update_select_layout(MachineObject *obj)
{
    m_checkbox_list["timelapse"]->Hide();
    m_checkbox_list["bed_leveling"]->Hide();
    m_checkbox_list["use_ams"]->Hide();
    m_checkbox_list["flow_cali"]->Hide();
    m_checkbox_list["nozzle_offset_cali"]->Hide();

    if (!obj) { return; }
    AppConfig *config = wxGetApp().app_config;

    if (obj->is_enable_np) {
        m_checkbox_list["nozzle_offset_cali"]->Show();
        m_checkbox_list["nozzle_offset_cali"]->update_options(ops_auto);
        m_checkbox_list["bed_leveling"]->update_options(ops_auto);
        m_checkbox_list["flow_cali"]->update_options(ops_auto);

        m_checkbox_list["nozzle_offset_cali"]->setValue("auto");
        m_checkbox_list["bed_leveling"]->setValue("auto");
        m_checkbox_list["flow_cali"]->setValue("auto");
    } else {
        m_checkbox_list["bed_leveling"]->update_options(ops_no_auto);
        m_checkbox_list["flow_cali"]->update_options(ops_auto);

        if (config && config->get("print", "bed_leveling") == "0") {
            m_checkbox_list["bed_leveling"]->setValue("off");
        } else {
            m_checkbox_list["bed_leveling"]->setValue("on");
        }

        if (config && config->get("print", "flow_cali") == "0") {
            m_checkbox_list["flow_cali"]->setValue("off");
        } else {
            m_checkbox_list["flow_cali"]->setValue("on");
        }

        update_timelapse_enable_status();
        update_flow_cali_check(obj);
    }

    if (config && config->get("print", "timelapse") == "0") {
        m_checkbox_list["timelapse"]->setValue("off");
    } else {
        m_checkbox_list["timelapse"]->setValue("on");
    }
    Layout();
    Fit();
}

void SyncAmsInfoDialog::set_default_normal(const ThumbnailData &data)
{
    update_page_turn_state(false);
    if (m_cur_input_thumbnail_data.is_valid() && m_left_image_button) {
        auto &   temp_data = m_cur_input_thumbnail_data;
        wxImage image(temp_data.width, temp_data.height);
        image.InitAlpha();
        for (unsigned int r = 0; r < temp_data.height; ++r) {
            unsigned int rr = (temp_data.height - 1 - r) * temp_data.width;
            for (unsigned int c = 0; c < temp_data.width; ++c) {
                unsigned char *px = (unsigned char *) temp_data.pixels.data() + 4 * (rr + c);
                image.SetRGB((int) c, (int) r, px[0], px[1], px[2]);
                image.SetAlpha((int) c, (int) r, px[3]);
            }
        }
        image = image.Rescale(FromDIP(LEFT_THUMBNAIL_SIZE_WIDTH), FromDIP(LEFT_THUMBNAIL_SIZE_WIDTH));
        m_left_image_button->SetBitmap(image);
    }
    if (data.is_valid() && m_right_image_button) {
        wxImage image(data.width, data.height);
        image.InitAlpha();
        for (unsigned int r = 0; r < data.height; ++r) {
            unsigned int rr = (data.height - 1 - r) * data.width;
            for (unsigned int c = 0; c < data.width; ++c) {
                unsigned char *px = (unsigned char *) data.pixels.data() + 4 * (rr + c);
                image.SetRGB((int) c, (int) r, px[0], px[1], px[2]);
                image.SetAlpha((int) c, (int) r, px[3]);
            }
        }
        image = image.Rescale(FromDIP(RIGHT_THUMBNAIL_SIZE_WIDTH), FromDIP(RIGHT_THUMBNAIL_SIZE_WIDTH));
        m_right_image_button->SetBitmap(image);
        auto extruders = wxGetApp().plater()->get_partplate_list().get_plate(m_specify_plate_idx)->get_extruders();
        if (wxGetApp().plater()->get_extruders_colors().size() == extruders.size()) {
            //m_used_colors_tip_text->Hide();
        }
        else {
            //m_used_colors_tip_text->Show();
            //m_used_colors_tip_text->SetLabel("  (" + std::to_string(extruders.size()) + " " + _L("colors used") + ")");
        }
    }
    // disable pei bed
    DeviceManager *dev_manager = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev_manager)
        return;
    MachineObject *obj_       = dev_manager->get_selected_machine();
    wxSize         screenSize = wxGetDisplaySize();
    auto           dialogSize = this->GetSize();
}

bool SyncAmsInfoDialog::is_must_finish_slice_then_connected_printer() {
    if (m_specify_plate_idx >= 0) {
        return false;
    }
    return true;
}

void SyncAmsInfoDialog::hide_no_use_controls() {
    show_sizer(sizer_basic_right_info,false);
    show_sizer(m_basicl_sizer, false);
    //show_sizer(sizer_split_filament, false);
    show_sizer(m_sizer_options_timelapse, false);
    show_sizer(m_sizer_options_other, false);
    show_sizer(sizer_advanced_options_title, false);
    show_sizer(rename_sizer_v, false);
    show_sizer(rename_sizer_h, false);
    //m_statictext_ams_msg->Hide();
}

void SyncAmsInfoDialog::show_sizer(wxSizer *sizer, bool show)
{
    if (!sizer) { return; }
    wxSizerItemList items = sizer->GetChildren();
    for (wxSizerItemList::iterator it = items.begin(); it != items.end(); ++it) {
        wxSizerItem *item = *it;
        if (wxWindow *window = item->GetWindow()) { window->Show(show); }
        if (wxSizer *son_sizer = item->GetSizer()) { show_sizer(son_sizer, show); }
    }
}

void SyncAmsInfoDialog::deal_ok()
{
    if (m_input_info.connected_printer && !m_is_empty_project) {
        if (m_map_mode == MapModeEnum::Override || !m_result.is_same_printer) {
            m_is_empty_project = true;
            return;
        }
        m_result.direct_sync = false;
        m_result.sync_maps.clear();
        for (size_t i = 0; i < m_ams_mapping_result.size(); i++) {
            auto& temp  = m_result.sync_maps[m_ams_mapping_result[i].id];
            temp.ams_id      = m_ams_mapping_result[i].ams_id;
            temp.slot_id     = m_ams_mapping_result[i].slot_id;
        }
    }
}

bool SyncAmsInfoDialog::get_is_double_extruder()
{
    const auto &full_config = wxGetApp().preset_bundle->full_config();
    size_t      nozzle_nums = full_config.option<ConfigOptionFloatsNullable>("nozzle_diameter")->values.size();
    bool use_double_extruder = nozzle_nums > 1 ? true : false;
    return use_double_extruder;
}

bool SyncAmsInfoDialog::is_dirty_filament() {
    PresetCollection *m_presets = &wxGetApp().preset_bundle->filaments;
    if (m_check_dirty_fialment && m_presets && m_presets->get_edited_preset().is_dirty) {
        return true;
    }
    return false;
}

bool SyncAmsInfoDialog::is_need_show()
{
    if (!m_input_info.connected_printer) {
        return true;
    }
    if (m_is_empty_project && !is_dirty_filament()) {
        return false;
    }
    return true;
}

wxBoxSizer *SyncAmsInfoDialog::create_sizer_thumbnail(wxButton *image_button, bool left)
{
    auto sizer_thumbnail = new wxBoxSizer(wxVERTICAL);
    if (left) {
        wxBoxSizer *text_sizer = new wxBoxSizer(wxHORIZONTAL);
        auto        sync_text  = new wxStaticText(image_button->GetParent(), wxID_ANY, _CTX(L_CONTEXT("Original", "Sync_AMS"), "Sync_AMS"));
        sync_text->SetForegroundColour(wxColour(107, 107, 107, 100));
        text_sizer->Add(sync_text, 0, wxALIGN_CENTER | wxALL, 0);
        sizer_thumbnail->Add(sync_text, FromDIP(0), wxALIGN_CENTER | wxALL, FromDIP(4));
    }
    else {
        wxBoxSizer *text_sizer = new wxBoxSizer(wxHORIZONTAL);
        auto        sync_text  = new wxStaticText(image_button->GetParent(), wxID_ANY, _L("After mapping"));
        sync_text->SetForegroundColour(wxColour(107, 107, 107, 100));
        text_sizer->Add(sync_text, 0, wxALIGN_CENTER | wxALL, 0);
        sizer_thumbnail->Add(sync_text, FromDIP(0), wxALIGN_CENTER | wxALL, FromDIP(4));
    }
    sizer_thumbnail->Add(image_button, 0, wxALIGN_CENTER, 0);
    return sizer_thumbnail;
}

void SyncAmsInfoDialog::update_when_change_plate(int idx) {
    if (idx < 0) {
        return;
    }
    m_specify_plate_idx = idx;

    reset_and_sync_ams_list();
    m_cur_input_thumbnail_data = m_plater->get_partplate_list().get_plate(m_specify_plate_idx)->thumbnail_data;
    clone_thumbnail_data();
    set_default_normal(m_cur_input_thumbnail_data);

    wxCommandEvent empty;
    on_selection_changed(empty);

    update_swipe_button_state();
    Layout();
    Fit();
}

void SyncAmsInfoDialog::update_when_change_map_mode(int idx)
{
    m_map_mode = (MapModeEnum) idx;
    if (m_map_mode == MapModeEnum::ColorMap) {
        m_are_you_sure_title->SetLabel(_L("Are you sure to synchronize the filaments?"));
        show_color_panel(true);
    } else if (m_map_mode == MapModeEnum::Override) {
        m_are_you_sure_title->SetLabel(_L("Are you sure to directly override current filaments?"));
        show_color_panel(false);
    }
}

void SyncAmsInfoDialog::update_when_change_map_mode(wxCommandEvent &e)
{
    int win_id = e.GetId();
    auto mode = PageType(win_id);
    update_panel_status(mode);
    update_when_change_map_mode(mode);
}

void SyncAmsInfoDialog::update_panel_status(PageType page)
{
    std::vector<CapsuleButton *> button_list = {m_colormap_btn, m_override_btn};
    for (auto p : button_list) {
        if (p && p->IsSelected()) {
            p->Select(false);
        }
    }
    for (size_t i = 0; i < button_list.size(); i++) {
        if (i == int(page)) {
            button_list[i]->Select(true);
            break;
        }
    }
}

void SyncAmsInfoDialog::show_color_panel(bool flag, bool update_layout)
{
    //show_sizer(m_plate_combox_sizer, flag);
    if (m_sizer_line) {
        show_sizer(m_sizer_line, flag);
    }
    m_two_thumbnail_panel->Show(flag);

    m_filament_panel->Show(flag);  // empty_project

    m_attention_text->Show(flag);
    m_tip_text->Show(flag);
    m_more_setting_tips->Show(flag);

    if (!flag) {
        show_sizer(m_append_color_sizer, false);
        show_sizer(m_merge_color_sizer, false);
    }
    else {
        update_more_setting();
    }
    m_confirm_title->Show(flag);
    if (flag) {
        auto extruders = wxGetApp().plater()->get_partplate_list().get_plate(m_specify_plate_idx)->get_extruders();
        /*if (wxGetApp().plater()->get_extruders_colors().size() != extruders.size()) {
            m_used_colors_tip_text->Show();
        }*/
    } else {
        //m_used_colors_tip_text->Hide();
    }
    if (update_layout){
        Layout();
        Fit();
    }
}

void SyncAmsInfoDialog::update_more_setting(bool layout)
{
    show_sizer(m_append_color_sizer, m_expand_more_settings);
    show_sizer(m_merge_color_sizer, m_expand_more_settings);
    if (layout) {
        Layout();
        Fit();
    }
}

void SyncAmsInfoDialog::init_bitmaps()
{
    m_swipe_left_bmp_normal  = ScalableBitmap(this, "previous_item", m_bmp_pix_cont);
    m_swipe_left_bmp_hover   = ScalableBitmap(this, "previous_item_hover", m_bmp_pix_cont);
    m_swipe_left_bmp_disable  = ScalableBitmap(this, "previous_item_disable", m_bmp_pix_cont);
    m_swipe_right_bmp_normal = ScalableBitmap(this, "next_item", m_bmp_pix_cont);
    m_swipe_right_bmp_hover  = ScalableBitmap(this, "next_item_hover", m_bmp_pix_cont);
    m_swipe_right_bmp_disable  = ScalableBitmap(this, "next_item_disable", m_bmp_pix_cont);
}


void SyncAmsInfoDialog::add_two_image_control()
{// thumbnail
    m_two_thumbnail_panel = new StaticBox(this);
    // m_two_thumbnail_panel->SetBackgroundColour(wxColour(0xF8F8F8));
    m_two_thumbnail_panel->SetBorderWidth(0);
    //m_two_thumbnail_panel->SetMinSize(wxSize(FromDIP(637), -1));
    //m_two_thumbnail_panel->SetMaxSize(wxSize(FromDIP(637), -1));
    m_two_thumbnail_panel_sizer = new wxBoxSizer(wxVERTICAL);

    auto view_two_thumbnail_sizer = new wxBoxSizer(wxHORIZONTAL);
    view_two_thumbnail_sizer->AddSpacer(FromDIP(60));
    auto swipe_left__sizer = new wxBoxSizer(wxVERTICAL);
    swipe_left__sizer->AddStretchSpacer();
    init_bitmaps();
    m_swipe_left_button = new ScalableButton(m_two_thumbnail_panel, wxID_ANY, "previous_item", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, true,
                                             m_bmp_pix_cont);
    m_swipe_left_button->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) {
        if (!m_swipe_left_button_enable) { return; }
        m_swipe_left_button->SetBitmap(m_swipe_left_bmp_hover.bmp());
    });
    m_swipe_left_button->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) {
        if (!m_swipe_left_button_enable) { return; }
        m_swipe_left_button->SetBitmap(m_swipe_left_bmp_normal.bmp());
    });
    m_swipe_left_button->Bind(wxEVT_BUTTON, &SyncAmsInfoDialog::to_previous_plate, this);
    swipe_left__sizer->Add(m_swipe_left_button, 0, wxALIGN_CENTER | wxEXPAND | wxALIGN_CENTER_VERTICAL);
    swipe_left__sizer->AddStretchSpacer();
    view_two_thumbnail_sizer->Add(swipe_left__sizer, 0, wxEXPAND);
    view_two_thumbnail_sizer->AddSpacer(FromDIP(20));
    {
        m_two_image_panel = new StaticBox(m_two_thumbnail_panel);
        // m_two_thumbnail_panel->SetBackgroundColour(wxColour(0xF8F8F8));
        m_two_image_panel->SetBorderWidth(0);
        //m_two_image_panel->SetForegroundColour(wxColour(248, 248, 248, 100));
        m_two_image_panel->SetBackgroundColor(wxColour(248, 248, 248, 100));
        m_two_image_panel_sizer = new wxBoxSizer(wxHORIZONTAL);
        m_left_image_button     = new wxButton(m_two_image_panel, wxID_ANY, {}, wxDefaultPosition, wxSize(FromDIP(LEFT_THUMBNAIL_SIZE_WIDTH), FromDIP(LEFT_THUMBNAIL_SIZE_WIDTH)),
                                           wxBORDER_NONE | wxBU_AUTODRAW);
        m_left_sizer_thumbnail = create_sizer_thumbnail(m_left_image_button, true);
        m_two_image_panel_sizer->Add(m_left_sizer_thumbnail, FromDIP(0), wxALIGN_LEFT | wxEXPAND | wxLEFT | wxTOP | wxBOTTOM, FromDIP(8));
        m_two_image_panel_sizer->AddSpacer(FromDIP(5));

        m_right_image_button = new wxButton(m_two_image_panel, wxID_ANY, {}, wxDefaultPosition,
                                            wxSize(FromDIP(RIGHT_THUMBNAIL_SIZE_WIDTH), FromDIP(RIGHT_THUMBNAIL_SIZE_WIDTH)),
                                            wxBORDER_NONE | wxBU_AUTODRAW);
        m_right_sizer_thumbnail = create_sizer_thumbnail(m_right_image_button, false);
        m_two_image_panel_sizer->Add(m_right_sizer_thumbnail, FromDIP(0), wxALIGN_LEFT | wxEXPAND | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(8));
        m_two_image_panel->SetSizer(m_two_image_panel_sizer);
        m_two_image_panel->Layout();
        m_two_image_panel->Fit();

        view_two_thumbnail_sizer->Add(m_two_image_panel, FromDIP(0), wxALIGN_LEFT | wxEXPAND | wxTOP, FromDIP(2));
    }
    view_two_thumbnail_sizer->AddSpacer(FromDIP(20));
    auto swipe_right__sizer = new wxBoxSizer(wxVERTICAL);
    swipe_right__sizer->AddStretchSpacer();
    m_swipe_right_button    = new ScalableButton(m_two_thumbnail_panel, wxID_ANY, "next_item", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, true,
                                              m_bmp_pix_cont);
    m_swipe_right_button->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) {
        if (!m_swipe_right_button_enable) { return; }
        m_swipe_right_button->SetBitmap(m_swipe_right_bmp_hover.bmp());
    });
    m_swipe_right_button->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) {
        if (!m_swipe_right_button_enable) { return; }
        m_swipe_right_button->SetBitmap(m_swipe_right_bmp_normal.bmp());
    });
    m_swipe_right_button->Bind(wxEVT_BUTTON, &SyncAmsInfoDialog::to_next_plate, this);

    swipe_right__sizer->Add(m_swipe_right_button, 0, wxALIGN_CENTER | wxEXPAND | wxALIGN_CENTER_VERTICAL);
    swipe_right__sizer->AddStretchSpacer();
    view_two_thumbnail_sizer->Add(swipe_right__sizer, 0, wxEXPAND);
    view_two_thumbnail_sizer->AddSpacer(FromDIP(60));
    m_two_thumbnail_panel_sizer->Add(view_two_thumbnail_sizer, 0, wxEXPAND | wxTOP, FromDIP(5));

    m_choose_plate_sizer         = new wxBoxSizer(wxHORIZONTAL);
    m_choose_plate_sizer->AddStretchSpacer();

    wxStaticText *chose_combox_title = new wxStaticText(m_two_thumbnail_panel, wxID_ANY, _CTX(L_CONTEXT("Plate", "Sync_AMS"), "Sync_AMS"));
    m_choose_plate_sizer->Add(chose_combox_title, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxEXPAND | wxTOP, FromDIP(6));
    m_choose_plate_sizer->AddSpacer(FromDIP(10));

    m_combobox_plate = new ComboBox(m_two_thumbnail_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(50), -1), 0, NULL, wxCB_READONLY);
    for (size_t i = 0; i < m_plate_number_choices_str.size(); i++) {
        m_combobox_plate->Append(m_plate_number_choices_str[i]);
    }
    auto iter = std::find(m_plate_choices.begin(), m_plate_choices.end(), m_specify_plate_idx);
    if (iter != m_plate_choices.end()) {
        auto index = iter - m_plate_choices.begin();
        m_combobox_plate->SetSelection(index);
    }
    m_combobox_plate->Bind(wxEVT_COMBOBOX, [this](auto &e) {
        if (e.GetSelection() < m_plate_choices.size()) {
            update_when_change_plate(m_plate_choices[e.GetSelection()]);
        }
    });
    m_choose_plate_sizer->Add(m_combobox_plate, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(3));
    m_choose_plate_sizer->AddStretchSpacer();
    m_two_thumbnail_panel_sizer->Add(m_choose_plate_sizer, 0, wxEXPAND |wxTOP, FromDIP(4));

    m_two_thumbnail_panel->SetSizer(m_two_thumbnail_panel_sizer);
    m_two_thumbnail_panel->Layout();
    m_two_thumbnail_panel->Fit();
    m_sizer_main->Add(m_two_thumbnail_panel, FromDIP(0), wxALIGN_CENTER | wxEXPAND | wxLEFT | wxRIGHT, FromDIP(25));

    update_swipe_button_state();
}

void SyncAmsInfoDialog::to_next_plate(wxCommandEvent &event) {
    auto cobox_idx  = m_combobox_plate->GetSelection();
    cobox_idx++;
    if (cobox_idx >= m_combobox_plate->GetCount()) {
        return;
    }
    m_combobox_plate->SetSelection(cobox_idx);
    update_when_change_plate(m_plate_choices[cobox_idx]);
}

void SyncAmsInfoDialog::to_previous_plate(wxCommandEvent &event) {
    auto cobox_idx = m_combobox_plate->GetSelection();
    cobox_idx--;
    if (cobox_idx < 0) {
        return;
    }
    m_combobox_plate->SetSelection(cobox_idx);
    update_when_change_plate(m_plate_choices[cobox_idx]);
}

void SyncAmsInfoDialog::update_swipe_button_state()
{
    m_swipe_left_button_enable = true;
    m_swipe_left_button->Enable();
    m_swipe_left_button->SetBitmap(m_swipe_left_bmp_normal.bmp());
    m_swipe_right_button_enable = true;
    m_swipe_right_button->Enable();
    m_swipe_right_button->SetBitmap(m_swipe_right_bmp_normal.bmp());
    if (m_combobox_plate->GetSelection() == 0) { // auto plate_index = m_plate_choices[m_combobox_plate->GetSelection()];
        m_swipe_left_button->SetBitmap(m_swipe_left_bmp_disable.bmp());
        m_swipe_left_button_enable = false;
    }
    if (m_combobox_plate->GetSelection() == m_combobox_plate->GetCount() - 1) {
        m_swipe_right_button->SetBitmap(m_swipe_right_bmp_disable.bmp());
        m_swipe_right_button_enable = false;
    }
}

void SyncAmsInfoDialog::updata_ui_when_priner_not_same() {
    show_color_panel(false);
    m_are_you_sure_title->Show(false);
    if (m_mode_combox_sizer)
        m_mode_combox_sizer->Show(false);

    m_button_cancel->Hide();
    m_button_ok->Show();
    m_button_ok->SetLabel(_L("OK"));
    m_confirm_title->Show();
    m_confirm_title->SetLabel(_L("The connected printer does not match the currently selected printer. Please change the selected printer."));
    SetMinSize(wxSize(FromDIP(800), -1));
    SetMaxSize(wxSize(FromDIP(800), -1));
    Layout();
    Fit();
}

SyncAmsInfoDialog::SyncAmsInfoDialog(wxWindow *parent, SyncInfo &info) :
    DPIDialog(static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY, _L("Synchronize AMS Filament Information"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
    , m_input_info(info)
    , m_export_3mf_cancel(false)
    , m_mapping_popup(AmsMapingPopup(this))
    , m_mapping_tip_popup(AmsMapingTipPopup(this))
    , m_mapping_tutorial_popup(AmsTutorialPopup(this))
{
    m_plater = wxGetApp().plater();
    SelectMachineDialog::init_machine_bed_types();
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    ops_auto.push_back(POItem{"auto", "Auto"});
    ops_auto.push_back(POItem{"on", "On"});
    ops_auto.push_back(POItem{"off", "Off"});

    ops_no_auto.push_back(POItem{"on", "On"});
    ops_no_auto.push_back(POItem{"off", "Off"});

    SetMinSize(wxSize(SyncAmsInfoDialogWidth, -1));
    SetMaxSize(wxSize(SyncAmsInfoDialogWidth, -1));

    // bind
    Bind(wxEVT_CLOSE_WINDOW, &SyncAmsInfoDialog::on_cancel, this);

    for (int i = 0; i < BED_TYPE_COUNT; i++) { m_bedtype_list.push_back(SelectMachineDialog::MACHINE_BED_TYPE_STRING[i]); }

    // font
    SetFont(wxGetApp().normal_font());

    // icon
    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    Freeze();
    SetBackgroundColour(m_colour_def_color);

    m_sizer_main = new wxBoxSizer(wxVERTICAL);
    m_line_top   = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    m_line_top->SetBackgroundColour(wxColour(166, 169, 170));
    m_sizer_main->Add(m_line_top, 0, wxEXPAND, 0);
    //m_sizer_main->Add(0, 0, 0, wxTOP, FromDIP(11));
    auto &bSizer = m_sizer_main;
        { // content
        GUI::PartPlateList &plate_list             = wxGetApp().plater()->get_partplate_list();
        GUI::PartPlate *    curr_plate             = GUI::wxGetApp().plater()->get_partplate_list().get_selected_plate();
        m_is_empty_project                         = true;
        m_plate_number_choices_str.clear();
        for (size_t i = 0; i < plate_list.get_plate_count(); i++) {
            auto temp_plate = GUI::wxGetApp().plater()->get_partplate_list().get_plate(i);
            if (!temp_plate->get_objects_on_this_plate().empty()) {
                if (m_is_empty_project) {
                    m_is_empty_project     = false;
                }
                m_plate_number_choices_str.Add(i < 10 ? ("0" + std::to_wstring(i + 1)) : std::to_wstring(i));
                m_plate_choices.emplace_back(i);
            }
        }
        if (m_is_empty_project == false) {
            //use map mode
            m_mode_combox_sizer = new wxBoxSizer(wxHORIZONTAL);
            m_colormap_btn      = new CapsuleButton(this, PageType::ptColorMap, _L("Mapping"), true);
            m_override_btn      = new CapsuleButton(this, PageType::ptOverride, _L("Overwriting"), false);
            m_mode_combox_sizer->AddSpacer(FromDIP(25));
            m_mode_combox_sizer->AddStretchSpacer();
            m_mode_combox_sizer->Add(m_colormap_btn, 0, wxALIGN_CENTER | wxEXPAND | wxALL, FromDIP(2));
            m_mode_combox_sizer->AddSpacer(FromDIP(8));
            m_mode_combox_sizer->Add(m_override_btn, 0, wxALIGN_CENTER | wxEXPAND | wxALL, FromDIP(2));
            m_mode_combox_sizer->AddStretchSpacer();

            m_colormap_btn->Bind(wxEVT_BUTTON, &SyncAmsInfoDialog::update_when_change_map_mode,this); // update_when_change_map_mode(e.GetSelection());
            m_override_btn->Bind(wxEVT_BUTTON, &SyncAmsInfoDialog::update_when_change_map_mode,this);

            bSizer->Add(m_mode_combox_sizer, FromDIP(0), wxEXPAND | wxALIGN_LEFT | wxTOP, FromDIP(10));
            m_specify_plate_idx = GUI::wxGetApp().plater()->get_partplate_list().get_curr_plate_index();

            bool not_exist = std::find(m_plate_choices.begin(), m_plate_choices.end(), m_specify_plate_idx) == m_plate_choices.end();
            if (not_exist) {
                m_specify_plate_idx = m_plate_choices[0];
            }
        }
    }

    m_basic_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_basic_panel->SetBackgroundColour(*wxWHITE);
    m_basicl_sizer = new wxBoxSizer(wxHORIZONTAL);

    /*basic info right*/
    sizer_basic_right_info = new wxBoxSizer(wxVERTICAL);
    /*rename*/
    auto sizer_rename = new wxBoxSizer(wxHORIZONTAL);
    m_rename_switch_panel = new wxSimplebook(m_basic_panel);
    m_rename_switch_panel->SetBackgroundColour(*wxWHITE);
    m_rename_switch_panel->SetSize(wxSize(FromDIP(360), FromDIP(25)));
    m_rename_switch_panel->SetMinSize(wxSize(FromDIP(360), FromDIP(25)));
    m_rename_switch_panel->SetMaxSize(wxSize(FromDIP(360), FromDIP(25)));

    m_rename_normal_panel = new wxPanel(m_rename_switch_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_rename_normal_panel->SetBackgroundColour(*wxWHITE);
    rename_sizer_v = new wxBoxSizer(wxVERTICAL);
    rename_sizer_h = new wxBoxSizer(wxHORIZONTAL);

    m_rename_text = new wxStaticText(m_rename_normal_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    m_rename_text->SetFont(::Label::Head_13);
    m_rename_text->SetBackgroundColour(*wxWHITE);
    m_rename_text->SetMaxSize(wxSize(FromDIP(340), -1));
    rename_editable       = new ScalableBitmap(this, "rename_edit", 20);
    rename_editable_light = new ScalableBitmap(this, "rename_edit", 20);
    m_rename_button       = new wxStaticBitmap(m_rename_normal_panel, wxID_ANY, rename_editable->bmp(), wxDefaultPosition, wxSize(FromDIP(20), FromDIP(20)), 0);
    m_rename_button->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_HAND); });
    m_rename_button->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_ARROW); });

    rename_sizer_h->Add(m_rename_text, 0, wxALIGN_CENTER , FromDIP(2));
    rename_sizer_h->Add(0, 0, 0, wxLEFT, FromDIP(3));
    rename_sizer_h->Add(m_rename_button, 0, wxALIGN_CENTER, 0);
    rename_sizer_v->Add(rename_sizer_h, 1, wxTOP, 0);
    m_rename_normal_panel->SetSizer(rename_sizer_v);
    m_rename_normal_panel->SetMaxSize(wxSize(-1, FromDIP(0)));
    rename_sizer_v->Fit(m_rename_normal_panel);

    m_rename_edit_panel = new wxPanel(m_rename_switch_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_rename_edit_panel->SetBackgroundColour(*wxWHITE);
    auto rename_edit_sizer_v = new wxBoxSizer(wxVERTICAL);

    m_rename_input = new ::TextInput(m_rename_edit_panel, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_rename_input->GetTextCtrl()->SetFont(::Label::Body_13);
    m_rename_input->SetSize(wxSize(FromDIP(360), FromDIP(24)));
    m_rename_input->SetMinSize(wxSize(FromDIP(360), FromDIP(24)));
    m_rename_input->SetMaxSize(wxSize(FromDIP(360), FromDIP(24)));
    m_rename_input->Bind(wxEVT_TEXT_ENTER, [this](auto &e) { on_rename_enter(); });
    m_rename_input->Bind(wxEVT_KILL_FOCUS, [this](auto &e) {
        if (!m_rename_input->HasFocus() && !m_rename_text->HasFocus())
            on_rename_enter();
        else
            e.Skip();
    });
    rename_edit_sizer_v->Add(m_rename_input, 1, wxALIGN_CENTER, 0);

    m_rename_edit_panel->SetSizer(rename_edit_sizer_v);
    m_rename_edit_panel->Layout();
    rename_edit_sizer_v->Fit(m_rename_edit_panel);

    m_rename_button->Bind(wxEVT_LEFT_DOWN, &SyncAmsInfoDialog::on_rename_click, this);
    m_rename_switch_panel->AddPage(m_rename_normal_panel, wxEmptyString, true);
    m_rename_switch_panel->AddPage(m_rename_edit_panel, wxEmptyString, false);

    /*last & next page*/
    sizer_rename->Add(m_rename_switch_panel, 0, wxALIGN_CENTER, 0);
    sizer_rename->Add(0, 0, 0, wxEXPAND, 0);


    /*printer combobox*/
    /*wxBoxSizer *sizer_split_printer = new wxBoxSizer(wxHORIZONTAL);
    m_stext_printer_title           = new Label(m_basic_panel, _L("Printer"));
    m_stext_printer_title->SetFont(::Label::Body_14);
    m_stext_printer_title->SetForegroundColour(0x909090);
    auto m_split_line = new wxPanel(m_basic_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_split_line->SetBackgroundColour(0xeeeeee);
    m_split_line->SetMinSize(wxSize(-1, 1));
    m_split_line->SetMaxSize(wxSize(-1, 1));
    sizer_split_printer->Add(0, 0, 0, wxEXPAND, 0);
    sizer_split_printer->Add(m_stext_printer_title, 0, wxALIGN_CENTER, 0);
    sizer_split_printer->Add(m_split_line, 1, wxALIGN_CENTER_VERTICAL, 0);*/

    wxBoxSizer *sizer_printer_area      = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *sizer_bed_staticbox     = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *sizer_printer_staticbox = new wxBoxSizer(wxHORIZONTAL);

    /*printer area*/
    auto printer_staticbox = new StaticBox(m_basic_panel);
    printer_staticbox->SetMinSize(wxSize(FromDIP(338), FromDIP(68)));
    printer_staticbox->SetMaxSize(wxSize(FromDIP(338), FromDIP(68)));
    printer_staticbox->SetBorderColor(wxColour(0xCECECE));

    m_printer_image = new wxStaticBitmap(printer_staticbox, wxID_ANY, create_scaled_bitmap("printer_preview_BL-P001", this, 52));
    m_printer_image->SetMinSize(wxSize(FromDIP(52), FromDIP(52)));
    m_printer_image->SetMaxSize(wxSize(FromDIP(52), FromDIP(52)));

    m_comboBox_printer = new ComboBox(printer_staticbox, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
    m_comboBox_printer->SetBorderWidth(0);
    m_comboBox_printer->SetMinSize(wxSize(FromDIP(250), FromDIP(60)));
    m_comboBox_printer->SetMaxSize(wxSize(FromDIP(250), FromDIP(60)));
    m_comboBox_printer->SetBackgroundColor(*wxWHITE);
    m_comboBox_printer->Bind(wxEVT_COMBOBOX, &SyncAmsInfoDialog::on_selection_changed, this);

    m_btn_bg_enable = StateColor(std::pair<wxColour, int>(wxColour(27, 136, 68), StateColor::Pressed), std::pair<wxColour, int>(wxColour(61, 203, 115), StateColor::Hovered),
                                 std::pair<wxColour, int>(wxColour(0, 174, 66), StateColor::Normal));

    m_button_refresh = new ScalableButton(printer_staticbox, wxID_ANY, "refresh_printer", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, true);
    m_button_refresh->Bind(wxEVT_BUTTON, &SyncAmsInfoDialog::on_refresh, this);

    sizer_printer_staticbox->Add(0, 0, 0, wxLEFT, FromDIP(7));
    sizer_printer_staticbox->Add(m_printer_image, 0, wxALIGN_CENTER, 0);
    sizer_printer_staticbox->Add(m_comboBox_printer, 0, wxALIGN_CENTER, 0);
    sizer_printer_staticbox->Add(m_button_refresh, 0, wxALIGN_CENTER, 0);

    printer_staticbox->SetSizer(sizer_printer_staticbox);
    printer_staticbox->Layout();
    printer_staticbox->Fit();

    /*bed area*/
    auto bed_staticbox = new StaticBox(m_basic_panel);
    bed_staticbox->SetMinSize(wxSize(FromDIP(98), FromDIP(68)));
    bed_staticbox->SetMaxSize(wxSize(FromDIP(98), FromDIP(68)));
    bed_staticbox->SetBorderColor(wxColour(0xCECECE));

    m_bed_image = new wxStaticBitmap(bed_staticbox, wxID_ANY, create_scaled_bitmap("bed_cool", this, 32));
    m_bed_image->SetBackgroundColour(*wxWHITE);
    m_bed_image->SetMinSize(wxSize(FromDIP(32), FromDIP(32)));
    m_bed_image->SetMaxSize(wxSize(FromDIP(32), FromDIP(32)));

    m_text_bed_type = new Label(bed_staticbox);
    m_text_bed_type->SetForegroundColour(0xCECECE);
    m_text_bed_type->SetMaxSize(wxSize(FromDIP(80), FromDIP(24)));

    sizer_bed_staticbox->Add(m_bed_image, 0, wxALIGN_CENTER, 0);
    sizer_bed_staticbox->Add(m_text_bed_type, 0, wxALIGN_CENTER, 0);

    bed_staticbox->SetSizer(sizer_bed_staticbox);
    bed_staticbox->Layout();
    bed_staticbox->Fit();

    sizer_printer_area->Add(printer_staticbox, 0, wxALIGN_CENTER, 0);
    sizer_printer_area->Add(0, 0, 0, wxLEFT, FromDIP(4));
    sizer_printer_area->Add(bed_staticbox, 0, wxALIGN_CENTER, 0);

    m_text_printer_msg = new Label(m_basic_panel);
    m_text_printer_msg->SetMinSize(wxSize(FromDIP(420), FromDIP(24)));
    m_text_printer_msg->SetMaxSize(wxSize(FromDIP(420), FromDIP(24)));
    m_text_printer_msg->SetFont(::Label::Body_13);
    m_text_printer_msg->Hide();

    sizer_basic_right_info->Add(sizer_rename, 0, wxTOP, 0);
    sizer_basic_right_info->Add(m_text_bed_type, 0, wxTOP, 0);
    //sizer_basic_right_info->Add(sizer_split_printer, 1, wxEXPAND, 0);
    sizer_basic_right_info->Add(sizer_printer_area, 0, wxTOP, 0);
    sizer_basic_right_info->Add(m_text_printer_msg, 0, wxLEFT, 0);

    //m_basicl_sizer->Add(m_sizer_thumbnail_area, 0, wxLEFT, 0);
    //m_basicl_sizer->Add(0, 0, 0, wxLEFT, FromDIP(8));
    m_basicl_sizer->Add(sizer_basic_right_info, 0, wxLEFT, 0);

    m_basic_panel->SetSizer(m_basicl_sizer);
    m_basic_panel->Layout();

    /*filaments info*/
    /*sizer_split_filament = new wxBoxSizer(wxHORIZONTAL);

    auto m_stext_filament_title = new Label(this, _L("Filament"));
    m_stext_filament_title->SetFont(::Label::Body_14);
    m_stext_filament_title->SetForegroundColour(0x909090);

    auto m_split_line_filament = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    m_split_line_filament->SetBackgroundColour(0xeeeeee);
    m_split_line_filament->SetMinSize(wxSize(-1, 1));
    m_split_line_filament->SetMaxSize(wxSize(-1, 1));*/

    /*m_sizer_autorefill = new wxBoxSizer(wxHORIZONTAL);
    m_ams_backup_tip   = new Label(this, _L("Auto Refill"));
    m_ams_backup_tip->SetFont(::Label::Head_12);
    m_ams_backup_tip->SetForegroundColour(wxColour(0x00AE42));
    m_ams_backup_tip->SetBackgroundColour(*wxWHITE);
    img_ams_backup = new wxStaticBitmap(this, wxID_ANY, create_scaled_bitmap("automatic_material_renewal", this, 16), wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)), 0);
    img_ams_backup->SetBackgroundColour(*wxWHITE);

    m_sizer_autorefill->Add(0, 0, 1, wxEXPAND, 0);
    m_sizer_autorefill->Add(img_ams_backup, 0, wxALL, FromDIP(3));
    m_sizer_autorefill->Add(m_ams_backup_tip, 0, wxTOP, FromDIP(5));

    m_ams_backup_tip->Hide();
    img_ams_backup->Hide();

    m_ams_backup_tip->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_HAND); });
    img_ams_backup->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_HAND); });

    m_ams_backup_tip->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_ARROW); });
    img_ams_backup->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_ARROW); });

    m_ams_backup_tip->Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        if (!m_is_in_sending_mode) {
            popup_filament_backup();
            on_rename_enter();
        }
    });
    img_ams_backup->Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        if (!m_is_in_sending_mode) popup_filament_backup();
        on_rename_enter();
    });*/

    //sizer_split_filament->Add(0, 0, 0, wxEXPAND, 0);
    //sizer_split_filament->Add(m_stext_filament_title, 0, wxALIGN_CENTER, 0);
    //sizer_split_filament->Add(m_split_line_filament, 1, wxALIGN_CENTER_VERTICAL, 0);
    //sizer_split_filament->Add(m_sizer_autorefill, 0, wxALIGN_CENTER, 0);

    /*filament area*/
    /*1 extruder*/
    m_filament_panel = new StaticBox(this);
    //m_filament_panel->SetBackgroundColour(wxColour(0xF8F8F8));
    m_filament_panel->SetBorderWidth(0);
    m_filament_panel->SetMinSize(wxSize(FromDIP(637), -1));
    m_filament_panel->SetMaxSize(wxSize(FromDIP(637), -1));
    m_filament_panel_sizer = new wxBoxSizer(wxVERTICAL);

    m_sizer_ams_mapping = new wxFlexGridSizer(0, SYNC_FLEX_GRID_COL, FromDIP(6), FromDIP(7));
    m_filament_panel_sizer->Add(m_sizer_ams_mapping, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
    m_filament_panel->SetSizer(m_filament_panel_sizer);
    m_filament_panel->Layout();
    m_filament_panel->Fit();

    /*left & right extruder*/
    m_sizer_filament_2extruder = new wxBoxSizer(wxHORIZONTAL);

    m_filament_left_panel = new StaticBox(this);
    m_filament_left_panel->SetBackgroundColour(wxColour(0xF8F8F8));
    m_filament_left_panel->SetBorderWidth(0);
    m_filament_left_panel->SetMinSize(wxSize(FromDIP(315), -1));
    m_filament_left_panel->SetMaxSize(wxSize(FromDIP(315), -1));

    m_filament_panel_left_sizer     = new wxBoxSizer(wxVERTICAL);
    auto left_recommend_title_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto left_recommend_title1      = new Label(m_filament_left_panel, _L("Left Extruder"));
    left_recommend_title1->SetFont(::Label::Head_13);
    left_recommend_title1->SetBackgroundColour(wxColour(0xF8F8F8));
    auto left_recommend_title2 = new Label(m_filament_left_panel, _L("(Recommended filament)"));
    left_recommend_title2->SetFont(::Label::Body_13);
    left_recommend_title2->SetForegroundColour(wxColour(0x6B6B6B));
    left_recommend_title2->SetBackgroundColour(wxColour(0xF8F8F8));
    left_recommend_title_sizer->Add(left_recommend_title1, 0, wxALIGN_CENTER, 0);
    left_recommend_title_sizer->Add(0, 0, 0, wxLEFT, FromDIP(4));
    left_recommend_title_sizer->Add(left_recommend_title2, 0, wxALIGN_CENTER, 0);

    m_sizer_ams_mapping_left = new wxGridSizer(0, 5, FromDIP(7), FromDIP(7));
    m_filament_panel_left_sizer->Add(left_recommend_title_sizer, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    m_filament_panel_left_sizer->Add(m_sizer_ams_mapping_left, 0, wxEXPAND | wxALL, FromDIP(10));
    m_filament_left_panel->SetSizer(m_filament_panel_left_sizer);
    m_filament_left_panel->Layout();

    m_filament_right_panel = new StaticBox(this);
    m_filament_right_panel->SetBorderWidth(0);
    m_filament_right_panel->SetBackgroundColour(wxColour(0xf8f8f8));
    m_filament_right_panel->SetMinSize(wxSize(FromDIP(315), -1));
    m_filament_right_panel->SetMaxSize(wxSize(FromDIP(315), -1));

    m_filament_panel_right_sizer     = new wxBoxSizer(wxVERTICAL);
    auto right_recommend_title_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto right_recommend_title1      = new Label(m_filament_right_panel, _L("Right Extruder"));
    right_recommend_title1->SetFont(::Label::Head_13);
    right_recommend_title1->SetBackgroundColour(wxColour(0xF8F8F8));

    auto right_recommend_title2 = new Label(m_filament_right_panel, _L("(Recommended filament)"));
    right_recommend_title2->SetFont(::Label::Body_13);
    right_recommend_title2->SetForegroundColour(wxColour(0x6B6B6B));
    right_recommend_title2->SetBackgroundColour(wxColour(0xF8F8F8));
    right_recommend_title_sizer->Add(right_recommend_title1, 0, wxALIGN_CENTER, 0);
    right_recommend_title_sizer->Add(0, 0, 0, wxLEFT, FromDIP(4));
    right_recommend_title_sizer->Add(right_recommend_title2, 0, wxALIGN_CENTER, 0);

    m_sizer_ams_mapping_right = new wxGridSizer(0, 5, FromDIP(7), FromDIP(7));
    m_filament_panel_right_sizer->Add(right_recommend_title_sizer, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    m_filament_panel_right_sizer->Add(m_sizer_ams_mapping_right, 0, wxEXPAND | wxALL, FromDIP(10));
    m_filament_right_panel->SetSizer(m_filament_panel_right_sizer);
    m_filament_right_panel->Layout();

    m_sizer_filament_2extruder->Add(m_filament_left_panel, 0, wxEXPAND, 0);
    m_sizer_filament_2extruder->Add(0, 0, 1, wxEXPAND, 0);
    m_sizer_filament_2extruder->Add(m_filament_right_panel, 0, wxEXPAND, 0);
    m_sizer_filament_2extruder->Layout();

    m_filament_left_panel->Hide();//init
    m_filament_right_panel->Hide();
    m_filament_panel->Hide();//init

    sizer_advanced_options_title = new wxBoxSizer(wxHORIZONTAL);
    auto        advanced_options_title       = new Label(this, _L("Advanced Options"));
    advanced_options_title->SetFont(::Label::Body_13);
    advanced_options_title->SetForegroundColour(wxColour(38, 46, 48));

    m_advanced_options_icon = new wxStaticBitmap(this, wxID_ANY, create_scaled_bitmap("advanced_option1", this, 18), wxDefaultPosition, wxSize(FromDIP(18), FromDIP(18)));

    sizer_advanced_options_title->Add(0, 0, 1, wxEXPAND, 0);
    sizer_advanced_options_title->Add(advanced_options_title, 0, wxALIGN_CENTER, 0);
    sizer_advanced_options_title->Add(m_advanced_options_icon, 0, wxALIGN_CENTER, 0);

    advanced_options_title->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_HAND); });
    advanced_options_title->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_ARROW); });
    advanced_options_title->Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        if (m_options_other->IsShown()) {
            m_options_other->Hide();
            m_advanced_options_icon->SetBitmap(create_scaled_bitmap("advanced_option1", this, 18));
        } else {
            m_options_other->Show();
            m_advanced_options_icon->SetBitmap(create_scaled_bitmap("advanced_option2", this, 18));
        }
        Layout();
        Fit();
    });

    m_options_other           = new wxPanel(this);
    m_sizer_options_timelapse = new wxBoxSizer(wxVERTICAL);
    m_sizer_options_other     = new wxBoxSizer(wxVERTICAL);

    auto option_timelapse = new PrintOption(this, _L("Timelapse"), wxEmptyString, ops_no_auto, "timelapse");

    auto option_auto_bed_level =
        new PrintOption(m_options_other, _L("Auto Bed Leveling"),
                        _L("Check heatbed flatness. Leveling makes extruded height uniform.\n*Automatic mode: Level first (about 10 seconds). Skip if surface is fine."),
                        ops_auto, "bed_leveling");

    auto option_flow_dynamics_cali =
        new PrintOption(m_options_other, _L("Flow Dynamics Calibration"),
                        _L("Find the best coefficient for dynamic flow calibration to enhance print quality.\n*Automatic mode: Skip if the filament was calibrated recently."),
                        ops_auto, "flow_cali");

    auto option_nozzle_offset_cali_cali =
        new PrintOption(m_options_other, _L("Nozzle Offset Calibration"),
                        _L("Calibrate nozzle offsets to enhance print quality.\n*Automatic mode: Check for calibration before printing; skip if unnecessary."), ops_auto);

    auto option_use_ams = new PrintOption(m_options_other, _L("Use AMS"), wxEmptyString, ops_no_auto);

    option_use_ams->Bind(EVT_SWITCH_PRINT_OPTION, [this](auto &e) {
        m_ams_mapping_result.clear();
        sync_ams_mapping_result(m_ams_mapping_result);
    });

    option_use_ams->setValue("off");
    m_sizer_options_timelapse->Add(option_timelapse, 0, wxEXPAND  | wxBOTTOM, FromDIP(5));
    m_sizer_options_other->Add(option_use_ams, 0, wxEXPAND  | wxBOTTOM, FromDIP(5));
    m_sizer_options_other->Add(option_auto_bed_level, 0, wxEXPAND  | wxBOTTOM, FromDIP(5));
    m_sizer_options_other->Add(option_flow_dynamics_cali, 0, wxEXPAND  | wxBOTTOM, FromDIP(5));
    m_sizer_options_other->Add(option_nozzle_offset_cali_cali, 0, wxEXPAND  | wxBOTTOM, FromDIP(5));

    m_options_other->SetSizer(m_sizer_options_other);
    m_options_other->SetMaxSize(wxSize(-1, FromDIP(0)));
    m_checkbox_list["timelapse"]          = option_timelapse;
    m_checkbox_list["bed_leveling"]       = option_auto_bed_level;
    m_checkbox_list["use_ams"]            = option_use_ams;
    m_checkbox_list["flow_cali"]          = option_flow_dynamics_cali;
    m_checkbox_list["nozzle_offset_cali"] = option_nozzle_offset_cali_cali;

    option_timelapse->Hide();
    option_auto_bed_level->Hide();
    option_flow_dynamics_cali->Hide();
    option_nozzle_offset_cali_cali->Hide();
    option_use_ams->Hide();

    m_simplebook = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, SELECT_MACHINE_DIALOG_SIMBOOK_SIZE, 0);

    // perpare mode
    m_panel_prepare = new wxPanel(m_simplebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_prepare->SetBackgroundColour(m_colour_def_color);
    wxBoxSizer *m_sizer_prepare = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *m_sizer_pcont   = new wxBoxSizer(wxHORIZONTAL);

    /*m_sizer_prepare->Add(0, 0, 1, wxTOP, FromDIP(12));

    auto hyperlink_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_hyperlink          = new wxHyperlinkCtrl(m_panel_prepare, wxID_ANY, _L("Click here if you can't connect to the printer"),
                                      wxT("https://wiki.bambulab.com/en/software/bambu-studio/failed-to-connect-printer"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);

    hyperlink_sizer->Add(m_hyperlink, 0, wxALIGN_CENTER | wxALL, 5);
    m_sizer_prepare->Add(hyperlink_sizer, 0, wxALIGN_CENTER | wxALL, 5);*/

    m_button_ensure = new Button(m_panel_prepare, _L("Send"));
    m_button_ensure->SetBackgroundColor(m_btn_bg_enable);
    m_button_ensure->SetBorderColor(m_btn_bg_enable);
    m_button_ensure->SetTextColor(StateColor::darkModeColorFor("#FFFFFE"));
    m_button_ensure->SetSize(SELECT_MACHINE_DIALOG_BUTTON_SIZE);
    m_button_ensure->SetMinSize(SELECT_MACHINE_DIALOG_BUTTON_SIZE);
    m_button_ensure->SetMinSize(SELECT_MACHINE_DIALOG_BUTTON_SIZE);
    m_button_ensure->SetCornerRadius(FromDIP(5));
    m_button_ensure->Bind(wxEVT_BUTTON, &SyncAmsInfoDialog::on_ok_btn, this);

    m_sizer_pcont->Add(0, 0, 1, wxEXPAND, 0);
    m_sizer_pcont->Add(m_button_ensure, 0, wxRIGHT, 0);

    m_sizer_prepare->Add(m_sizer_pcont, 0, wxEXPAND, 0);
    m_panel_prepare->SetSizer(m_sizer_prepare);
    m_panel_prepare->SetMaxSize(wxSize(-1, FromDIP(0)));
    m_simplebook->AddPage(m_panel_prepare, wxEmptyString, true);

    // sending mode
    m_status_bar    = std::make_shared<BBLStatusBarSend>(m_simplebook);
    m_panel_sending = m_status_bar->get_panel();
    m_simplebook->AddPage(m_panel_sending, wxEmptyString, false);

    // finish mode
    m_panel_finish = new wxPanel(m_simplebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_finish->SetBackgroundColour(wxColour(135, 206, 250));
    wxBoxSizer *m_sizer_finish   = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *m_sizer_finish_v = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *m_sizer_finish_h = new wxBoxSizer(wxHORIZONTAL);

    auto imgsize      = FromDIP(25);
    auto completedimg = new wxStaticBitmap(m_panel_finish, wxID_ANY, create_scaled_bitmap("completed", m_panel_finish, 25), wxDefaultPosition, wxSize(imgsize, imgsize), 0);
    m_sizer_finish_h->Add(completedimg, 0, wxALIGN_CENTER | wxALL, FromDIP(5));

    m_statictext_finish = new wxStaticText(m_panel_finish, wxID_ANY, L("send completed"), wxDefaultPosition, wxDefaultSize, 0);
    m_statictext_finish->Wrap(-1);
    m_statictext_finish->SetForegroundColour(wxColour(0, 174, 66));
    m_sizer_finish_h->Add(m_statictext_finish, 0, wxALIGN_CENTER | wxALL, FromDIP(5));

    m_sizer_finish_v->Add(m_sizer_finish_h, 1, wxALIGN_CENTER, 0);

    m_sizer_finish->Add(m_sizer_finish_v, 1, wxALIGN_CENTER, 0);

    m_panel_finish->SetSizer(m_sizer_finish);
    m_simplebook->AddPage(m_panel_finish, wxEmptyString, false);

    // show bind failed info
    m_sw_print_failed_info = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(380), FromDIP(125)), wxVSCROLL);
    m_sw_print_failed_info->SetBackgroundColour(*wxWHITE);
    m_sw_print_failed_info->SetScrollRate(0, 5);
    m_sw_print_failed_info->SetMinSize(wxSize(FromDIP(380), FromDIP(125)));
    m_sw_print_failed_info->SetMaxSize(wxSize(FromDIP(380), FromDIP(125)));

    wxBoxSizer *sizer_print_failed_info = new wxBoxSizer(wxVERTICAL);
    m_sw_print_failed_info->SetSizer(sizer_print_failed_info);

    wxBoxSizer *sizer_error_code = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *sizer_error_desc = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *sizer_extra_info = new wxBoxSizer(wxHORIZONTAL);

    auto st_title_error_code     = new wxStaticText(m_sw_print_failed_info, wxID_ANY, _L("Error code"));
    auto st_title_error_code_doc = new wxStaticText(m_sw_print_failed_info, wxID_ANY, ": ");
    m_st_txt_error_code          = new Label(m_sw_print_failed_info, wxEmptyString);
    st_title_error_code->SetForegroundColour(0x909090);
    st_title_error_code_doc->SetForegroundColour(0x909090);
    m_st_txt_error_code->SetForegroundColour(0x909090);
    st_title_error_code->SetFont(::Label::Body_13);
    st_title_error_code_doc->SetFont(::Label::Body_13);
    m_st_txt_error_code->SetFont(::Label::Body_13);
    st_title_error_code->SetMinSize(wxSize(FromDIP(74), -1));
    st_title_error_code->SetMaxSize(wxSize(FromDIP(74), -1));
    m_st_txt_error_code->SetMinSize(wxSize(FromDIP(260), -1));
    m_st_txt_error_code->SetMaxSize(wxSize(FromDIP(260), -1));
    sizer_error_code->Add(st_title_error_code, 0, wxALL, 0);
    sizer_error_code->Add(st_title_error_code_doc, 0, wxALL, 0);
    sizer_error_code->Add(m_st_txt_error_code, 0, wxALL, 0);

    auto st_title_error_desc     = new wxStaticText(m_sw_print_failed_info, wxID_ANY, wxT("Error desc"));
    auto st_title_error_desc_doc = new wxStaticText(m_sw_print_failed_info, wxID_ANY, ": ");
    m_st_txt_error_desc          = new Label(m_sw_print_failed_info, wxEmptyString);
    st_title_error_desc->SetForegroundColour(0x909090);
    st_title_error_desc_doc->SetForegroundColour(0x909090);
    m_st_txt_error_desc->SetForegroundColour(0x909090);
    st_title_error_desc->SetFont(::Label::Body_13);
    st_title_error_desc_doc->SetFont(::Label::Body_13);
    m_st_txt_error_desc->SetFont(::Label::Body_13);
    st_title_error_desc->SetMinSize(wxSize(FromDIP(74), -1));
    st_title_error_desc->SetMaxSize(wxSize(FromDIP(74), -1));
    m_st_txt_error_desc->SetMinSize(wxSize(FromDIP(260), -1));
    m_st_txt_error_desc->SetMaxSize(wxSize(FromDIP(260), -1));
    sizer_error_desc->Add(st_title_error_desc, 0, wxALL, 0);
    sizer_error_desc->Add(st_title_error_desc_doc, 0, wxALL, 0);
    sizer_error_desc->Add(m_st_txt_error_desc, 0, wxALL, 0);

    auto st_title_extra_info     = new wxStaticText(m_sw_print_failed_info, wxID_ANY, wxT("Extra info"));
    auto st_title_extra_info_doc = new wxStaticText(m_sw_print_failed_info, wxID_ANY, ": ");
    m_st_txt_extra_info          = new Label(m_sw_print_failed_info, wxEmptyString);
    st_title_extra_info->SetForegroundColour(0x909090);
    st_title_extra_info_doc->SetForegroundColour(0x909090);
    m_st_txt_extra_info->SetForegroundColour(0x909090);
    st_title_extra_info->SetFont(::Label::Body_13);
    st_title_extra_info_doc->SetFont(::Label::Body_13);
    m_st_txt_extra_info->SetFont(::Label::Body_13);
    st_title_extra_info->SetMinSize(wxSize(FromDIP(74), -1));
    st_title_extra_info->SetMaxSize(wxSize(FromDIP(74), -1));
    m_st_txt_extra_info->SetMinSize(wxSize(FromDIP(260), -1));
    m_st_txt_extra_info->SetMaxSize(wxSize(FromDIP(260), -1));
    sizer_extra_info->Add(st_title_extra_info, 0, wxALL, 0);
    sizer_extra_info->Add(st_title_extra_info_doc, 0, wxALL, 0);
    sizer_extra_info->Add(m_st_txt_extra_info, 0, wxALL, 0);

    m_link_network_state = new wxHyperlinkCtrl(m_sw_print_failed_info, wxID_ANY, _L("Check the status of current system services"), "");
    m_link_network_state->SetFont(::Label::Body_12);
    m_link_network_state->Bind(wxEVT_LEFT_DOWN, [this](auto &e) { wxGetApp().link_to_network_check(); });
    m_link_network_state->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { m_link_network_state->SetCursor(wxCURSOR_HAND); });
    m_link_network_state->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { m_link_network_state->SetCursor(wxCURSOR_ARROW); });

    sizer_print_failed_info->Add(m_link_network_state, 0, wxLEFT, 5);
    sizer_print_failed_info->Add(sizer_error_code, 0, wxLEFT, 5);
    sizer_print_failed_info->Add(sizer_error_desc, 0, wxLEFT, 5);
    sizer_print_failed_info->Add(sizer_extra_info, 0, wxLEFT, 5);
    // m_sizer_main->Add(m_sizer_mode_switch, 0, wxALIGN_CENTER, 0);
   // m_sizer_main->Add(m_basic_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    //m_sizer_main->Add(sizer_split_filament, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->Add(m_filament_panel, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->Add(m_sizer_filament_2extruder, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));

    //m_sizer_main->Add(m_statictext_ams_msg, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(15));
   // m_sizer_main->Add(sizer_split_options, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));

    /*m_sizer_main->Add(sizer_advanced_options_title, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->Add(m_sizer_options_timelapse, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(15));
    m_sizer_main->Add(m_options_other, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(0));
    m_sizer_main->Add(m_simplebook, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    m_sizer_main->Add(m_sw_print_failed_info, 0, wxALIGN_CENTER, 0);*/

    {//new content//tip confirm ok button
        wxBoxSizer *tip_sizer = new wxBoxSizer(wxHORIZONTAL);
        m_attention_text      = new wxStaticText(this, wxID_ANY, _L("Attention") + ": ");
        tip_sizer->Add(m_attention_text, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));
        m_tip_text            = new wxStaticText(this, wxID_ANY, _L("Only synchronize filament type and color, not including AMS slot information."));
        m_tip_text->SetForegroundColour(wxColour(107, 107, 107, 100));
        tip_sizer->Add(m_tip_text, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));
        tip_sizer->AddSpacer(FromDIP(25));
        bSizer->Add(tip_sizer, 0, wxEXPAND | wxLEFT, FromDIP(25));

        add_two_image_control();

        wxBoxSizer * more_setting_sizer = new wxBoxSizer(wxVERTICAL);
        m_more_setting_tips = new wxStaticText(this, wxID_ANY, _L("Advanced settings >"));
        m_more_setting_tips->SetForegroundColour(wxColour(0, 174, 100));
        m_more_setting_tips->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
            m_expand_more_settings = !m_expand_more_settings;
            update_more_setting();
        });
        more_setting_sizer->Add(m_more_setting_tips, 0, wxALIGN_LEFT | wxTOP, FromDIP(4));

        m_append_color_sizer    = new wxBoxSizer(wxHORIZONTAL);
        m_append_color_checkbox = new ::CheckBox(this, wxID_ANY);
        //m_append_color_checkbox->SetForegroundColour(wxColour(107, 107, 107, 100));
        m_append_color_checkbox->SetValue(wxGetApp().app_config->get_bool("enable_append_color_by_sync_ams"));
        m_append_color_checkbox->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) {
            auto flag = wxGetApp().app_config->get_bool("enable_append_color_by_sync_ams");
            wxGetApp().app_config->set_bool("enable_append_color_by_sync_ams",!flag);
            m_append_color_checkbox->SetValue(!flag);
        });
        m_append_color_checkbox->Hide();
        m_append_color_sizer->Add(m_append_color_checkbox, 0, wxALIGN_LEFT | wxTOP, FromDIP(4));
        const int gap_between_checebox_and_text = 2;
        m_append_color_text = new wxStaticText(this, wxID_ANY, _L("Unused AMS filaments should also be added to the filament list."));
        m_append_color_text->Hide();
        m_append_color_sizer->AddSpacer(FromDIP(gap_between_checebox_and_text));
        m_append_color_sizer->Add(m_append_color_text, 0, wxALIGN_LEFT | wxTOP, FromDIP(4));

        more_setting_sizer->Add(m_append_color_sizer, 0, wxALIGN_LEFT | wxTOP, FromDIP(4));

        m_merge_color_sizer    = new wxBoxSizer(wxHORIZONTAL);
        m_merge_color_checkbox = new ::CheckBox(this, wxID_ANY);
        //m_merge_color_checkbox->SetForegroundColour(wxColour(107, 107, 107, 100));
        m_merge_color_checkbox->SetValue(wxGetApp().app_config->get_bool("enable_merge_color_by_sync_ams"));
        m_merge_color_checkbox->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) {
            auto flag = wxGetApp().app_config->get_bool("enable_merge_color_by_sync_ams");
            wxGetApp().app_config->set_bool("enable_merge_color_by_sync_ams",!flag);
            m_merge_color_checkbox->SetValue(!flag);
        });
        m_merge_color_checkbox->Hide();
        m_merge_color_sizer->Add(m_merge_color_checkbox, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));

        m_merge_color_text = new wxStaticText(this, wxID_ANY, _L("Automatically merge the same colors in the model after mapping."));
        m_merge_color_text->Hide();
        m_merge_color_sizer->AddSpacer(FromDIP(gap_between_checebox_and_text));
        m_merge_color_sizer->Add(m_merge_color_text, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));

        more_setting_sizer->Add(m_merge_color_sizer, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));

        bSizer->Add(more_setting_sizer, 0, wxEXPAND | wxLEFT, FromDIP(25));

        wxBoxSizer *confirm_boxsizer = new wxBoxSizer(wxVERTICAL);
        m_confirm_title              = new wxStaticText(this, wxID_ANY, _L("After sync, all currently configured filament presets and colors will be discarded."),
            wxDefaultPosition, wxDefaultSize);
        //m_confirm_title->Wrap(FromDIP(SyncAmsInfoDialogWidth - 50));
        //m_confirm_title->SetFont(Label::Head_14);
        confirm_boxsizer->Add(m_confirm_title, 0, wxALIGN_LEFT | wxTOP | wxRIGHT, FromDIP(10));
        m_are_you_sure_title = new wxStaticText(this, wxID_ANY,_L("Are you sure to synchronize the filaments?"));
        //m_are_you_sure_title->SetFont(Label::Head_14);
        confirm_boxsizer->Add(m_are_you_sure_title, 0, wxALIGN_LEFT | wxTOP, FromDIP(0));
        bSizer->Add(confirm_boxsizer, 0, wxALIGN_LEFT | wxLEFT, FromDIP(25));

        wxBoxSizer *warning_sizer = new wxBoxSizer(wxHORIZONTAL);
        m_warning_text            = new wxStaticText(this, wxID_ANY, _L("Error") + ":");
        m_warning_text->SetForegroundColour(wxColour(107, 107, 107, 100));
        m_warning_text->Hide();
        warning_sizer->Add(m_warning_text, 0, wxALIGN_CENTER | wxTOP, FromDIP(2));
        bSizer->Add(warning_sizer, 0, wxEXPAND | wxLEFT, FromDIP(25));

        wxBoxSizer *bSizer_button = new wxBoxSizer(wxHORIZONTAL);
        bSizer_button->SetMinSize(wxSize(FromDIP(100), -1));
        /* m_checkbox = new wxCheckBox(this, wxID_ANY, _L("Don't show again"), wxDefaultPosition, wxDefaultSize, 0);
         bSizer_button->Add(m_checkbox, 0, wxALIGN_LEFT);*/
        bSizer_button->AddStretchSpacer(1);
        StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(27, 136, 68), StateColor::Pressed), std::pair<wxColour, int>(wxColour(61, 203, 115), StateColor::Hovered),
                                std::pair<wxColour, int>(AMS_CONTROL_BRAND_COLOUR, StateColor::Normal));
        m_button_ok = new Button(this, m_input_info.connected_printer ? _L("Synchronize now") : _L("OK"));
        m_button_ok->SetBackgroundColor(btn_bg_green);
        m_button_ok->SetBorderColor(*wxWHITE);
        m_button_ok->SetTextColor(wxColour(0xFFFFFE));
        m_button_ok->SetFont(Label::Body_12);
        m_button_ok->SetSize(wxSize(FromDIP(90), FromDIP(24)));
        m_button_ok->SetMinSize(wxSize(FromDIP(90), FromDIP(24)));
        m_button_ok->SetCornerRadius(FromDIP(12));
        bSizer_button->Add(m_button_ok, 0, wxALIGN_RIGHT | wxLEFT | wxTOP, FromDIP(10));

        m_button_ok->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
            deal_ok();
            EndModal(wxID_YES);
            SetFocusIgnoringChildren();
        });

        StateColor btn_bg_white(std::pair<wxColour, int>(wxColour(206, 206, 206), StateColor::Pressed), std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Hovered),
                                std::pair<wxColour, int>(*wxWHITE, StateColor::Normal));

        m_button_cancel = new Button(this, m_input_info.cancel_text_to_later ? _L("Later") :_L("Cancel"));
        m_button_cancel->SetBackgroundColor(btn_bg_white);
        m_button_cancel->SetBorderColor(wxColour(38, 46, 48));
        m_button_cancel->SetFont(Label::Body_12);
        m_button_cancel->SetSize(BUTTON_SIZE);
        m_button_cancel->SetMinSize(BUTTON_SIZE);
        m_button_cancel->SetCornerRadius(FromDIP(12));
        bSizer_button->Add(m_button_cancel, 0, wxALIGN_RIGHT | wxLEFT | wxTOP, FromDIP(10));

        m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
            EndModal(wxID_CANCEL);
        });

        bSizer->Add(bSizer_button, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    }
    show_print_failed_info(false);

    SetSizer(m_sizer_main);
    Layout();
    Fit();
    Thaw();

    init_bind();
    init_timer();
    //Centre(wxBOTH);
    wxGetApp().UpdateDlgDarkUI(this);
}

void SyncAmsInfoDialog::init_bind()
{
    Bind(wxEVT_TIMER, &SyncAmsInfoDialog::on_timer, this);
    Bind(EVT_CLEAR_IPADDRESS, &SyncAmsInfoDialog::clear_ip_address_config, this);
    Bind(EVT_SHOW_ERROR_INFO, [this](auto &e) { show_print_failed_info(true); });
    Bind(EVT_UPDATE_USER_MACHINE_LIST, &SyncAmsInfoDialog::update_printer_combobox, this);
    Bind(EVT_PRINT_JOB_CANCEL, &SyncAmsInfoDialog::on_print_job_cancel, this);
    Bind(EVT_SET_FINISH_MAPPING, &SyncAmsInfoDialog::on_set_finish_mapping, this);
    Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        check_fcous_state(this);
        e.Skip();
    });
    m_panel_prepare->Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        check_fcous_state(this);
        e.Skip();
    });
    m_basic_panel->Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        check_fcous_state(this);
        e.Skip();
    });


    Bind(EVT_CONNECT_LAN_MODE_PRINT, [this](wxCommandEvent &e) {
        if (e.GetInt() == 0) {
            DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
            if (!dev) return;
            MachineObject *obj = dev->get_selected_machine();
            if (!obj) return;

            if (obj->dev_id == e.GetString()) { m_comboBox_printer->SetValue(obj->dev_name + "(LAN)"); }
        }
    });
}

void SyncAmsInfoDialog::check_focus(wxWindow *window)
{
    if (window == m_rename_input || window == m_rename_input->GetTextCtrl()) { on_rename_enter(); }
}

void SyncAmsInfoDialog::show_print_failed_info(bool show, int code, wxString description, wxString extra)
{
    if (show) {
        if (!m_sw_print_failed_info->IsShown()) {
            m_sw_print_failed_info->Show(true);

            m_st_txt_error_code->SetLabelText(wxString::Format("%d", m_print_error_code));
            m_st_txt_error_desc->SetLabelText(wxGetApp().filter_string(m_print_error_msg));
            m_st_txt_extra_info->SetLabelText(wxGetApp().filter_string(m_print_error_extra));

            m_st_txt_error_code->Wrap(FromDIP(260));
            m_st_txt_error_desc->Wrap(FromDIP(260));
            m_st_txt_extra_info->Wrap(FromDIP(260));
        } else {
            m_sw_print_failed_info->Show(false);
        }
        Layout();
        Fit();
    } else {
        if (!m_sw_print_failed_info->IsShown()) { return; }
        m_sw_print_failed_info->Show(false);
        m_st_txt_error_code->SetLabelText(wxEmptyString);
        m_st_txt_error_desc->SetLabelText(wxEmptyString);
        m_st_txt_extra_info->SetLabelText(wxEmptyString);
        Layout();
        Fit();
    }
}

void SyncAmsInfoDialog::check_fcous_state(wxWindow *window)
{
    check_focus(window);
    auto children = window->GetChildren();
    for (auto child : children) { check_fcous_state(child); }
}

void SyncAmsInfoDialog::popup_filament_backup()
{
    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    if (dev->get_selected_machine() /* && dev->get_selected_machine()->filam_bak.size() > 0*/) {
        AmsReplaceMaterialDialog *m_replace_material_popup = new AmsReplaceMaterialDialog(this);
        m_replace_material_popup->update_mapping_result(m_ams_mapping_result);
        m_replace_material_popup->update_machine_obj(dev->get_selected_machine());
        m_replace_material_popup->ShowModal();
    }
}

void SyncAmsInfoDialog::prepare_mode(bool refresh_button)
{
    // disable combobox
    m_comboBox_printer->Enable();
    Enable_Auto_Refill(true);
    show_print_failed_info(false);

    m_is_in_sending_mode = false;
    if (m_print_job) { m_print_job->join(); }
    if (wxIsBusy()) wxEndBusyCursor();

    if (refresh_button) { Enable_Send_Button(true); }

    m_status_bar->reset();
    if (m_simplebook->GetSelection() != 0) {
        m_simplebook->SetSelection(0);
        Layout();
        Fit();
    }

    if (m_print_page_mode != PrintPageModePrepare) {
        m_print_page_mode = PrintPageModePrepare;
        for (auto it = m_materialList.begin(); it != m_materialList.end(); it++) { it->second->item->enable(); }
    }
}

void SyncAmsInfoDialog::sending_mode()
{
    // disable combobox
    m_comboBox_printer->Disable();
    Enable_Auto_Refill(false);

    m_is_in_sending_mode = true;
    if (m_simplebook->GetSelection() != 1) {
        m_simplebook->SetSelection(1);
        Layout();
        Fit();
    }

    if (m_print_page_mode != PrintPageModeSending) {
        m_print_page_mode = PrintPageModeSending;
        for (auto it = m_materialList.begin(); it != m_materialList.end(); it++) { it->second->item->disable(); }
    }
}

void SyncAmsInfoDialog::finish_mode()
{
    m_print_page_mode    = PrintPageModeFinish;
    m_is_in_sending_mode = false;
    m_simplebook->SetSelection(2);
    Layout();
    Fit();
}

void SyncAmsInfoDialog::sync_ams_mapping_result(std::vector<FilamentInfo> &result)
{
    if (result.empty()) {
        BOOST_LOG_TRIVIAL(trace) << "ams_mapping result is empty";
        for (auto it = m_materialList.begin(); it != m_materialList.end(); it++) {
            wxString ams_id  = "Ext";
            wxColour ams_col = wxColour(0xCE, 0xCE, 0xCE);
            it->second->item->set_ams_info(ams_col, ams_id);
        }
        return;
    }

    for (auto f = result.begin(); f != result.end(); f++) {
        BOOST_LOG_TRIVIAL(trace) << "ams_mapping f id = " << f->id << ", tray_id = " << f->tray_id << ", color = " << f->color << ", type = " << f->type;

        MaterialHash::iterator iter = m_materialList.begin();
        while (iter != m_materialList.end()) {
            int           id   = iter->second->id;
            Material *    item = iter->second;
            MaterialItem *m    = item->item;

            if (f->id == id) {
                wxString ams_id;
                wxColour ams_col;

                if (f->tray_id == VIRTUAL_TRAY_MAIN_ID || f->tray_id == VIRTUAL_TRAY_DEPUTY_ID) {
                    ams_id = "Ext";
                }

                else if (f->tray_id >= 0) {
                    ams_id = wxGetApp().transition_tridid(f->tray_id);
                    // ams_id = wxString::Format("%02d", f->tray_id + 1);
                } else {
                    ams_id = "-";
                }

                if (!f->color.empty()) {
                    ams_col = AmsTray::decode_color(f->color);
                } else {
                    // default color
                    ams_col = wxColour(0xCE, 0xCE, 0xCE);
                }
                std::vector<wxColour> cols;
                for (auto col : f->colors) { cols.push_back(AmsTray::decode_color(col)); }
                m->set_ams_info(ams_col, ams_id, f->ctype, cols);
                break;
            }
            iter++;
        }
    }
    auto tab_index = (MainFrame::TabPosition) dynamic_cast<Notebook *>(wxGetApp().tab_panel())->GetSelection();
    if (tab_index == MainFrame::TabPosition::tp3DEditor || tab_index == MainFrame::TabPosition::tpPreview) { updata_thumbnail_data_after_connected_printer(); }
}

bool SyncAmsInfoDialog::do_ams_mapping(MachineObject *obj_)
{
    if (!obj_) return false;
    obj_->get_ams_colors(m_cur_colors_in_thumbnail);
    // try color and type mapping

    const auto &full_config    = wxGetApp().preset_bundle->full_config();
    const auto &project_config = wxGetApp().preset_bundle->project_config;
    size_t      nozzle_nums    = full_config.option<ConfigOptionFloatsNullable>("nozzle_diameter")->values.size();
    m_filaments_map            = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_real_filament_maps(project_config);

    int               filament_result = 0;
    std::vector<bool> map_opt; // four values: use_left_ams, use_right_ams, use_left_ext, use_right_ext
    if (nozzle_nums > 1) {
        // get nozzle property, the extders are same?
        if (false) {
            std::vector<FilamentInfo> m_ams_mapping_result_left, m_ams_mapping_result_right;
            std::vector<FilamentInfo> m_filament_left, m_filament_right;
            for (auto it = m_filaments.begin(); it != m_filaments.end(); it++) {
                if (it->id < 0 || it->id > m_filaments_map.size()) {
                    BOOST_LOG_TRIVIAL(info) << "error: do_ams_mapping: m_filaments[].it" << it->id;
                    BOOST_LOG_TRIVIAL(info) << "m_filaments_map.size()" << m_filaments_map.size();
                    return false;
                }
                if (m_filaments_map[it->id] == 1)
                    m_filament_left.push_back(*it);
                else if (m_filaments_map[it->id] == 2)
                    m_filament_right.push_back(*it);
            }

            bool has_left_ams = false, has_right_ams = false;
            for (auto ams_item : obj_->amsList) {
                if (ams_item.second->nozzle == 0) {
                    if (obj_->is_main_extruder_on_left())
                        has_left_ams = true;
                    else
                        has_right_ams = true;
                } else if (ams_item.second->nozzle == 1) {
                    if (obj_->is_main_extruder_on_left())
                        has_right_ams = true;
                    else
                        has_left_ams = true;
                }

                if (has_left_ams && has_right_ams) break;
            }

            map_opt           = {true, false, !has_left_ams, false}; // four values: use_left_ams, use_right_ams, use_left_ext, use_right_ext
            int result_first  = obj_->ams_filament_mapping(m_filament_left, m_ams_mapping_result_left, map_opt);
            map_opt           = {false, true, false, !has_right_ams};
            int result_second = obj_->ams_filament_mapping(m_filament_right, m_ams_mapping_result_right, map_opt);

            // m_ams_mapping_result.clear();
            m_ams_mapping_result.resize(m_ams_mapping_result_left.size() + m_ams_mapping_result_right.size());
            std::merge(m_ams_mapping_result_left.begin(), m_ams_mapping_result_left.end(), m_ams_mapping_result_right.begin(), m_ams_mapping_result_right.end(),
                       m_ams_mapping_result.begin(), [](const FilamentInfo &f1, const FilamentInfo &f2) {
                           return f1.id < f2.id; // Merge based on age
                       });
            filament_result = (result_first && result_second);
        }
        // can hybrid mapping
        else {
            map_opt         = {true, true, true, true}; // four values: use_left_ams, use_right_ams, use_left_ext, use_right_ext
            filament_result = obj_->ams_filament_mapping(m_filaments, m_ams_mapping_result, map_opt);
        }
        // When filaments cannot be matched automatically, whether to use ext for automatic supply
        // auto_supply_with_ext(obj_->vt_slot);
    }

    // single nozzle
    else {
        if (obj_->is_support_amx_ext_mix_mapping()) {
            map_opt         = {false, true, false, true}; // four values: use_left_ams, use_right_ams, use_left_ext, use_right_ext
            filament_result = obj_->ams_filament_mapping(m_filaments, m_ams_mapping_result, map_opt);
            // auto_supply_with_ext(obj_->vt_slot);
        } else {
            map_opt         = {false, true, false, false};
            filament_result = obj_->ams_filament_mapping(m_filaments, m_ams_mapping_result, map_opt);
        }
    }

    if (filament_result == 0) {
        print_ams_mapping_result(m_ams_mapping_result);
        std::string ams_array;
        std::string ams_array2;
        std::string mapping_info;
        get_ams_mapping_result(ams_array, ams_array2, mapping_info);
        if (ams_array.empty()) {
            reset_ams_material();
            BOOST_LOG_TRIVIAL(info) << "ams_mapping_array=[]";
        } else {
            sync_ams_mapping_result(m_ams_mapping_result);
            BOOST_LOG_TRIVIAL(info) << "ams_mapping_array=" << ams_array;
            BOOST_LOG_TRIVIAL(info) << "ams_mapping_array2=" << ams_array2;
            BOOST_LOG_TRIVIAL(info) << "ams_mapping_info=" << mapping_info;
        }
        return obj_->is_valid_mapping_result(m_ams_mapping_result);
    } else {
        // do not support ams mapping try to use order mapping
        bool is_valid = obj_->is_valid_mapping_result(m_ams_mapping_result);
        if (filament_result != 1 && !is_valid) {
            // reset invalid result
            for (int i = 0; i < m_ams_mapping_result.size(); i++) {
                m_ams_mapping_result[i].tray_id  = -1;
                m_ams_mapping_result[i].distance = 99999;
            }
        }
        sync_ams_mapping_result(m_ams_mapping_result);
        return is_valid;
    }

    return true;
}

bool SyncAmsInfoDialog::get_ams_mapping_result(std::string &mapping_array_str, std::string &mapping_array_str2, std::string &ams_mapping_info)
{
    if (m_ams_mapping_result.empty())
        return false;

    bool valid_mapping_result = true;
    int  invalid_count        = 0;
    for (int i = 0; i < m_ams_mapping_result.size(); i++) {
        if (m_ams_mapping_result[i].tray_id == -1) {
            valid_mapping_result = false;
            invalid_count++;
        }
    }

    if (invalid_count == m_ams_mapping_result.size()) {
        return false;
    } else {
        json mapping_v0_json   = json::array();
        json mapping_v1_json   = json::array();
        json mapping_info_json = json::array();

        /* get filament maps */
        std::vector<int> filament_maps;
        Plater *         plater = wxGetApp().plater();
        if (plater) {
            PartPlate *curr_plate = plater->get_partplate_list().get_curr_plate();
            if (curr_plate) {
                filament_maps = curr_plate->get_filament_maps();
            } else {
                BOOST_LOG_TRIVIAL(error) << "get_ams_mapping_result, curr_plate is nullptr";
            }
        } else {
            BOOST_LOG_TRIVIAL(error) << "get_ams_mapping_result, plater is nullptr";
        }

        for (int i = 0; i < wxGetApp().preset_bundle->filament_presets.size(); i++) {
            int  tray_id = -1;
            json mapping_item_v1;
            mapping_item_v1["ams_id"]  = 0xff;
            mapping_item_v1["slot_id"] = 0xff;
            json mapping_item;
            mapping_item["ams"]          = tray_id;
            mapping_item["targetColor"]  = "";
            mapping_item["filamentId"]   = "";
            mapping_item["filamentType"] = "";
            for (int k = 0; k < m_ams_mapping_result.size(); k++) {
                if (m_ams_mapping_result[k].id == i) {
                    tray_id                      = m_ams_mapping_result[k].tray_id;
                    mapping_item["ams"]          = tray_id;
                    mapping_item["filamentType"] = m_filaments[k].type;
                    if (i >= 0 && i < wxGetApp().preset_bundle->filament_presets.size()) {
                        auto it = wxGetApp().preset_bundle->filaments.find_preset(wxGetApp().preset_bundle->filament_presets[i]);
                        if (it != nullptr) { mapping_item["filamentId"] = it->filament_id; }
                    }
                    /* nozzle id */
                    if (i >= 0 && i < filament_maps.size()) { mapping_item["nozzleId"] = convert_filament_map_nozzle_id_to_task_nozzle_id(filament_maps[i]); }
                    // convert #RRGGBB to RRGGBBAA
                    mapping_item["sourceColor"] = m_filaments[k].color;
                    mapping_item["targetColor"] = m_ams_mapping_result[k].color;
                    if (tray_id == VIRTUAL_TRAY_MAIN_ID || tray_id == VIRTUAL_TRAY_DEPUTY_ID) { tray_id = -1; }

                    /*new ams mapping data*/
                    try {
                        if (m_ams_mapping_result[k].ams_id.empty() || m_ams_mapping_result[k].slot_id.empty()) { // invalid case
                            mapping_item_v1["ams_id"]  = VIRTUAL_TRAY_MAIN_ID;
                            mapping_item_v1["slot_id"] = VIRTUAL_TRAY_MAIN_ID;
                        } else {
                            mapping_item_v1["ams_id"]  = std::stoi(m_ams_mapping_result[k].ams_id);
                            mapping_item_v1["slot_id"] = std::stoi(m_ams_mapping_result[k].slot_id);
                        }
                    } catch (...) {}
                }
            }
            mapping_v0_json.push_back(tray_id);
            mapping_v1_json.push_back(mapping_item_v1);
            mapping_info_json.push_back(mapping_item);
        }

        mapping_array_str  = mapping_v0_json.dump();
        mapping_array_str2 = mapping_v1_json.dump();

        ams_mapping_info = mapping_info_json.dump();
        return valid_mapping_result;
    }
    return true;
}

bool SyncAmsInfoDialog::build_nozzles_info(std::string &nozzles_info)
{
    /* init nozzles info */
    json nozzle_info_json = json::array();
    nozzles_info          = nozzle_info_json.dump();

    PresetBundle *preset_bundle = wxGetApp().preset_bundle;
    if (!preset_bundle) return false;
    auto opt_nozzle_diameters = preset_bundle->printers.get_edited_preset().config.option<ConfigOptionFloatsNullable>("nozzle_diameter");
    if (opt_nozzle_diameters == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "build_nozzles_info, opt_nozzle_diameters is nullptr";
        return false;
    }
    auto opt_nozzle_volume_type = preset_bundle->project_config.option<ConfigOptionEnumsGeneric>("nozzle_volume_type");
    if (opt_nozzle_volume_type == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "build_nozzles_info, opt_nozzle_volume_type is nullptr";
        return false;
    }
    json nozzle_item;
    /* only o1d two nozzles has build_nozzles info now */
    if (opt_nozzle_diameters->size() != 2) { return false; }
    for (size_t i = 0; i < opt_nozzle_diameters->size(); i++) {
        if (i == (size_t) ConfigNozzleIdx::NOZZLE_LEFT) {
            nozzle_item["id"] = CloudTaskNozzleId::NOZZLE_LEFT;
        } else if (i == (size_t) ConfigNozzleIdx::NOZZLE_RIGHT) {
            nozzle_item["id"] = CloudTaskNozzleId::NOZZLE_RIGHT;
        } else {
            /* unknown ConfigNozzleIdx */
            BOOST_LOG_TRIVIAL(error) << "build_nozzles_info, unknown ConfigNozzleIdx = " << i;
            assert(false);
            continue;
        }
        nozzle_item["type"] = nullptr;
        if (i >= 0 && i < opt_nozzle_volume_type->size()) { nozzle_item["flowSize"] = get_nozzle_volume_type_cloud_string((NozzleVolumeType) opt_nozzle_volume_type->get_at(i)); }
        if (i >= 0 && i < opt_nozzle_diameters->size()) { nozzle_item["diameter"] = opt_nozzle_diameters->get_at(i); }
        nozzle_info_json.push_back(nozzle_item);
    }
    nozzles_info = nozzle_info_json.dump();
    return true;
}

bool SyncAmsInfoDialog::can_hybrid_mapping(ExtderData data)
{
    // Mixed mappings are not allowed
    return false;

    if (data.total_extder_count <= 1 || data.extders.size() <= 1 || !wxGetApp().preset_bundle) return false;

    // The default two extruders are left, right, but the order of the extruders on the machine is right, left.
    // Therefore, some adjustments need to be made.
    std::vector<std::string> flow_type_of_machine;
    for (auto it = data.extders.rbegin(); it != data.extders.rend(); it++) {
        // exist field is not updated, wait add
        // if (it->exist < 3) return false;
        std::string type_str = it->current_nozzle_flow_type ? "High Flow" : "Standard";
        flow_type_of_machine.push_back(type_str);
    }
    // get the nozzle type of preset --> flow_types
    const Preset &      current_printer = wxGetApp().preset_bundle->printers.get_selected_preset();
    const Preset *      base_printer    = wxGetApp().preset_bundle->printers.get_preset_base(current_printer);
    std::string         base_name       = base_printer->name;
    auto                flow_data       = wxGetApp().app_config->get_nozzle_volume_types_from_config(base_name);
    std::vector<string> flow_types;
    boost::split(flow_types, flow_data, boost::is_any_of(","));
    if (flow_types.size() <= 1 || flow_types.size() != flow_type_of_machine.size()) return false;

    // Only when all preset nozzle types and machine nozzle types are exactly the same, return true.
    auto type = flow_types[0];
    for (int i = 0; i < flow_types.size(); i++) {
        if (flow_types[i] != type || flow_type_of_machine[i] != type) return false;
    }
    return true;
}

// When filaments cannot be matched automatically, whether to use ext for automatic supply
void SyncAmsInfoDialog::auto_supply_with_ext(std::vector<AmsTray> slots)
{
    if (slots.size() <= 0) return;

    for (int i = 0; i < m_ams_mapping_result.size(); i++) {
        auto it = m_ams_mapping_result[i];
        if (it.ams_id == "") {
            AmsTray slot("");
            if (m_filaments_map[it.id] == 1 && slots.size() > 1)
                slot = slots[1];
            else if (m_filaments_map[it.id] == 2)
                slot = slots[0];
            if (slot.id.empty()) continue;
            m_ams_mapping_result[i].ams_id  = slot.id;
            m_ams_mapping_result[i].color   = slot.color;
            m_ams_mapping_result[i].type    = slot.type;
            m_ams_mapping_result[i].colors  = slot.cols;
            m_ams_mapping_result[i].tray_id = atoi(slot.id.c_str());
            m_ams_mapping_result[i].slot_id = "0";
        }
    }
}

bool SyncAmsInfoDialog::is_nozzle_type_match(ExtderData data)
{
    if (data.total_extder_count <= 1 || data.extders.size() <= 1 || !wxGetApp().preset_bundle) return false;

    const auto &project_config = wxGetApp().preset_bundle->project_config;
    // check nozzle used
    auto                       used_filaments = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_used_filaments();                   // 1 based
    auto                       filament_maps  = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_real_filament_maps(project_config); // 1 based
    std::map<int, std::string> used_extruders_flow;
    std::vector<int>           used_extruders; // 0 based
    for (auto f : used_filaments) {
        if (f <= 0) {
               BOOST_LOG_TRIVIAL(error) << "check error:f < 0";
               continue;
        }
        int filament_extruder = filament_maps[f - 1] - 1;
        if (std::find(used_extruders.begin(), used_extruders.end(), filament_extruder) == used_extruders.end()) {
            if (filament_extruder < 0) {
                BOOST_LOG_TRIVIAL(error) << "check error:filament_extruder < 0";
                continue;
            }
            used_extruders.emplace_back(filament_extruder);
        }
    }

    std::sort(used_extruders.begin(), used_extruders.end());

    auto nozzle_volume_type_opt = dynamic_cast<const ConfigOptionEnumsGeneric *>(wxGetApp().preset_bundle->project_config.option("nozzle_volume_type"));
    for (auto i = 0; i < used_extruders.size(); i++) {
        if (nozzle_volume_type_opt) {
            NozzleVolumeType nozzle_volume_type = (NozzleVolumeType) (nozzle_volume_type_opt->get_at(used_extruders[i]));
            if (nozzle_volume_type == NozzleVolumeType::nvtStandard) {
                used_extruders_flow[used_extruders[i]] = "Standard";
            } else {
                used_extruders_flow[used_extruders[i]] = "High Flow";
            }
        }
    }

    vector<int> map_extruders = {1, 0};

    // The default two extruders are left, right, but the order of the extruders on the machine is right, left.
    std::vector<std::string> flow_type_of_machine;
    for (auto it = data.extders.begin(); it != data.extders.end(); it++) {
        if (it->current_nozzle_flow_type == NozzleFlowType::H_FLOW) {
            flow_type_of_machine.push_back("High Flow");
        } else if (it->current_nozzle_flow_type == NozzleFlowType::S_FLOW) {
            flow_type_of_machine.push_back("Standard");
        }
    }

    // Only when all preset nozzle types and machine nozzle types are exactly the same, return true.
    for (std::map<int, std::string>::iterator it = used_extruders_flow.begin(); it != used_extruders_flow.end(); it++) {
        int target_machine_nozzle_id = map_extruders[it->first];

        if (target_machine_nozzle_id <= flow_type_of_machine.size()) {
            if (flow_type_of_machine[target_machine_nozzle_id] != used_extruders_flow[it->first]) { return false; }
        }
    }
    return true;
}

int SyncAmsInfoDialog::convert_filament_map_nozzle_id_to_task_nozzle_id(int nozzle_id)
{
    if (nozzle_id == (int) FilamentMapNozzleId::NOZZLE_LEFT) {
        return (int) CloudTaskNozzleId::NOZZLE_LEFT;
    } else if (nozzle_id == (int) FilamentMapNozzleId::NOZZLE_RIGHT) {
        return (int) CloudTaskNozzleId::NOZZLE_RIGHT;
    } else {
        /* unsupported nozzle id */
        assert(false);
        return nozzle_id;
    }
}

void SyncAmsInfoDialog::prepare(int print_plate_idx) { m_print_plate_idx = print_plate_idx; }

void SyncAmsInfoDialog::update_ams_status_msg(wxString msg, bool is_warning)
{
    if (!m_statictext_ams_msg) { return; }
    auto colour = is_warning ? wxColour(0xFF, 0x6F, 0x00) : wxColour(0x6B, 0x6B, 0x6B);
    m_statictext_ams_msg->SetForegroundColour(colour);

    if (msg.empty()) {
        if (!m_statictext_ams_msg->GetLabel().empty()) {
            m_statictext_ams_msg->SetLabel(wxEmptyString);
            m_statictext_ams_msg->Hide();
            Layout();
            Fit();
        }
    } else {
        msg = format_text(msg);

        auto str_new = msg.utf8_string();
        stripWhiteSpace(str_new);

        auto str_old = m_statictext_ams_msg->GetLabel().utf8_string();
        stripWhiteSpace(str_old);

        if (str_new != str_old) {
            if (m_statictext_ams_msg->GetLabel() != msg) {
                m_statictext_ams_msg->SetLabel(msg);
                m_statictext_ams_msg->Wrap(FromDIP(600));
                m_statictext_ams_msg->Show();
                Layout();
                Fit();
            }
        }
    }
}

void SyncAmsInfoDialog::stripWhiteSpace(std::string &str)
{
    if (str == "") { return; }

    string::iterator cur_it;
    cur_it = str.begin();

    while (cur_it != str.end()) {
        if ((*cur_it) == '\n' || (*cur_it) == ' ') {
            cur_it = str.erase(cur_it);
        } else {
            cur_it++;
        }
    }
}

wxString SyncAmsInfoDialog::format_text(wxString &m_msg)
{
    if (wxGetApp().app_config->get("language") != "zh_CN") {
        return m_msg;
    }
    if (!m_statictext_ams_msg) { return m_msg; }
    wxString out_txt      = m_msg;
    wxString count_txt    = "";
    int      new_line_pos = 0;

    for (int i = 0; i < m_msg.length(); i++) {
        auto text_size = m_statictext_ams_msg->GetTextExtent(count_txt);
        if (text_size.x < (FromDIP(400))) {
            count_txt += m_msg[i];
        } else {
            out_txt.insert(i - 1, '\n');
            count_txt = "";
        }
    }
    return out_txt;
}

void SyncAmsInfoDialog::update_priner_status_msg(wxString msg, bool is_warning)
{
    auto colour = is_warning ? wxColour(0xFF, 0x6F, 0x00) : wxColour(0x6B, 0x6B, 0x6B);
    m_text_printer_msg->SetForegroundColour(colour);

    if (msg.empty()) {
        if (!m_text_printer_msg->GetLabel().empty()) {
            m_text_printer_msg->SetLabel(wxEmptyString);
            m_text_printer_msg->Hide();
            Layout();
            Fit();
        }
    } else {
        msg = format_text(msg);

        auto str_new = msg.utf8_string();
        stripWhiteSpace(str_new);

        auto str_old = m_text_printer_msg->GetLabel().utf8_string();
        stripWhiteSpace(str_old);

        if (str_new != str_old) {
            if (m_text_printer_msg->GetLabel() != msg) {
                m_text_printer_msg->SetLabel(msg);
                m_text_printer_msg->SetMinSize(wxSize(FromDIP(420), -1));
                m_text_printer_msg->SetMaxSize(wxSize(FromDIP(420), -1));
                m_text_printer_msg->Wrap(FromDIP(420));
                m_text_printer_msg->Show();
                Layout();
                Fit();
            }
        }
    }
}

void SyncAmsInfoDialog::update_print_status_msg(wxString msg, bool is_warning, bool is_printer_msg)
{
    if (is_printer_msg) {
        update_ams_status_msg(wxEmptyString, false);
        update_priner_status_msg(msg, is_warning);
    } else {
        update_ams_status_msg(msg, is_warning);
        update_priner_status_msg(wxEmptyString, false);
    }
}

void SyncAmsInfoDialog::update_print_error_info(int code, std::string msg, std::string extra)
{
    m_print_error_code  = code;
    m_print_error_msg   = msg;
    m_print_error_extra = extra;
}

bool SyncAmsInfoDialog::has_tips(MachineObject *obj)
{
    if (!obj) return false;

    // must set to a status if return true
    if (m_checkbox_list["timelapse"]->IsShown() && (m_checkbox_list["timelapse"]->getValue() == "on")) {
        if (obj->get_sdcard_state() == MachineObject::SdcardState::NO_SDCARD) {
            show_status(PrintDialogStatus::PrintStatusTimelapseNoSdcard);
            return true;
        }
    }

    return false;
}

void SyncAmsInfoDialog::show_status(PrintDialogStatus status, std::vector<wxString> params)
{
    if (m_print_status != status) {
        m_result.is_same_printer = true;
        BOOST_LOG_TRIVIAL(info) << "select_machine_dialog: show_status = " << status << "(" << get_print_status_info(status) << ")";
    }
    m_print_status = status;

    // m_comboBox_printer
    if (status == PrintDialogStatus::PrintStatusRefreshingMachineList)
        m_comboBox_printer->Disable();
    else
        m_comboBox_printer->Enable();

    // other
    if (status == PrintDialogStatus::PrintStatusInit) {
        update_print_status_msg(wxEmptyString, false, false);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNoUserLogin) {
        wxString msg_text = _L("No login account, only printers in LAN mode are displayed");
        update_print_status_msg(msg_text, false, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusInvalidPrinter) {
        update_print_status_msg(wxEmptyString, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusConnectingServer) {
        wxString msg_text = _L("Connecting to server");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusReading) {
        wxString msg_text = _L("Synchronizing device information");
        update_print_status_msg(msg_text, false, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusReadingFinished) {
        update_print_status_msg(wxEmptyString, false, true);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusReadingTimeout) {
        wxString msg_text = _L("Synchronizing device information time out");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusInUpgrading) {
        wxString msg_text = _L("Cannot send the print job when the printer is updating firmware");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusInSystemPrinting) {
        wxString msg_text = _L("The printer is executing instructions. Please restart printing after it ends");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusInPrinting) {
        wxString msg_text = _L("The printer is busy on other print job");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusDisableAms) {
        update_print_status_msg(wxEmptyString, false, false);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNeedUpgradingAms) {
        wxString msg_text;
        if (params.size() > 0)
            msg_text = wxString::Format(_L("Filament %s exceeds the number of AMS slots. Please update the printer firmware to support AMS slot assignment."), params[0]);
        else
            msg_text = _L("Filament exceeds the number of AMS slots. Please update the printer firmware to support AMS slot assignment.");
        update_print_status_msg(msg_text, true, false);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusAmsMappingSuccess) {
        wxString msg_text = _L("Filaments to AMS slots mappings have been established. You can click a filament above to change its mapping AMS slot");
        update_print_status_msg(msg_text, false, false);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusAmsMappingInvalid) {
        wxString msg_text = _L("Please click each filament above to specify its mapping AMS slot before sending the print job");
        update_print_status_msg(msg_text, true, false);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusAmsMappingMixInvalid) {
        wxString msg_text = _L("Please do not mix-use the Ext with AMS");
        update_print_status_msg(msg_text, true, false);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNozzleDataInvalid) {
        wxString msg_text = _L("Invalid nozzle information, please refresh or manually set nozzle information.");
        update_print_status_msg(msg_text, true, false);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNozzleMatchInvalid) {
        wxString msg_text = _L("Please check whether the nozzle type of the device is the same as the preset nozzle type.");
        update_print_status_msg(msg_text, true, false);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusAmsMappingU0Invalid) {
        wxString msg_text;
        if (params.size() > 1)
            msg_text = wxString::Format(_L("Filament %s does not match the filament in AMS slot %s. Please update the printer firmware to support AMS slot assignment."),
                                        params[0], params[1]);
        else
            msg_text = _L("Filament does not match the filament in AMS slot. Please update the printer firmware to support AMS slot assignment.");
        update_print_status_msg(msg_text, true, false);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusAmsMappingValid) {
        wxString msg_text = _L("Filaments to AMS slots mappings have been established. You can click a filament above to change its mapping AMS slot");
        update_print_status_msg(msg_text, false, false);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusRefreshingMachineList) {
        update_print_status_msg(wxEmptyString, false, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(false);
    } else if (status == PrintDialogStatus::PrintStatusSending) {
        Enable_Send_Button(false);
        Enable_Refresh_Button(false);
    } else if (status == PrintDialogStatus::PrintStatusSendingCanceled) {
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusLanModeNoSdcard) {
        wxString msg_text = _L("Storage needs to be inserted before printing via LAN.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusLanModeSDcardNotAvailable) {
        wxString msg_text = _L("Storage is not available or is in read-only mode.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusAmsMappingByOrder) {
        wxString msg_text = _L("The printer firmware only supports sequential mapping of filament => AMS slot.");
        update_print_status_msg(msg_text, false, false);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNoSdcard) {
        wxString msg_text = _L("Storage needs to be inserted before printing.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusUnsupportedPrinter) {
        wxString msg_text;
        try {
            DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
            if (!dev) return;

            // source print
            MachineObject *obj_ = dev->get_selected_machine();
            if (obj_ == nullptr) return;
            auto sourcet_print_name = obj_->get_printer_type_display_str();
            sourcet_print_name.Replace(wxT("Bambu Lab "), wxEmptyString);

            // target print
            std::string target_model_id;
            if (m_print_type == PrintFromType::FROM_NORMAL) {
                PresetBundle *preset_bundle = wxGetApp().preset_bundle;
                target_model_id             = preset_bundle->printers.get_edited_preset().get_printer_type(preset_bundle);
            } else if (m_print_type == PrintFromType::FROM_SDCARD_VIEW) {
                if (m_required_data_plate_data_list.size() > 0) { target_model_id = m_required_data_plate_data_list[m_print_plate_idx]->printer_model_id; }
            }

            auto target_print_name = wxString(obj_->get_preset_printer_model_name(target_model_id));
            target_print_name.Replace(wxT("Bambu Lab "), wxEmptyString);
            msg_text = wxString::Format(_L("The selected printer (%s) is incompatible with the chosen printer profile in the slicer (%s)."), sourcet_print_name,
                                        target_print_name);
            if (m_result.is_same_printer) {
                m_result.is_same_printer = false;
                updata_ui_when_priner_not_same();
            }
            return;
        } catch (...) {}

        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusTimelapseNoSdcard) {
        wxString msg_text = _L("Storage needs to be inserted to record timelapse.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNeedForceUpgrading) {
        wxString msg_text = _L("Cannot send the print job to a printer whose firmware is required to get updated.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNeedConsistencyUpgrading) {
        wxString msg_text = _L("Cannot send the print job to a printer whose firmware is required to get updated.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusBlankPlate) {
        wxString msg_text = _L("Cannot send the print job for empty plate");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusNotSupportedPrintAll) {
        wxString msg_text = _L("This printer does not support printing all plates");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    } else if (status == PrintDialogStatus::PrintStatusTimelapseWarning) {
        wxString   msg_text;
        PartPlate *plate = m_plater->get_partplate_list().get_curr_plate();
        for (auto warning : plate->get_slice_result()->warnings) {
            if (warning.msg == NOT_GENERATE_TIMELAPSE) {
                if (warning.error_code == "1001C001") {
                    msg_text = _L("When enable spiral vase mode, machines with I3 structure will not generate timelapse videos.");
                } else if (warning.error_code == "1001C002") {
                    msg_text = _L("Timelapse is not supported because Print sequence is set to \"By object\".");
                }
            }
        }
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    } else if (status == PrintStatusMixAmsAndVtSlotWarning) {
        wxString msg_text = _L("You selected external and AMS filament at the same time in an extruder, you will need manually change external filament.");
        update_print_status_msg(msg_text, true, false);
        Enable_Send_Button(true);
        Enable_Refresh_Button(true);
    }

    // m_panel_warn m_simplebook
    if (status == PrintDialogStatus::PrintStatusSending) {
        sending_mode();
    } else {
        prepare_mode(false);
    }
}

void SyncAmsInfoDialog::init_timer()
{
    m_refresh_timer = new wxTimer();
    m_refresh_timer->SetOwner(this);
}

void SyncAmsInfoDialog::on_cancel(wxCloseEvent &event)
{
    if (m_mapping_popup.IsShown())
        m_mapping_popup.Dismiss();

    if (m_print_job) {
        if (m_print_job->is_running()) {
            m_print_job->cancel();
            m_print_job->join();
        }
    }
    this->EndModal(wxID_CANCEL);
}

bool SyncAmsInfoDialog::is_blocking_printing(MachineObject *obj_)
{
    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return true;
    auto        target_model = obj_->printer_type;
    std::string source_model = "";

    if (m_print_type == PrintFromType::FROM_NORMAL) {
        PresetBundle *preset_bundle = wxGetApp().preset_bundle;
        source_model                = preset_bundle->printers.get_edited_preset().get_printer_type(preset_bundle);

    } else if (m_print_type == PrintFromType::FROM_SDCARD_VIEW) {
        if (m_required_data_plate_data_list.size() > 0) { source_model = m_required_data_plate_data_list[m_print_plate_idx]->printer_model_id; }
    }

    if (source_model != target_model) {
        std::vector<std::string>      compatible_machine = dev->get_compatible_machine(target_model);
        vector<std::string>::iterator it                 = find(compatible_machine.begin(), compatible_machine.end(), source_model);
        if (it == compatible_machine.end()) { return true; }
    }

    return false;
}

bool SyncAmsInfoDialog::is_same_nozzle_diameters(NozzleType &tag_nozzle_type, float &nozzle_diameter)
{
    bool is_same_nozzle_diameters = true;

    float       preset_nozzle_diameters;
    std::string preset_nozzle_type;

    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return true;

    MachineObject *obj_ = dev->get_selected_machine();
    if (obj_ == nullptr) return true;

    try {
        PresetBundle *preset_bundle        = wxGetApp().preset_bundle;
        auto          opt_nozzle_diameters = preset_bundle->printers.get_edited_preset().config.option<ConfigOptionFloatsNullable>("nozzle_diameter");

        const ConfigOptionEnumsGenericNullable *nozzle_type = preset_bundle->printers.get_edited_preset().config.option<ConfigOptionEnumsGenericNullable>("nozzle_type");
        std::vector<std::string>                preset_nozzle_types(nozzle_type->size());
        for (size_t idx = 0; idx < nozzle_type->size(); ++idx) preset_nozzle_types[idx] = NozzleTypeEumnToStr[NozzleType(nozzle_type->values[idx])];

        std::vector<std::string> machine_nozzle_types(obj_->m_extder_data.extders.size());
        for (size_t idx = 0; idx < obj_->m_extder_data.extders.size(); ++idx) machine_nozzle_types[idx] = obj_->m_extder_data.extders[idx].current_nozzle_type;

        auto used_filaments = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_used_filaments();                                  // 1 based
        auto filament_maps  = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_real_filament_maps(preset_bundle->project_config); // 1 based

        std::vector<int> used_extruders; // 0 based
        for (auto f : used_filaments) {
            int filament_extruder = filament_maps[f - 1] - 1;
            if (std::find(used_extruders.begin(), used_extruders.end(), filament_extruder) == used_extruders.end()) used_extruders.emplace_back(filament_extruder);
        }
        std::sort(used_extruders.begin(), used_extruders.end());

        // TODO [tao wang] : add idx mapping
        tag_nozzle_type = obj_->m_extder_data.extders[0].current_nozzle_type;

        if (opt_nozzle_diameters != nullptr) {
            for (auto i = 0; i < used_extruders.size(); i++) {
                auto extruder           = used_extruders[i];
                preset_nozzle_diameters = float(opt_nozzle_diameters->get_at(extruder));
                if (preset_nozzle_diameters != obj_->m_extder_data.extders[0].current_nozzle_diameter) { is_same_nozzle_diameters = false; }
            }
        }

    } catch (...) {}

    nozzle_diameter = preset_nozzle_diameters;

    return is_same_nozzle_diameters;
}

bool SyncAmsInfoDialog::is_same_nozzle_type(std::string &filament_type, NozzleType &tag_nozzle_type)
{
    bool is_same_nozzle_type = true;

    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return true;

    MachineObject *obj_ = dev->get_selected_machine();
    if (obj_ == nullptr) return true;

    NozzleType nozzle_type        = obj_->m_extder_data.extders[0].current_nozzle_type;
    auto       printer_nozzle_hrc = Print::get_hrc_by_nozzle_type(nozzle_type);

    auto                   preset_bundle = wxGetApp().preset_bundle;
    MaterialHash::iterator iter          = m_materialList.begin();
    while (iter != m_materialList.end()) {
        Material *    item                = iter->second;
        MaterialItem *m                   = item->item;
        auto          filament_nozzle_hrc = preset_bundle->get_required_hrc_by_filament_type(m->m_material_name.ToStdString());

        if (abs(filament_nozzle_hrc) > abs(printer_nozzle_hrc)) {
            filament_type = m->m_material_name.ToStdString();
            BOOST_LOG_TRIVIAL(info) << "filaments hardness mismatch: filament = " << filament_type << " printer_nozzle_hrc = " << printer_nozzle_hrc;
            is_same_nozzle_type = false;
            tag_nozzle_type     = NozzleType::ntHardenedSteel;
            return is_same_nozzle_type;
        } else {
            tag_nozzle_type = obj_->m_extder_data.extders[0].current_nozzle_type;
        }

        iter++;
    }

    return is_same_nozzle_type;
}

bool SyncAmsInfoDialog::is_same_printer_model()
{
    bool           result = true;
    DeviceManager *dev    = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return result;

    MachineObject *obj_ = dev->get_selected_machine();

    assert(obj_->dev_id == m_printer_last_select);
    if (obj_ == nullptr) { return result; }

    PresetBundle *preset_bundle = wxGetApp().preset_bundle;
    if (preset_bundle && preset_bundle->printers.get_edited_preset().get_printer_type(preset_bundle) != obj_->printer_type) {
        if ((obj_->is_support_upgrade_kit && obj_->installed_upgrade_kit) && (preset_bundle->printers.get_edited_preset().get_printer_type(preset_bundle) == "C12")) {
            return true;
        }

        BOOST_LOG_TRIVIAL(info) << "printer_model: source = " << preset_bundle->printers.get_edited_preset().get_printer_type(preset_bundle);
        BOOST_LOG_TRIVIAL(info) << "printer_model: target = " << obj_->printer_type;
        return false;
    }

    if (obj_->is_support_upgrade_kit && obj_->installed_upgrade_kit) {
        BOOST_LOG_TRIVIAL(info) << "printer_model: source = " << preset_bundle->printers.get_edited_preset().get_printer_type(preset_bundle);
        BOOST_LOG_TRIVIAL(info) << "printer_model: target = " << obj_->printer_type << " (plus)";
        return false;
    }

    return true;
}

void SyncAmsInfoDialog::show_errors(wxString &info)
{
    ConfirmBeforeSendDialog confirm_dlg(this, wxID_ANY, _L("Errors"));
    confirm_dlg.update_text(info);
    confirm_dlg.on_show();
}

void SyncAmsInfoDialog::on_ok_btn(wxCommandEvent &event)
{
    bool has_slice_warnings = false;
    bool is_printing_block  = false;

    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject *obj_ = dev->get_selected_machine();
    if (!obj_) return;

    std::vector<ConfirmBeforeSendInfo> confirm_text;
    confirm_text.push_back(ConfirmBeforeSendInfo(_L("Please check the following:")));

    // Check Printer Model Id
    bool is_same_printer_type = is_same_printer_model();
    if (!is_same_printer_type && (m_print_type == PrintFromType::FROM_NORMAL)) {
        confirm_text.push_back(ConfirmBeforeSendInfo(_L("The printer type selected when generating G-Code is not consistent with the currently selected printer. It is "
                                                        "recommended that you use the same printer type for slicing.")));
        has_slice_warnings = true;
    }

    // check blacklist
    for (auto i = 0; i < m_ams_mapping_result.size(); i++) {
        auto tid = m_ams_mapping_result[i].tray_id;

        std::string filament_type = boost::to_upper_copy(m_ams_mapping_result[i].type);
        std::string filament_brand;

        for (auto fs : m_filaments) {
            if (fs.id == m_ams_mapping_result[i].id) { filament_brand = m_filaments[i].brand; }
        }

        bool        in_blacklist = false;
        std::string action;
        std::string info;
        DeviceManager::check_filaments_in_blacklist(filament_brand, filament_type, tid, in_blacklist, action, info);

        if (in_blacklist && action == "warning") {
            wxString prohibited_error = wxString::FromUTF8(info);

            confirm_text.push_back(ConfirmBeforeSendInfo(prohibited_error));
            has_slice_warnings = true;
        }
    }

    PartPlate *plate = m_plater->get_partplate_list().get_curr_plate();

    for (auto warning : plate->get_slice_result()->warnings) {
        if (warning.msg == BED_TEMP_TOO_HIGH_THAN_FILAMENT) {
            if ((obj_->get_printer_is_enclosed())) {
                // confirm_text.push_back(Plater::get_slice_warning_string(warning) + "\n");
                // has_slice_warnings = true;
            }
        } else if (warning.msg == NOT_SUPPORT_TRADITIONAL_TIMELAPSE) {
            if (obj_->get_printer_arch() == PrinterArch::ARCH_I3 && (m_checkbox_list["timelapse"]->getValue() == "on")) {
                confirm_text.push_back(ConfirmBeforeSendInfo(Plater::get_slice_warning_string(warning)));
                has_slice_warnings = true;
            }
        } else if (warning.msg == NOT_GENERATE_TIMELAPSE) {
            continue;
        } else if (warning.msg == NOZZLE_HRC_CHECKER) {
            wxString error_info = Plater::get_slice_warning_string(warning);
            if (error_info.IsEmpty()) { error_info = wxString::Format("%s\n", warning.msg); }

            confirm_text.push_back(ConfirmBeforeSendInfo(error_info));
            has_slice_warnings = true;
        }
    }

    // check for unidentified material
    auto mapping_result       = m_mapping_popup.parse_ams_mapping(obj_->amsList);
    auto has_unknown_filament = false;

    // check if ams mapping is has errors, tpu
    bool     has_prohibited_filament = false;
    wxString prohibited_error        = wxEmptyString;

    for (auto i = 0; i < m_ams_mapping_result.size(); i++) {
        auto tid = m_ams_mapping_result[i].tray_id;

        std::string filament_type = boost::to_upper_copy(m_ams_mapping_result[i].type);
        std::string filament_brand;

        for (auto fs : m_filaments) {
            if (fs.id == m_ams_mapping_result[i].id) { filament_brand = m_filaments[i].brand; }
        }

        bool        in_blacklist = false;
        std::string action;
        std::string info;
        DeviceManager::check_filaments_in_blacklist(filament_brand, filament_type, tid, in_blacklist, action, info);

        if (in_blacklist && action == "prohibition") {
            has_prohibited_filament = true;
            prohibited_error        = wxString::FromUTF8(info);
        }

        for (auto miter : mapping_result) {
            // matching
            if (miter.id == tid) {
                if (miter.type == TrayType::THIRD || miter.type == TrayType::EMPTY) {
                    has_unknown_filament = true;
                    break;
                }
            }
        }
    }

    if (has_prohibited_filament && obj_->has_ams() && (m_checkbox_list["use_ams"]->getValue() == "on")) {
        wxString tpu_tips = prohibited_error;
        show_errors(tpu_tips);
        return;
    }

    if (has_unknown_filament) {
        has_slice_warnings = true;
        confirm_text.push_back(ConfirmBeforeSendInfo(_L("There are some unknown filaments in the AMS mappings. Please check whether they are the required filaments. If they are "
                                                        "okay, press \"Confirm\" to start printing.")));
    }

    float       nozzle_diameter;
    std::string filament_type;
    NozzleType  tag_nozzle_type;

    if (!obj_->m_extder_data.extders[0].current_nozzle_type == NozzleType::ntUndefine && (m_print_type == PrintFromType::FROM_NORMAL)) {
        if (!is_same_nozzle_diameters(tag_nozzle_type, nozzle_diameter)) {
            has_slice_warnings = true;
            is_printing_block  = true;

            wxString nozzle_in_preset  = wxString::Format(_L("nozzle in preset: %.1f %s"), nozzle_diameter, "");
            wxString nozzle_in_printer = wxString::Format(_L("nozzle memorized: %.1f %s"), obj_->m_extder_data.extders[0].current_nozzle_diameter, "");

            confirm_text.push_back(ConfirmBeforeSendInfo(_L("Your nozzle diameter in sliced file is not consistent with memorized nozzle. If you changed your nozzle lately, "
                                                            "please go to Device > Printer Parts to change settings.") +
                                                             "\n    " + nozzle_in_preset + "\n    " + nozzle_in_printer + "\n",
                                                         ConfirmBeforeSendInfo::InfoLevel::Warning));
        }

        if (!is_same_nozzle_type(filament_type, tag_nozzle_type)) {
            has_slice_warnings = true;
            is_printing_block  = true;
            nozzle_diameter    = obj_->m_extder_data.extders[0].current_nozzle_diameter;

            wxString nozzle_in_preset = wxString::Format(_L("Printing %1s material with %2s nozzle may cause nozzle damage."), filament_type,
                                                         format_steel_name(obj_->m_extder_data.extders[0].current_nozzle_type));
            confirm_text.push_back(ConfirmBeforeSendInfo(nozzle_in_preset, ConfirmBeforeSendInfo::InfoLevel::Warning));
        }
    }

    if (has_slice_warnings) {
        wxString                confirm_title = _L("Warning");
        ConfirmBeforeSendDialog confirm_dlg(this, wxID_ANY, confirm_title);

        if (is_printing_block) {
            confirm_dlg.hide_button_ok();
            confirm_dlg.edit_cancel_button_txt(_L("Close"), true);
            confirm_text.push_back(ConfirmBeforeSendInfo(_L("Please fix the error above, otherwise printing cannot continue."), ConfirmBeforeSendInfo::InfoLevel::Warning));
        } else {
            confirm_text.push_back(ConfirmBeforeSendInfo(_L("Please click the confirm button if you still want to proceed with printing.")));
        }

        confirm_dlg.Bind(EVT_SECONDARY_CHECK_CONFIRM, [this, &confirm_dlg](wxCommandEvent &e) {
            confirm_dlg.on_hide();
            this->on_send_print();
        });

        wxString info_msg = wxEmptyString;

        for (auto i = 0; i < confirm_text.size(); i++) {
            if (i == 0) {
                // info_msg += confirm_text[i];
            } else if (i == confirm_text.size() - 1) {
                // info_msg += confirm_text[i];
            } else {
                confirm_text[i].text = wxString::Format("%d. %s", i, confirm_text[i].text);
            }
        }
        confirm_dlg.update_text(confirm_text);
        confirm_dlg.on_show();

    } else {
        this->on_send_print();
    }
}

wxString SyncAmsInfoDialog::format_steel_name(NozzleType type)
{
    if (type == NozzleType::ntHardenedSteel) {
        return _L("Hardened Steel");
    } else if (type == NozzleType::ntStainlessSteel) {
        return _L("Stainless Steel");
    }

    return wxEmptyString;
}

void SyncAmsInfoDialog::Enable_Auto_Refill(bool enable)
{
    if (!m_ams_backup_tip) { return; }
    if (enable) {
        m_ams_backup_tip->SetForegroundColour(wxColour(0x00AE42));
    } else {
        m_ams_backup_tip->SetForegroundColour(wxColour(0x90, 0x90, 0x90));
    }
    m_ams_backup_tip->Refresh();
}

void SyncAmsInfoDialog::connect_printer_mqtt()
{
    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject *obj_ = dev->get_selected_machine();

    if (obj_->connection_type() == "cloud") {
        show_status(PrintDialogStatus::PrintStatusSending);
        m_status_bar->disable_cancel_button();
        m_status_bar->set_status_text(_L("Connecting to the printer. Unable to cancel during the connection process."));

#if !BBL_RELEASE_TO_PUBLIC
        obj_->connect(false, wxGetApp().app_config->get("enable_ssl_for_mqtt") == "true" ? true : false);
#else
        obj_->connect(false, obj_->local_use_ssl_for_mqtt);
#endif

    } else {
        on_send_print();
    }
}

void SyncAmsInfoDialog::on_send_print()
{
    BOOST_LOG_TRIVIAL(info) << "print_job: on_ok to send";
    m_is_canceled = false;
    Enable_Send_Button(false);

    if (m_mapping_popup.IsShown())
        m_mapping_popup.Dismiss();

    if (m_print_type == PrintFromType::FROM_NORMAL && m_is_in_sending_mode) return;

    int result = 0;
    if (m_printer_last_select.empty()) { return; }

    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    MachineObject *obj_ = dev->get_selected_machine();
    assert(obj_->dev_id == m_printer_last_select);
    if (obj_ == nullptr) { return; }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", print_job: for send task, current printer id =  " << m_printer_last_select << std::endl;
    show_status(PrintDialogStatus::PrintStatusSending);

    m_status_bar->reset();
    m_status_bar->set_prog_block();
    m_status_bar->set_cancel_callback_fina([this]() {
        BOOST_LOG_TRIVIAL(info) << "print_job: enter canceled";
        if (m_print_job) {
            if (m_print_job->is_running()) {
                BOOST_LOG_TRIVIAL(info) << "print_job: canceled";
                m_print_job->cancel();
            }
            m_print_job->join();
        }
        m_is_canceled         = true;
        wxCommandEvent *event = new wxCommandEvent(EVT_PRINT_JOB_CANCEL);
        wxQueueEvent(this, event);
    });

    if (m_is_canceled) {
        BOOST_LOG_TRIVIAL(info) << "print_job: m_is_canceled";
        //m_status_bar->set_status_text(task_canceled_text);
        return;
    }

    // enter sending mode
    sending_mode();
    m_status_bar->enable_cancel_button();

    // get ams_mapping_result
    std::string ams_mapping_array;
    std::string ams_mapping_array2;
    std::string ams_mapping_info;
    if (m_checkbox_list["use_ams"]->getValue() == "on")
        get_ams_mapping_result(ams_mapping_array, ams_mapping_array2, ams_mapping_info);
    else {
        json mapping_info_json = json::array();
        json item;
        if (m_filaments.size() > 0) {
            item["sourceColor"]  = m_filaments[0].color.substr(1, 8);
            item["filamentType"] = m_filaments[0].type;
            mapping_info_json.push_back(item);
            ams_mapping_info = mapping_info_json.dump();
        }
    }

    if (m_print_type == PrintFromType::FROM_NORMAL) {
        result = m_plater->send_gcode(m_print_plate_idx, [this](int export_stage, int current, int total, bool &cancel) {
            if (this->m_is_canceled) return;
            bool     cancelled = false;
            wxString msg       = _L("Preparing print job");
            m_status_bar->update_status(msg, cancelled, 10, true);
            m_export_3mf_cancel = cancel = cancelled;
        });

        if (m_is_canceled || m_export_3mf_cancel) {
            BOOST_LOG_TRIVIAL(info) << "print_job: m_export_3mf_cancel or m_is_canceled";
           // m_status_bar->set_status_text(task_canceled_text);
            return;
        }

        if (result < 0) {
            wxString msg = _L("Abnormal print file data. Please slice again");
            m_status_bar->set_status_text(msg);
            return;
        }

        // export config 3mf if needed
        if (!obj_->is_lan_mode_printer()) {
            result = m_plater->export_config_3mf(m_print_plate_idx);
            if (result < 0) {
                BOOST_LOG_TRIVIAL(trace) << "export_config_3mf failed, result = " << result;
                return;
            }
        }
        if (m_is_canceled || m_export_3mf_cancel) {
            BOOST_LOG_TRIVIAL(info) << "print_job: m_export_3mf_cancel or m_is_canceled";
           // m_status_bar->set_status_text(task_canceled_text);
            return;
        }
    }

    m_print_job           = std::make_shared<PrintJob>(m_status_bar, m_plater, m_printer_last_select);
    m_print_job->m_dev_ip = obj_->dev_ip;
    // m_print_job->m_ftp_folder = obj_->get_ftp_folder();
    m_print_job->m_access_code = obj_->get_access_code();

#if !BBL_RELEASE_TO_PUBLIC
    m_print_job->m_local_use_ssl_for_ftp  = wxGetApp().app_config->get("enable_ssl_for_ftp") == "true" ? true : false;
    m_print_job->m_local_use_ssl_for_mqtt = wxGetApp().app_config->get("enable_ssl_for_mqtt") == "true" ? true : false;
#else
    m_print_job->m_local_use_ssl_for_ftp = obj_->local_use_ssl_for_ftp;
    m_print_job->m_local_use_ssl_for_mqtt = obj_->local_use_ssl_for_mqtt;
#endif

    m_print_job->connection_type  = obj_->connection_type();
    m_print_job->cloud_print_only = obj_->is_support_cloud_print_only;

    if (m_print_type == PrintFromType::FROM_NORMAL) {
        BOOST_LOG_TRIVIAL(info) << "print_job: m_print_type = from_normal";
        m_print_job->m_print_type = "from_normal";
        m_print_job->set_project_name(m_current_project_name.utf8_string());
    } else if (m_print_type == PrintFromType::FROM_SDCARD_VIEW) {
        BOOST_LOG_TRIVIAL(info) << "print_job: m_print_type = from_sdcard_view";
        m_print_job->m_print_type = "from_sdcard_view";
        // m_print_job->connection_type = "lan";

        try {
            m_print_job->m_print_from_sdc_plate_idx = m_required_data_plate_data_list[m_print_plate_idx]->plate_index + 1;
            m_print_job->set_dst_name(m_required_data_file_path);
        } catch (...) {}
        BOOST_LOG_TRIVIAL(info) << "print_job: m_print_plate_idx =" << m_print_job->m_print_from_sdc_plate_idx;

        auto input_str_arr = wxGetApp().split_str(m_required_data_file_name, ".gcode.3mf");
        if (input_str_arr.size() <= 1) {
            input_str_arr = wxGetApp().split_str(m_required_data_file_name, ".3mf");
            if (input_str_arr.size() > 1) { m_print_job->set_project_name(input_str_arr[0]); }
        } else {
            m_print_job->set_project_name(input_str_arr[0]);
        }
    }

    if (obj_->is_support_ams_mapping()) {
        m_print_job->task_ams_mapping      = ams_mapping_array;
        m_print_job->task_ams_mapping2     = ams_mapping_array2;
        m_print_job->task_ams_mapping_info = ams_mapping_info;
    } else {
        m_print_job->task_ams_mapping      = "";
        m_print_job->task_ams_mapping2     = "";
        m_print_job->task_ams_mapping_info = "";
    }

    /* build nozzles info for multi extruders printers */
    if (build_nozzles_info(m_print_job->task_nozzles_info)) { BOOST_LOG_TRIVIAL(error) << "build_nozzle_info errors"; }

    m_print_job->has_sdcard = obj_->get_sdcard_state() == MachineObject::SdcardState::HAS_SDCARD_NORMAL;

    bool timelapse_option = m_checkbox_list["timelapse"]->IsShown() ? true : false;
    if (timelapse_option) { timelapse_option = m_checkbox_list["timelapse"]->getValue() == "on"; }

    m_print_job->set_print_config(SelectMachineDialog::MachineBedTypeString[0], (m_checkbox_list["bed_leveling"]->getValue() == "on"),
                                  (m_checkbox_list["flow_cali"]->getValue() == "on"), false,
                                  timelapse_option, true, m_checkbox_list["bed_leveling"]->getValueInt(), m_checkbox_list["flow_cali"]->getValueInt(),
                                  m_checkbox_list["nozzle_offset_cali"]->getValueInt());

    if (obj_->has_ams()) {
        m_print_job->task_use_ams = (m_checkbox_list["use_ams"]->getValue() == "on");
    } else {
        m_print_job->task_use_ams = false;
    }

    BOOST_LOG_TRIVIAL(info) << "print_job: timelapse_option = " << timelapse_option;
    BOOST_LOG_TRIVIAL(info) << "print_job: use_ams = " << m_print_job->task_use_ams;

    m_print_job->on_success([this]() { finish_mode(); });

    m_print_job->on_check_ip_address_fail([this]() {
        wxCommandEvent *evt = new wxCommandEvent(EVT_CLEAR_IPADDRESS);
        wxQueueEvent(this, evt);
        wxGetApp().show_ip_address_enter_dialog();
    });

    // update ota version
    NetworkAgent *agent = wxGetApp().getAgent();
    if (agent) {
        std::string dev_ota_str = "dev_ota_ver:" + obj_->dev_id;
        agent->track_update_property(dev_ota_str, obj_->get_ota_version());
    }

    m_print_job->start();
    BOOST_LOG_TRIVIAL(info) << "print_job: start print job";
}

void SyncAmsInfoDialog::clear_ip_address_config(wxCommandEvent &e) { prepare_mode(); }

void SyncAmsInfoDialog::update_user_machine_list()
{
    NetworkAgent *m_agent = wxGetApp().getAgent();
    if (m_agent && m_agent->is_user_login()) {
        boost::thread get_print_info_thread = Slic3r::create_thread([this, token = std::weak_ptr(m_token)] {
            NetworkAgent *agent = wxGetApp().getAgent();
            unsigned int  http_code;
            std::string   body;
            int           result = agent->get_user_print_info(&http_code, &body);
            CallAfter([token, this, result, body] {
                if (token.expired()) { return; }
                if (result == 0) {
                    m_print_info = body;
                } else {
                    m_print_info = "";
                }
                wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
                event.SetEventObject(this);
                wxPostEvent(this, event);
            });
        });
    } else {
        wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
        event.SetEventObject(this);
        wxPostEvent(this, event);
    }
}

void SyncAmsInfoDialog::on_refresh(wxCommandEvent &event)
{
    if (m_is_empty_project) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "m_printer_last_select: on_refresh";
    show_status(PrintDialogStatus::PrintStatusRefreshingMachineList);

    update_user_machine_list();
}

void SyncAmsInfoDialog::on_set_finish_mapping(wxCommandEvent &evt)
{
    auto selection_data     = evt.GetString();
    auto selection_data_arr = wxSplit(selection_data.ToStdString(), '|');

    BOOST_LOG_TRIVIAL(info) << "The ams mapping selection result: data is " << selection_data;

    if (selection_data_arr.size() == 8) {
        auto ams_colour      = wxColour(wxAtoi(selection_data_arr[0]), wxAtoi(selection_data_arr[1]), wxAtoi(selection_data_arr[2]), wxAtoi(selection_data_arr[3]));
        int  old_filament_id = (int) wxAtoi(selection_data_arr[5]);
        if (m_print_type == PrintFromType::FROM_NORMAL) { // todo:support sd card
            change_default_normal(old_filament_id, ams_colour);
            final_deal_edge_pixels_data(m_preview_thumbnail_data);
            set_default_normal(m_preview_thumbnail_data); // do't reset ams
        }

        int                      ctype = 0;
        std::vector<wxColour>    material_cols;
        std::vector<std::string> tray_cols;
        for (auto mapping_item : m_mapping_popup.m_mapping_item_list) {
            if (mapping_item->m_tray_data.id == evt.GetInt()) {
                ctype         = mapping_item->m_tray_data.ctype;
                material_cols = mapping_item->m_tray_data.material_cols;
                for (auto col : mapping_item->m_tray_data.material_cols) {
                    wxString color = wxString::Format("#%02X%02X%02X%02X", col.Red(), col.Green(), col.Blue(), col.Alpha());
                    tray_cols.push_back(color.ToStdString());
                }
                break;
            }
        }

        for (auto i = 0; i < m_ams_mapping_result.size(); i++) {
            if (m_ams_mapping_result[i].id == wxAtoi(selection_data_arr[5])) {
                m_ams_mapping_result[i].tray_id = evt.GetInt();
                auto     ams_colour = wxColour(wxAtoi(selection_data_arr[0]), wxAtoi(selection_data_arr[1]), wxAtoi(selection_data_arr[2]), wxAtoi(selection_data_arr[3]));
                wxString color      = wxString::Format("#%02X%02X%02X%02X", ams_colour.Red(), ams_colour.Green(), ams_colour.Blue(), ams_colour.Alpha());
                m_ams_mapping_result[i].color  = color.ToStdString();
                m_ams_mapping_result[i].ctype  = ctype;
                m_ams_mapping_result[i].colors = tray_cols;

                m_ams_mapping_result[i].ams_id  = selection_data_arr[6].ToStdString();
                m_ams_mapping_result[i].slot_id = selection_data_arr[7].ToStdString();
            }
            BOOST_LOG_TRIVIAL(trace) << "The ams mapping result: id is " << m_ams_mapping_result[i].id << "tray_id is " << m_ams_mapping_result[i].tray_id;
        }

        MaterialHash::iterator iter = m_materialList.begin();
        while (iter != m_materialList.end()) {
            Material *    item = iter->second;
            MaterialItem *m    = item->item;
            if (item->id == m_current_filament_id) {
                auto ams_colour = wxColour(wxAtoi(selection_data_arr[0]), wxAtoi(selection_data_arr[1]), wxAtoi(selection_data_arr[2]), wxAtoi(selection_data_arr[3]));
                m->set_ams_info(ams_colour, selection_data_arr[4], ctype, material_cols);
            }
            iter++;
        }
    }
}

void SyncAmsInfoDialog::on_print_job_cancel(wxCommandEvent &evt)
{
    BOOST_LOG_TRIVIAL(info) << "print_job: canceled";
    show_status(PrintDialogStatus::PrintStatusInit);
    // enter prepare mode
    prepare_mode();
}

std::vector<std::string> SyncAmsInfoDialog::sort_string(std::vector<std::string> strArray)
{
    std::vector<std::string> outputArray;
    std::sort(strArray.begin(), strArray.end());
    std::vector<std::string>::iterator st;
    for (st = strArray.begin(); st != strArray.end(); st++) { outputArray.push_back(*st); }

    return outputArray;
}

bool SyncAmsInfoDialog::is_timeout()
{
    if (m_timeout_count > 15 * 1000 / LIST_REFRESH_INTERVAL) { return true; }
    return false;
}

int SyncAmsInfoDialog::update_print_required_data(
    Slic3r::DynamicPrintConfig config, Slic3r::Model model, Slic3r::PlateDataPtrs plate_data_list, std::string file_name, std::string file_path)
{
    m_required_data_plate_data_list.clear();
    m_required_data_config = config;
    m_required_data_model  = model;
    // m_required_data_plate_data_list = plate_data_list;
    for (auto i = 0; i < plate_data_list.size(); i++) {
        if (!plate_data_list[i]->gcode_file.empty()) { m_required_data_plate_data_list.push_back(plate_data_list[i]); }
    }

    m_required_data_file_name = file_name;
    m_required_data_file_path = file_path;
    return m_required_data_plate_data_list.size();
}

void SyncAmsInfoDialog::reset_timeout() { m_timeout_count = 0; }

void SyncAmsInfoDialog::update_user_printer()
{
    Slic3r::DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    // update user print info
    if (!m_print_info.empty()) {
        dev->parse_user_print_info(m_print_info);
        m_print_info = "";
    }

    // clear machine list
    m_list.clear();
    m_comboBox_printer->Clear();
    std::vector<std::string>               machine_list;
    wxArrayString                          machine_list_name;
    std::map<std::string, MachineObject *> option_list;

    // user machine list
    option_list = dev->get_my_machine_list();

    // same machine only appear once
    for (auto it = option_list.begin(); it != option_list.end(); it++) {
        if (it->second && (it->second->is_online() || it->second->is_connected())) { machine_list.push_back(it->second->dev_name); }
    }

    // lan machine list
    auto lan_option_list = dev->get_local_machine_list();

    for (auto elem : lan_option_list) {
        MachineObject *mobj = elem.second;

        /* do not show printer bind state is empty */
        if (!mobj->is_avaliable()) continue;
        if (!mobj->is_online()) continue;
        if (!mobj->is_lan_mode_printer()) continue;
        if (!mobj->has_access_right()) {
            option_list[mobj->dev_name] = mobj;
            machine_list.push_back(mobj->dev_name);
        }
    }

    machine_list = sort_string(machine_list);
    for (auto tt = machine_list.begin(); tt != machine_list.end(); tt++) {
        for (auto it = option_list.begin(); it != option_list.end(); it++) {
            if (it->second->dev_name == *tt) {
                m_list.push_back(it->second);
                wxString dev_name_text = from_u8(it->second->dev_name);
                if (it->second->is_lan_mode_printer()) { dev_name_text += "(LAN)"; }
                machine_list_name.Add(dev_name_text);
                break;
            }
        }
    }

    m_comboBox_printer->Set(machine_list_name);

    MachineObject *obj = dev->get_selected_machine();

    if (obj) {
        if (obj->is_lan_mode_printer() && !obj->has_access_right()) {
            m_printer_last_select = "";
        } else {
            m_printer_last_select = obj->dev_id;
        }

    } else {
        m_printer_last_select = "";
    }

    if (m_list.size() > 0) {
        // select a default machine
        if (m_printer_last_select.empty()) {
            int def_selection = -1;
            for (int i = 0; i < m_list.size(); i++) {
                if (m_list[i]->is_lan_mode_printer() && !m_list[i]->has_access_right()) {
                    continue;
                } else {
                    def_selection = i;
                }
            }

            if (def_selection >= 0) {
                m_printer_last_select = m_list[def_selection]->dev_id;
                m_comboBox_printer->SetSelection(def_selection);
                wxCommandEvent event(wxEVT_COMBOBOX);
                event.SetEventObject(m_comboBox_printer);
                wxPostEvent(m_comboBox_printer, event);
            }
        }

        for (auto i = 0; i < m_list.size(); i++) {
            if (m_list[i]->dev_id == m_printer_last_select) {
                if (obj && !obj->get_lan_mode_connection_state()) {
                    m_comboBox_printer->SetSelection(i);
                    m_printer_name = m_comboBox_printer->GetValue();
                    wxCommandEvent event(wxEVT_COMBOBOX);
                    event.SetEventObject(m_comboBox_printer);
                    wxPostEvent(m_comboBox_printer, event);
                }
            }
        }
    } else {
        m_printer_last_select = "";
        update_select_layout(nullptr);
        m_comboBox_printer->SetTextLabel("");
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "for send task, current printer id =  " << m_printer_last_select << std::endl;
}

void SyncAmsInfoDialog::on_rename_click(wxMouseEvent &event)
{
    m_is_rename_mode = true;
    m_rename_input->GetTextCtrl()->SetValue(m_current_project_name);
    m_rename_switch_panel->SetSelection(1);
    m_rename_input->GetTextCtrl()->SetFocus();
    m_rename_input->GetTextCtrl()->SetInsertionPointEnd();
}

void SyncAmsInfoDialog::on_rename_enter()
{
    if (m_is_rename_mode == false) {
        return;
    } else {
        m_is_rename_mode = false;
    }

    auto     new_file_name = m_rename_input->GetTextCtrl()->GetValue();
    wxString temp;
    int      num = 0;
    for (auto t : new_file_name) {
        if (t == wxString::FromUTF8("\x20")) {
            num++;
            if (num == 1) temp += t;
        } else {
            num = 0;
            temp += t;
        }
    }
    new_file_name         = temp;
    auto     m_valid_type = Valid;
    wxString info_line;

    const char *unusable_symbols = "<>[]:/\\|?*\"";

    const std::string unusable_suffix = PresetCollection::get_suffix_modified(); //"(modified)";
    for (size_t i = 0; i < std::strlen(unusable_symbols); i++) {
        if (new_file_name.find_first_of(unusable_symbols[i]) != std::string::npos) {
            info_line    = _L("Name is invalid;") + "\n" + _L("illegal characters:") + " " + unusable_symbols;
            m_valid_type = NoValid;
            break;
        }
    }

    if (m_valid_type == Valid && new_file_name.find(unusable_suffix) != std::string::npos) {
        info_line    = _L("Name is invalid;") + "\n" + _L("illegal suffix:") + "\n\t" + from_u8(PresetCollection::get_suffix_modified());
        m_valid_type = NoValid;
    }

    if (m_valid_type == Valid && new_file_name.empty()) {
        info_line    = _L("The name is not allowed to be empty.");
        m_valid_type = NoValid;
    }

    if (m_valid_type == Valid && new_file_name.find_first_of(' ') == 0) {
        info_line    = _L("The name is not allowed to start with space character.");
        m_valid_type = NoValid;
    }

    if (m_valid_type == Valid && new_file_name.find_last_of(' ') == new_file_name.length() - 1) {
        info_line    = _L("The name is not allowed to end with space character.");
        m_valid_type = NoValid;
    }

    if (m_valid_type == Valid && new_file_name.size() >= 100) {
        info_line    = _L("The name length exceeds the limit.");
        m_valid_type = NoValid;
    }

    if (m_valid_type != Valid) {
        MessageDialog msg_wingow(nullptr, info_line, "", wxICON_WARNING | wxOK);
        if (msg_wingow.ShowModal() == wxID_OK) {
            m_rename_switch_panel->SetSelection(0);
            m_rename_text->SetLabel(m_current_project_name);
            m_rename_normal_panel->Layout();
            return;
        }
    }

    m_current_project_name = new_file_name;
    m_rename_switch_panel->SetSelection(0);
    m_rename_text->SetLabelText(m_current_project_name);
    m_rename_normal_panel->Layout();
}

void SyncAmsInfoDialog::update_printer_combobox(wxCommandEvent &event)
{
    show_status(PrintDialogStatus::PrintStatusInit);
    update_user_printer();
}

void SyncAmsInfoDialog::on_timer(wxTimerEvent &event)
{
    wxGetApp().reset_to_active();
    update_show_status();

    /// show auto refill
    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject *obj_ = dev->get_selected_machine();
    if (!obj_) return;

    if (!m_check_flag && obj_->is_info_ready()) {
        update_select_layout(obj_);
        update_ams_check(obj_);
        m_check_flag = true;
    }

    if (!obj_ || obj_->amsList.empty() || obj_->ams_exist_bits == 0 || !obj_->is_support_filament_backup || !obj_->is_support_show_filament_backup ||
        !obj_->ams_auto_switch_filament_flag || m_checkbox_list["use_ams"]->getValue() != "on") {
        if (m_ams_backup_tip && m_ams_backup_tip->IsShown()) {
            m_ams_backup_tip->Hide();
            img_ams_backup->Hide();
            Layout();
            Fit();
        }
    } else {
        if (m_ams_backup_tip && !m_ams_backup_tip->IsShown()) {
            m_ams_backup_tip->Show();
            img_ams_backup->Show();
            Layout();
            Fit();
        }
    }
}

void SyncAmsInfoDialog::on_selection_changed(wxCommandEvent &event)
{
    /* reset timeout and reading printer info */
    m_status_bar->reset();
    m_timeout_count     = 0;
    m_ams_mapping_res   = false;
    m_ams_mapping_valid = false;
    m_ams_mapping_result.clear();

    auto           selection = m_comboBox_printer->GetSelection();
    DeviceManager *dev       = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    MachineObject *obj = nullptr;
    for (int i = 0; i < m_list.size(); i++) {
        if (i == selection) {
            // check lan mode machine
            //if (m_list[i]->is_lan_mode_printer() && !m_list[i]->has_access_right()) {
            //   /* ConnectPrinterDialog dlg(wxGetApp().mainframe, wxID_ANY, _L("Input access code"));
            //    dlg.set_machine_object(m_list[i]);
            //    auto res              = dlg.ShowModal();
            //    m_printer_last_select = "";
            //    m_comboBox_printer->SetSelection(-1);
            //    m_comboBox_printer->Refresh();
            //    m_comboBox_printer->Update();*/
            //}

            //m_printer_last_select = m_list[i]->dev_id;
            //obj                   = m_list[i];

            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "for send task, current printer id =  " << m_printer_last_select << std::endl;
            break;
        }
    }

    if (obj) {
        // update image
        auto printer_img_name = "printer_preview_" + obj->printer_type;
        m_printer_image->SetBitmap(create_scaled_bitmap(printer_img_name, this, 52));

        obj->command_get_version();
        obj->command_request_push_all();
        if (!dev->get_selected_machine()) {
            dev->set_selected_machine(m_printer_last_select, true);
        } else if (dev->get_selected_machine()->dev_id != m_printer_last_select) {
            dev->set_selected_machine(m_printer_last_select, true);
        }

        // reset the timelapse check status for I3 structure
        if (obj->get_printer_arch() == PrinterArch::ARCH_I3) {
            m_checkbox_list["timelapse"]->setValue("off");
            AppConfig *config = wxGetApp().app_config;
            if (config) config->set_str("print", "timelapse", "0");
        }

        // Has changed machine unrecoverably
        GUI::wxGetApp().sidebar().load_ams_list(obj->dev_id, obj);
        m_check_flag = false;
    } else {
        BOOST_LOG_TRIVIAL(error) << "on_selection_changed dev_id not found";
        return;
    }

    // reset print status
    update_flow_cali_check(obj);

    show_status(PrintDialogStatus::PrintStatusInit);

    update_show_status();
}

void SyncAmsInfoDialog::update_flow_cali_check(MachineObject *obj)
{
    auto bed_type       = m_plater->get_partplate_list().get_curr_plate()->get_bed_type(true);
    auto show_cali_tips = true;

    if (obj && obj->get_printer_arch() == PrinterArch::ARCH_I3) { show_cali_tips = false; }

    set_flow_calibration_state(true, show_cali_tips);
}

void SyncAmsInfoDialog::update_show_status()
{
    // refreshing return
    if (get_status() == PrintDialogStatus::PrintStatusRefreshingMachineList) return;

    if (get_status() == PrintDialogStatus::PrintStatusSending) return;

    if (get_status() == PrintDialogStatus::PrintStatusSendingCanceled) return;

    NetworkAgent * agent = Slic3r::GUI::wxGetApp().getAgent();
    DeviceManager *dev   = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!agent) {
        update_ams_check(nullptr);
        return;
    }
    if (!dev) return;
    dev->check_pushing();

    // blank plate has no valid gcode file
    if (is_must_finish_slice_then_connected_printer()) { return; }
    MachineObject *obj_ = dev->get_my_machine(m_printer_last_select);
    if (!obj_) {
        update_ams_check(nullptr);
        if (agent) {
            if (agent->is_user_login()) {
                show_status(PrintDialogStatus::PrintStatusInvalidPrinter);
            } else {
                show_status(PrintDialogStatus::PrintStatusNoUserLogin);
            }
        }
        return;
    }
    agent->install_device_cert(obj_->dev_id, obj_->is_lan_mode_printer());

    /* check cloud machine connections */
    if (!obj_->is_lan_mode_printer()) {
        if (!agent->is_server_connected()) {
            agent->refresh_connection();
            show_status(PrintDialogStatus::PrintStatusConnectingServer);
            reset_timeout();
            return;
        }
    }

    if (!obj_->is_info_ready()) {
        if (is_timeout()) {
            m_ams_mapping_result.clear();
            sync_ams_mapping_result(m_ams_mapping_result);
            show_status(PrintDialogStatus::PrintStatusReadingTimeout);
            return;
        } else {
            m_timeout_count++;
            show_status(PrintDialogStatus::PrintStatusReading);
            return;
        }
        return;
    }

    reset_timeout();

    if (!obj_->is_support_print_all && m_print_plate_idx == PLATE_ALL_IDX) {
        show_status(PrintDialogStatus::PrintStatusNotSupportedPrintAll);
        return;
    }

    // do ams mapping if no ams result
    bool clean_ams_mapping = false;
    if (m_ams_mapping_result.empty()) {
        if (m_checkbox_list.find("use_ams") != m_checkbox_list.end() && m_checkbox_list["use_ams"]->getValue() == "on") {
            do_ams_mapping(obj_);
        } else {
            clean_ams_mapping = true;
        }
    }

    if (clean_ams_mapping) {
        m_ams_mapping_result.clear();
        sync_ams_mapping_result(m_ams_mapping_result);
    }

    // reading done
    if (wxGetApp().app_config && wxGetApp().app_config->get("internal_debug").empty()) {
        if (obj_->upgrade_force_upgrade) {
            show_status(PrintDialogStatus::PrintStatusNeedForceUpgrading);
            return;
        }

        if (obj_->upgrade_consistency_request) {
            show_status(PrintStatusNeedConsistencyUpgrading);
            return;
        }
    }

    if (is_blocking_printing(obj_)) {
        show_status(PrintDialogStatus::PrintStatusUnsupportedPrinter);
        return;
    } else if (obj_->is_in_upgrading()) {
        show_status(PrintDialogStatus::PrintStatusInUpgrading);
        return;
    } else if (obj_->is_system_printing()) {
        show_status(PrintDialogStatus::PrintStatusInSystemPrinting);
        return;
    } else if (obj_->is_in_printing() || obj_->ams_status_main == AMS_STATUS_MAIN_FILAMENT_CHANGE) {
        show_status(PrintDialogStatus::PrintStatusInPrinting);
        return;
    } else if (!obj_->is_support_print_without_sd && (obj_->get_sdcard_state() == MachineObject::SdcardState::NO_SDCARD)) {
        show_status(PrintDialogStatus::PrintStatusNoSdcard);
        return;
    }

    // check sdcard when if lan mode printer
    if (obj_->is_lan_mode_printer()) {
        if (obj_->get_sdcard_state() == MachineObject::SdcardState::NO_SDCARD) {
            show_status(PrintDialogStatus::PrintStatusLanModeNoSdcard);
            return;
        } else if (obj_->get_sdcard_state() == MachineObject::SdcardState::HAS_SDCARD_ABNORMAL || obj_->get_sdcard_state() == MachineObject::SdcardState::HAS_SDCARD_READONLY) {
            show_status(PrintDialogStatus::PrintStatusLanModeSDcardNotAvailable);
            return;
        }
    }

    // no ams
    if (!obj_->has_ams() || m_checkbox_list["use_ams"]->getValue() != "on") {
        if (!has_tips(obj_)) {
            if (has_timelapse_warning()) {
                show_status(PrintDialogStatus::PrintStatusTimelapseWarning);
            } else {
                show_status(PrintDialogStatus::PrintStatusReadingFinished);
            }
        }
        return;
    }

    if (m_checkbox_list["use_ams"]->getValue() != "on") {
        m_ams_mapping_result.clear();
        sync_ams_mapping_result(m_ams_mapping_result);

        if (has_timelapse_warning()) {
            show_status(PrintDialogStatus::PrintStatusTimelapseWarning);
        } else {
            show_status(PrintDialogStatus::PrintStatusDisableAms);
        }

        return;
    }

    // do ams mapping if no ams result
    if (m_ams_mapping_result.empty()) { do_ams_mapping(obj_); }

    const auto &full_config = wxGetApp().preset_bundle->full_config();
    size_t      nozzle_nums = full_config.option<ConfigOptionFloatsNullable>("nozzle_diameter")->values.size();

    // the nozzle type of preset and machine are different
    if (nozzle_nums > 1) {
        if (!obj_->is_nozzle_data_invalid()) {
            show_status(PrintDialogStatus::PrintStatusNozzleDataInvalid);
            return;
        }

        if (!is_nozzle_type_match(obj_->m_extder_data)) {
            show_status(PrintDialogStatus::PrintStatusNozzleMatchInvalid);
            return;
        }
    }

    if (!m_mapping_popup.m_supporting_mix_print && nozzle_nums == 1) {
        bool useAms = false;
        bool useExt = false;
        for (auto iter = m_ams_mapping_result.begin(); iter != m_ams_mapping_result.end(); iter++) {
            if (iter->tray_id != VIRTUAL_TRAY_MAIN_ID) { useAms = true; }
            if (iter->tray_id == VIRTUAL_TRAY_MAIN_ID) { useExt = true; }
            if (useAms && useExt) {
                show_status(PrintDialogStatus::PrintStatusAmsMappingMixInvalid);
                return;
            }
        }
    }

    // check ams and vt_slot mix use status
    {
        struct ExtruderStatus
        {
            bool has_ams{false};
            bool has_vt_slot{false};
        };
        std::vector<ExtruderStatus> extruder_status(nozzle_nums);
        for (const FilamentInfo &item : m_ams_mapping_result) {
            if (item.ams_id.empty()) continue;

            int extruder_id = obj_->get_extruder_id_by_ams_id(item.ams_id);
            if (DeviceManager::is_virtual_slot(stoi(item.ams_id)))
                extruder_status[extruder_id].has_vt_slot = true;
            else
                extruder_status[extruder_id].has_ams = true;
        }
        for (auto extruder : extruder_status) {
            if (extruder.has_ams && extruder.has_vt_slot) {
                show_status(PrintDialogStatus::PrintStatusMixAmsAndVtSlotWarning);
                return;
            }
        }
    }

    if (!obj_->is_support_ams_mapping()) {
        int exceed_index = -1;
        if (obj_->is_mapping_exceed_filament(m_ams_mapping_result, exceed_index)) {
            std::vector<wxString> params;
            params.push_back(wxString::Format("%02d", exceed_index + 1));
            show_status(PrintDialogStatus::PrintStatusNeedUpgradingAms, params);
        } else {
            if (obj_->is_valid_mapping_result(m_ams_mapping_result)) {
                if (has_timelapse_warning()) {
                    show_status(PrintDialogStatus::PrintStatusTimelapseWarning);
                } else {
                    show_status(PrintDialogStatus::PrintStatusAmsMappingByOrder);
                }

            } else {
                int mismatch_index = -1;
                for (int i = 0; i < m_ams_mapping_result.size(); i++) {
                    if (m_ams_mapping_result[i].mapping_result == MappingResult::MAPPING_RESULT_TYPE_MISMATCH) {
                        mismatch_index = m_ams_mapping_result[i].id;
                        break;
                    }
                }
                std::vector<wxString> params;
                if (mismatch_index >= 0) {
                    params.push_back(wxString::Format("%02d", mismatch_index + 1));
                    params.push_back(wxString::Format("%02d", mismatch_index + 1));
                }
                show_status(PrintDialogStatus::PrintStatusAmsMappingU0Invalid, params);
            }
        }
        return;
    }

    if (m_ams_mapping_res) {
        if (has_timelapse_warning()) {
            show_status(PrintDialogStatus::PrintStatusTimelapseWarning);
        } else {
            show_status(PrintDialogStatus::PrintStatusAmsMappingSuccess);
        }
        return;
    } else {
        if (obj_->is_valid_mapping_result(m_ams_mapping_result)) {
            if (!has_tips(obj_)) {
                if (has_timelapse_warning()) {
                    show_status(PrintDialogStatus::PrintStatusTimelapseWarning);
                } else {
                    show_status(PrintDialogStatus::PrintStatusAmsMappingValid);
                }
                return;
            }
        } else {
            show_status(PrintDialogStatus::PrintStatusAmsMappingInvalid);
            return;
        }
    }
}

bool SyncAmsInfoDialog::has_timelapse_warning()
{
    PartPlate *plate = m_plater->get_partplate_list().get_curr_plate();
    for (auto warning : plate->get_slice_result()->warnings) {
        if (warning.msg == NOT_GENERATE_TIMELAPSE) { return true; }
    }

    return false;
}

void SyncAmsInfoDialog::update_timelapse_enable_status()
{
    AppConfig *config = wxGetApp().app_config;
    if (!has_timelapse_warning()) {
        if (!config || config->get("print", "timelapse") == "0")
            m_checkbox_list["timelapse"]->setValue("off");
        else
            m_checkbox_list["timelapse"]->setValue("on");
        m_checkbox_list["timelapse"]->Enable(true);
    } else {
        m_checkbox_list["timelapse"]->setValue("off");
        m_checkbox_list["timelapse"]->Enable(false);
        if (config) { config->set_str("print", "timelapse", "0"); }
    }
}

void SyncAmsInfoDialog::reset_ams_material()
{
    MaterialHash::iterator iter = m_materialList.begin();
    while (iter != m_materialList.end()) {
        int           id      = iter->first;
        Material *    item    = iter->second;
        MaterialItem *m       = item->item;
        wxString      ams_id  = "-";
        wxColour      ams_col = wxColour(0xEE, 0xEE, 0xEE);
        m->set_ams_info(ams_col, ams_id);
        iter++;
    }
}

void SyncAmsInfoDialog::Enable_Refresh_Button(bool en)
{
    if (!en) {
        if (m_button_refresh->IsEnabled()) { m_button_refresh->Disable(); }
    } else {
        if (!m_button_refresh->IsEnabled()) { m_button_refresh->Enable(); }
    }
}

void SyncAmsInfoDialog::Enable_Send_Button(bool en)
{
    if (!m_button_ensure) { return; }
    if (!en) {
        if (m_button_ensure->IsEnabled()) {
            m_button_ensure->Disable();
            m_button_ensure->SetBackgroundColor(wxColour(0x90, 0x90, 0x90));
            m_button_ensure->SetBorderColor(wxColour(0x90, 0x90, 0x90));
        }
    } else {
        if (!m_button_ensure->IsEnabled()) {
            m_button_ensure->Enable();
            m_button_ensure->SetBackgroundColor(m_btn_bg_enable);
            m_button_ensure->SetBorderColor(m_btn_bg_enable);
        }
    }
}

void SyncAmsInfoDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    rename_editable->msw_rescale();
    rename_editable_light->msw_rescale();
    if (ams_mapping_help_icon != nullptr) {
        ams_mapping_help_icon->msw_rescale();
        if (img_amsmapping_tip) img_amsmapping_tip->SetBitmap(ams_mapping_help_icon->bmp());
    }
    m_button_ensure->SetMinSize(SELECT_MACHINE_DIALOG_BUTTON_SIZE);
    m_button_ensure->SetCornerRadius(FromDIP(12));
    m_status_bar->msw_rescale();

    for (auto material1 : m_materialList) {
        material1.second->item->msw_rescale();
    }

    Fit();
    Refresh();
}

void SyncAmsInfoDialog::set_flow_calibration_state(bool state, bool show_tips)
{
    if (!state) {
        m_checkbox_list["flow_cali"]->setValue(state ? "on" : "off");
        m_checkbox_list["flow_cali"]->Enable();
    } else {
        AppConfig *config = wxGetApp().app_config;
        if (config && config->get("print", "flow_cali") == "0") {
            m_checkbox_list["flow_cali"]->setValue("off");
        } else {
            m_checkbox_list["flow_cali"]->setValue("on");
        }

        m_checkbox_list["flow_cali"]->Enable();
    }
}

void SyncAmsInfoDialog::set_default(bool hide_some)
{
    if (m_print_type == PrintFromType::FROM_NORMAL) {
        bool is_show = true;
        if (!hide_some) {
            if (m_stext_printer_title) { m_stext_printer_title->Show(is_show); }
            if (m_comboBox_printer) { m_comboBox_printer->Show(is_show); }
            if (m_button_refresh) { m_button_refresh->Show(is_show); }
            if (m_rename_normal_panel) { m_rename_normal_panel->Show(is_show); }
            if (m_hyperlink) { m_hyperlink->Show(is_show); }
        }
    } else if (m_print_type == PrintFromType::FROM_SDCARD_VIEW) {
        bool is_show = false;
        if (m_stext_printer_title) { m_stext_printer_title->Show(is_show); }
        if (m_comboBox_printer) { m_comboBox_printer->Show(is_show); }
        if (m_button_refresh) { m_button_refresh->Show(is_show); }
        if (m_rename_normal_panel) { m_rename_normal_panel->Show(is_show); }
        if (m_hyperlink) { m_hyperlink->Show(is_show); }
    }

    // project name
    if (m_rename_switch_panel) { m_rename_switch_panel->SetSelection(0); }

    wxString filename = m_plater->get_export_gcode_filename("", true, m_print_plate_idx == PLATE_ALL_IDX ? true : false);
    if (m_print_plate_idx == PLATE_ALL_IDX && filename.empty()) { filename = _L("Untitled"); }

    if (filename.empty()) {
        filename = m_plater->get_export_gcode_filename("", true);
        if (filename.empty()) filename = _L("Untitled");
    }

    fs::path    filename_path(filename.c_str());
    std::string file_name = filename_path.filename().string();
    if (from_u8(file_name).find(_L("Untitled")) != wxString::npos) {
        PartPlate *part_plate = m_plater->get_partplate_list().get_plate(m_print_plate_idx);
        if (part_plate) {
            if (std::vector<ModelObject *> objects = part_plate->get_objects_on_this_plate(); objects.size() > 0) {
                file_name = objects[0]->name;
                for (int i = 1; i < objects.size(); i++) { file_name += (" + " + objects[i]->name); }
            }
            if (file_name.size() > 100) { file_name = file_name.substr(0, 97) + "..."; }
        }
    }
    m_current_project_name = wxString::FromUTF8(file_name);

    // unsupported character filter
    m_current_project_name = from_u8(filter_characters(m_current_project_name.ToUTF8().data(), "<>[]:/\\|?*\""));
    if (m_rename_text) {
        m_rename_text->SetLabelText(m_current_project_name);
        m_rename_normal_panel->Layout();
    }

    // clear combobox
    m_list.clear();
    if (m_comboBox_printer) { m_comboBox_printer->Clear(); }

    m_printer_last_select = "";
    m_print_info          = "";
    m_comboBox_printer->SetValue(wxEmptyString);
    m_comboBox_printer->Enable();

    // rset status bar
    m_status_bar->reset();

    NetworkAgent *agent = wxGetApp().getAgent();
    if (agent) {
        if (!hide_some) {
            if (agent->is_user_login()) {
                show_status(PrintDialogStatus::PrintStatusInit);
            } else {
                show_status(PrintDialogStatus::PrintStatusNoUserLogin);
            }
        }
    }

    if (m_print_type == PrintFromType::FROM_NORMAL) {
        reset_and_sync_ams_list();
        m_cur_input_thumbnail_data = m_specify_plate_idx == -1 ? m_plater->get_partplate_list().get_curr_plate()->thumbnail_data :
                                                                 m_plater->get_partplate_list().get_plate(m_specify_plate_idx)->thumbnail_data;
        set_default_normal(m_cur_input_thumbnail_data);
    }

    Layout();
    Fit();
}

void SyncAmsInfoDialog::reset_and_sync_ams_list()
{
    // for black list
    std::vector<std::string> materials;
    std::vector<std::string> brands;
    std::vector<std::string> display_materials;
    std::vector<std::string> m_filaments_id;
    auto                     preset_bundle = wxGetApp().preset_bundle;

    for (auto filament_name : preset_bundle->filament_presets) {
        for (int f_index = 0; f_index < preset_bundle->filaments.size(); f_index++) {
            PresetCollection *filament_presets = &wxGetApp().preset_bundle->filaments;
            Preset *          preset           = &filament_presets->preset(f_index);
            int               size             = preset_bundle->filaments.size();
            if (preset && filament_name.compare(preset->name) == 0) {
                std::string display_filament_type;
                std::string filament_type = preset->config.get_filament_type(display_filament_type);
                std::string m_filament_id = preset->filament_id;
                display_materials.push_back(display_filament_type);
                materials.push_back(filament_type);
                m_filaments_id.push_back(m_filament_id);

                std::string m_vendor_name = "";
                auto        vendor        = dynamic_cast<ConfigOptionStrings *>(preset->config.option("filament_vendor"));
                if (vendor && (vendor->values.size() > 0)) {
                    std::string vendor_name = vendor->values[0];
                    m_vendor_name           = vendor_name;
                }
                brands.push_back(m_vendor_name);
            }
        }
    }
    /*std::vector<int> extruders;
    if (m_specify_plate_idx == -1) {
        extruders = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_used_extruders();
    } else {
        extruders = wxGetApp().plater()->get_partplate_list().get_plate(m_specify_plate_idx)->get_extruders();
    }*/
    std::vector<int> extruders(wxGetApp().plater()->get_extruders_colors().size());
    std::iota(extruders.begin(), extruders.end(), 1);
    BitmapCache            bmcache;
    MaterialHash::iterator iter = m_materialList.begin();
    while (iter != m_materialList.end()) {
        int       id   = iter->first;
        Material *item = iter->second;
        item->item->Destroy();
        delete item;
        iter++;
    }

    m_sizer_ams_mapping->Clear();
    m_materialList.clear();
    m_filaments.clear();

    bool use_double_extruder = get_is_double_extruder();
    if (use_double_extruder) {
        const auto &project_config = preset_bundle->project_config;
        m_filaments_map            = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_real_filament_maps(project_config);
    }
    auto contronal_index = 0;
    bool is_first_row    = true;
    for (auto i = 0; i < extruders.size(); i++) {
        auto          extruder = extruders[i] - 1;
        auto          colour   = wxGetApp().preset_bundle->project_config.opt_string("filament_colour", (unsigned int) extruder);
        unsigned char rgb[4];
        bmcache.parse_color4(colour, rgb);

        auto colour_rgb = wxColour((int) rgb[0], (int) rgb[1], (int) rgb[2], (int) rgb[3]);
        if (extruder >= materials.size() || extruder < 0 || extruder >= display_materials.size())
            continue;

        if (contronal_index % SYNC_FLEX_GRID_COL == 0) {
            wxBoxSizer *ams_tip_sizer = new wxBoxSizer(wxVERTICAL);
            if (is_first_row) {
                is_first_row              = false;
                auto tip0_text = new wxStaticText(m_filament_panel, wxID_ANY, _CTX(L_CONTEXT("Original", "Sync_AMS"), "Sync_AMS"));
                tip0_text->SetForegroundColour(wxColour(107, 107, 107, 100));
                ams_tip_sizer->Add(tip0_text, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));

                auto tip1_text = new wxStaticText(m_filament_panel, wxID_ANY, _L("AMS"));
                tip1_text->SetForegroundColour(wxColour(107, 107, 107, 100));
                ams_tip_sizer->Add(tip1_text, 0, wxALIGN_LEFT | wxTOP, FromDIP(6));
            }
            m_sizer_ams_mapping->Add(ams_tip_sizer, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));
            contronal_index++;
        }

        MaterialItem *item = nullptr;
        if (use_double_extruder) {
            if (m_filaments_map[extruder] == 1) {
                item = new MaterialItem(m_filament_panel, colour_rgb, _L(display_materials[extruder]),true);// m_filament_left_panel//special
                m_sizer_ams_mapping->Add(item, 0, wxALL, FromDIP(5));                                   // m_sizer_ams_mapping_left
            } else if (m_filaments_map[extruder] == 2) {
                item = new MaterialItem(m_filament_panel, colour_rgb, _L(display_materials[extruder]), true); // m_filament_right_panel
                m_sizer_ams_mapping->Add(item, 0, wxALL, FromDIP(5));                                   // m_sizer_ams_mapping_right
            }
            else {
                BOOST_LOG_TRIVIAL(error) << "check error:MaterialItem *item = nullptr";
                continue;
            }
        } else {
            item = new MaterialItem(m_filament_panel, colour_rgb, _L(display_materials[extruder]), true);
            m_sizer_ams_mapping->Add(item, 0, wxALL, FromDIP(5));
        }
        contronal_index++;
        item->SetToolTip(_L("Upper half area:  Original\nLower half area:  Filament in AMS\nAnd you can click it to modify"));
        item->Bind(wxEVT_LEFT_UP, [this, item, materials, extruder](wxMouseEvent &e) {});
        item->Bind(wxEVT_LEFT_DOWN, [this, item, materials, extruder](wxMouseEvent &e) {
            MaterialHash::iterator iter = m_materialList.begin();
            while (iter != m_materialList.end()) {
                int           id   = iter->first;
                Material *    item = iter->second;
                MaterialItem *m    = item->item;
                m->on_normal();
                iter++;
            }

            m_current_filament_id = extruder;
            item->on_selected();

            auto    mouse_pos = ClientToScreen(e.GetPosition());
            wxPoint rect      = item->ClientToScreen(wxPoint(0, 0));

            // update ams data
            DeviceManager *dev_manager = Slic3r::GUI::wxGetApp().getDeviceManager();
            if (!dev_manager) return;
            MachineObject *obj_        = dev_manager->get_selected_machine();
            const auto &   full_config = wxGetApp().preset_bundle->full_config();
            size_t         nozzle_nums = full_config.option<ConfigOptionFloatsNullable>("nozzle_diameter")->values.size();
            if (nozzle_nums > 1) {
                m_mapping_popup.set_show_type(ShowType::LEFT_AND_RIGHT);//special
            }
            // m_mapping_popup.set_show_type(ShowType::RIGHT);
            if (obj_ && obj_->is_support_ams_mapping()) {
                if (m_mapping_popup.IsShown())
                    return;
                wxPoint pos = item->ClientToScreen(wxPoint(0, 0));
                pos.y += item->GetRect().height;
                m_mapping_popup.Move(pos);

                if (obj_ && m_checkbox_list["use_ams"]->getValue() == "on" && obj_->dev_id == m_printer_last_select) {
                    m_mapping_popup.set_parent_item(item);
                    m_mapping_popup.set_current_filament_id(extruder);
                    m_mapping_popup.set_tag_texture(materials[extruder]);
                    m_mapping_popup.set_send_win(this);
                    m_mapping_popup.update(obj_);
                    m_mapping_popup.Popup();
                }
            }
        });

        Material *material_item = new Material();
        material_item->id       = extruder;
        material_item->item     = item;
        m_materialList[i]       = material_item;

        // build for ams mapping
        if (extruder < materials.size() && extruder >= 0) {
            FilamentInfo info;
            info.id          = extruder;
            info.type        = materials[extruder];
            info.brand       = brands[extruder];
            info.filament_id = m_filaments_id[extruder];
            info.color       = wxString::Format("#%02X%02X%02X%02X", colour_rgb.Red(), colour_rgb.Green(), colour_rgb.Blue(), colour_rgb.Alpha()).ToStdString();
            m_filaments.push_back(info);
        }
    }
    if (use_double_extruder) {
        //m_filament_left_panel->Show();//SyncAmsInfoDialog::reset_and_sync_ams_list()
        //m_filament_right_panel->Show();
        m_filament_panel->Show(); // SyncAmsInfoDialog::reset_and_sync_ams_list()//special
        m_sizer_ams_mapping_left->SetCols(4);
        m_sizer_ams_mapping_left->Layout();
        m_filament_panel_left_sizer->Layout();
        m_filament_left_panel->Layout();

        m_sizer_ams_mapping_right->SetCols(4);
        m_sizer_ams_mapping_right->Layout();
        m_filament_panel_right_sizer->Layout();
        m_filament_right_panel->Layout();
    } else {
        //m_filament_left_panel->Hide();//SyncAmsInfoDialog::reset_and_sync_ams_list()
        //m_filament_right_panel->Hide();
        m_filament_panel->Show();//SyncAmsInfoDialog::reset_and_sync_ams_list()
        m_sizer_ams_mapping->SetCols(SYNC_FLEX_GRID_COL);
        m_sizer_ams_mapping->Layout();
        m_filament_panel_sizer->Layout();
    }
    // reset_ams_material();//show "-"
}

void SyncAmsInfoDialog::clone_thumbnail_data()
{
    // record preview_colors
    MaterialHash::iterator iter = m_materialList.begin();
    if (m_preview_colors_in_thumbnail.size() != m_materialList.size()) { m_preview_colors_in_thumbnail.resize(m_materialList.size()); }
    while (iter != m_materialList.end()) {
        int           id                  = iter->first;
        Material *    item                = iter->second;
        MaterialItem *m                   = item->item;
        m_preview_colors_in_thumbnail[id] = m->m_material_coloul;
        if (item->id < m_cur_colors_in_thumbnail.size()) {
            m_cur_colors_in_thumbnail[item->id] = m->m_ams_coloul;
        } else { // exist empty or unrecognized type ams in machine
            m_cur_colors_in_thumbnail.resize(item->id + 1);
            m_cur_colors_in_thumbnail[item->id] = m->m_ams_coloul;
        }
        iter++;
    }
    // copy data
    auto &data = m_cur_input_thumbnail_data;
    m_preview_thumbnail_data.reset();
    m_preview_thumbnail_data.set(data.width, data.height);
    if (data.width > 0 && data.height > 0) {
        for (unsigned int r = 0; r < data.height; ++r) {
            unsigned int rr = (data.height - 1 - r) * data.width;
            for (unsigned int c = 0; c < data.width; ++c) {
                unsigned char *origin_px = (unsigned char *) data.pixels.data() + 4 * (rr + c);
                unsigned char *new_px    = (unsigned char *) m_preview_thumbnail_data.pixels.data() + 4 * (rr + c);
                for (size_t i = 0; i < 4; i++) { new_px[i] = origin_px[i]; }
            }
        }
    }
    // record_edge_pixels_data
    record_edge_pixels_data();
}

void SyncAmsInfoDialog::record_edge_pixels_data()
{
    auto is_not_in_preview_colors = [this](unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        for (size_t i = 0; i < m_preview_colors_in_thumbnail.size(); i++) {
            wxColour render_color = adjust_color_for_render(m_preview_colors_in_thumbnail[i]);
            if (render_color.Red() == r && render_color.Green() == g && render_color.Blue() == b /*&& render_color.Alpha() == a*/) { return false; }
        }
        return true;
    };
    ThumbnailData &data        = m_cur_no_light_thumbnail_data;
    ThumbnailData &origin_data = m_cur_input_thumbnail_data;
    if (data.width > 0 && data.height > 0) {
        m_edge_pixels.resize(data.width * data.height);
        for (unsigned int r = 0; r < data.height; ++r) {
            unsigned int rr = (data.height - 1 - r) * data.width;
            for (unsigned int c = 0; c < data.width; ++c) {
                unsigned char *no_light_px        = (unsigned char *) data.pixels.data() + 4 * (rr + c);
                unsigned char *origin_px          = (unsigned char *) origin_data.pixels.data() + 4 * (rr + c);
                m_edge_pixels[r * data.width + c] = false;
                if (origin_px[3] > 0) {
                    if (is_not_in_preview_colors(no_light_px[0], no_light_px[1], no_light_px[2], origin_px[3])) { m_edge_pixels[r * data.width + c] = true; }
                }
            }
        }
    }
}

wxColour SyncAmsInfoDialog::adjust_color_for_render(const wxColour &color)
{
    std::array<float, 4> _temp_color_color  = {color.Red() / 255.0f, color.Green() / 255.0f, color.Blue() / 255.0f, color.Alpha() / 255.0f};
    auto                 _temp_color_color_ = adjust_color_for_rendering(_temp_color_color);
    wxColour             render_color((int) (_temp_color_color_[0] * 255.0f), (int) (_temp_color_color_[1] * 255.0f), (int) (_temp_color_color_[2] * 255.0f),
                          (int) (_temp_color_color_[3] * 255.0f));
    return render_color;
}

void SyncAmsInfoDialog::final_deal_edge_pixels_data(ThumbnailData &data)
{
    if (data.width > 0 && data.height > 0 && m_edge_pixels.size() > 0) {
        for (unsigned int r = 0; r < data.height; ++r) {
            unsigned int rr            = (data.height - 1 - r) * data.width;
            bool         exist_rr_up   = r >= 1 ? true : false;
            bool         exist_rr_down = r <= data.height - 2 ? true : false;
            unsigned int rr_up         = exist_rr_up ? (data.height - 1 - (r - 1)) * data.width : 0;
            unsigned int rr_down       = exist_rr_down ? (data.height - 1 - (r + 1)) * data.width : 0;
            for (unsigned int c = 0; c < data.width; ++c) {
                bool           exist_c_left      = c >= 1 ? true : false;
                bool           exist_c_right     = c <= data.width - 2 ? true : false;
                unsigned int   c_left            = exist_c_left ? c - 1 : 0;
                unsigned int   c_right           = exist_c_right ? c + 1 : 0;
                unsigned char *cur_px            = (unsigned char *) data.pixels.data() + 4 * (rr + c);
                unsigned char *relational_pxs[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
                if (exist_rr_up && exist_c_left) { relational_pxs[0] = (unsigned char *) data.pixels.data() + 4 * (rr_up + c_left); }
                if (exist_rr_up) { relational_pxs[1] = (unsigned char *) data.pixels.data() + 4 * (rr_up + c); }
                if (exist_rr_up && exist_c_right) { relational_pxs[2] = (unsigned char *) data.pixels.data() + 4 * (rr_up + c_right); }
                if (exist_c_left) { relational_pxs[3] = (unsigned char *) data.pixels.data() + 4 * (rr + c_left); }
                if (exist_c_right) { relational_pxs[4] = (unsigned char *) data.pixels.data() + 4 * (rr + c_right); }
                if (exist_rr_down && exist_c_left) { relational_pxs[5] = (unsigned char *) data.pixels.data() + 4 * (rr_down + c_left); }
                if (exist_rr_down) { relational_pxs[6] = (unsigned char *) data.pixels.data() + 4 * (rr_down + c); }
                if (exist_rr_down && exist_c_right) { relational_pxs[7] = (unsigned char *) data.pixels.data() + 4 * (rr_down + c_right); }
                if (cur_px[3] > 0 && m_edge_pixels[r * data.width + c]) {
                    int rgba_sum[4] = {0, 0, 0, 0};
                    int valid_count = 0;
                    for (size_t k = 0; k < 8; k++) {
                        if (relational_pxs[k]) {
                            if (k == 0 && m_edge_pixels[(r - 1) * data.width + c_left]) { continue; }
                            if (k == 1 && m_edge_pixels[(r - 1) * data.width + c]) { continue; }
                            if (k == 2 && m_edge_pixels[(r - 1) * data.width + c_right]) { continue; }
                            if (k == 3 && m_edge_pixels[r * data.width + c_left]) { continue; }
                            if (k == 4 && m_edge_pixels[r * data.width + c_right]) { continue; }
                            if (k == 5 && m_edge_pixels[(r + 1) * data.width + c_left]) { continue; }
                            if (k == 6 && m_edge_pixels[(r + 1) * data.width + c]) { continue; }
                            if (k == 7 && m_edge_pixels[(r + 1) * data.width + c_right]) { continue; }
                            for (size_t m = 0; m < 4; m++) { rgba_sum[m] += relational_pxs[k][m]; }
                            valid_count++;
                        }
                    }
                    if (valid_count > 0) {
                        for (size_t m = 0; m < 4; m++) { cur_px[m] = std::clamp(int(rgba_sum[m] / (float) valid_count), 0, 255); }
                    }
                }
            }
        }
    }
}

void SyncAmsInfoDialog::updata_thumbnail_data_after_connected_printer()
{
    updata_ui_data_after_connected_printer();
    // change thumbnail_data
    ThumbnailData &input_data    = m_specify_plate_idx == -1 ? m_plater->get_partplate_list().get_curr_plate()->thumbnail_data :
                                                               m_plater->get_partplate_list().get_plate(m_specify_plate_idx)->thumbnail_data;
    ThumbnailData &no_light_data = m_specify_plate_idx == -1 ? m_plater->get_partplate_list().get_curr_plate()->no_light_thumbnail_data :
                                                               m_plater->get_partplate_list().get_plate(m_specify_plate_idx)->no_light_thumbnail_data;
    if (input_data.width == 0 || input_data.height == 0 || no_light_data.width == 0 || no_light_data.height == 0) { wxGetApp().plater()->update_all_plate_thumbnails(false); }
    unify_deal_thumbnail_data(input_data, no_light_data);
}

void SyncAmsInfoDialog::unify_deal_thumbnail_data(ThumbnailData &input_data, ThumbnailData &no_light_data)
{
    if (input_data.width == 0 || input_data.height == 0 || no_light_data.width == 0 || no_light_data.height == 0) {
        BOOST_LOG_TRIVIAL(error) << "SyncAmsInfoDialog::no_light_data is empty,error";
        return;
    }
    m_cur_input_thumbnail_data    = input_data;
    m_cur_no_light_thumbnail_data = no_light_data;
    clone_thumbnail_data();
    MaterialHash::iterator iter               = m_materialList.begin();
    bool                   is_connect_printer = true;
    while (iter != m_materialList.end()) {
        int           id   = iter->first;
        Material *    item = iter->second;
        MaterialItem *m    = item->item;
        if (m->m_ams_name == "-") {
            is_connect_printer = false;
            break;
        }
        iter++;
    }
    if (is_connect_printer) {
        change_default_normal(-1, wxColour());
        final_deal_edge_pixels_data(m_preview_thumbnail_data);
        set_default_normal(m_preview_thumbnail_data);
    }
}

void SyncAmsInfoDialog::change_default_normal(int old_filament_id, wxColour temp_ams_color)
{
    if (m_cur_colors_in_thumbnail.size() == 0) {
        BOOST_LOG_TRIVIAL(error) << "SyncAmsInfoDialog::change_default_normal:error:m_cur_colors_in_thumbnail.size() == 0";
        return;
    }
    if (old_filament_id >= 0) {
        if (old_filament_id < m_cur_colors_in_thumbnail.size()) {
            m_cur_colors_in_thumbnail[old_filament_id] = temp_ams_color;
        } else {
            BOOST_LOG_TRIVIAL(error) << "SyncAmsInfoDialog::change_default_normal:error:old_filament_id > m_cur_colors_in_thumbnail.size()";
            return;
        }
    }
    ThumbnailData &data          = m_cur_input_thumbnail_data;
    ThumbnailData &no_light_data = m_cur_no_light_thumbnail_data;
    if (data.width > 0 && data.height > 0 && data.width == no_light_data.width && data.height == no_light_data.height) {
        for (unsigned int r = 0; r < data.height; ++r) {
            unsigned int rr = (data.height - 1 - r) * data.width;
            for (unsigned int c = 0; c < data.width; ++c) {
                unsigned char *no_light_px = (unsigned char *) no_light_data.pixels.data() + 4 * (rr + c);
                unsigned char *origin_px   = (unsigned char *) data.pixels.data() + 4 * (rr + c);
                unsigned char *new_px      = (unsigned char *) m_preview_thumbnail_data.pixels.data() + 4 * (rr + c);
                if (origin_px[3] > 0 && m_edge_pixels[r * data.width + c] == false) {
                    auto filament_id = 255 - no_light_px[3];
                    if (filament_id >= m_cur_colors_in_thumbnail.size()) { continue; }
                    wxColour temp_ams_color_in_loop = m_cur_colors_in_thumbnail[filament_id];
                    wxColour ams_color              = adjust_color_for_render(temp_ams_color_in_loop);
                    // change color
                    new_px[3]                     = origin_px[3]; // alpha
                    int           origin_rgb      = origin_px[0] + origin_px[1] + origin_px[2];
                    int           no_light_px_rgb = no_light_px[0] + no_light_px[1] + no_light_px[2];
                    unsigned char i               = 0;
                    if (origin_rgb >= no_light_px_rgb) { // Brighten up
                        unsigned char cur_single_color = ams_color.Red();
                        new_px[i]                      = std::clamp(cur_single_color + (origin_px[i] - no_light_px[i]), 0, 255);
                        i++;
                        cur_single_color = ams_color.Green();
                        new_px[i]        = std::clamp(cur_single_color + (origin_px[i] - no_light_px[i]), 0, 255);
                        i++;
                        cur_single_color = ams_color.Blue();
                        new_px[i]        = std::clamp(cur_single_color + (origin_px[i] - no_light_px[i]), 0, 255);
                    } else { // Dimming
                        float         ratio            = origin_rgb / (float) no_light_px_rgb;
                        unsigned char cur_single_color = ams_color.Red();
                        new_px[i]                      = std::clamp((int) (cur_single_color * ratio), 0, 255);
                        i++;
                        cur_single_color = ams_color.Green();
                        new_px[i]        = std::clamp((int) (cur_single_color * ratio), 0, 255);
                        i++;
                        cur_single_color = ams_color.Blue();
                        new_px[i]        = std::clamp((int) (cur_single_color * ratio), 0, 255);
                    }
                }
            }
        }
    } else {
        BOOST_LOG_TRIVIAL(error) << "SyncAmsInfoDialog::change_defa:no_light_data is empty,error";
    }
}

void SyncAmsInfoDialog::update_page_turn_state(bool show)
{
    /* m_bitmap_last_plate->Show(show);
     m_bitmap_next_plate->Show(show);

     if (show) {
         if (m_print_plate_idx <= 0) { m_bitmap_last_plate->Hide(); }
         else { m_bitmap_last_plate->Show(); }

         if ((m_print_plate_idx + 1) >= m_print_plate_total) { m_bitmap_next_plate->Hide(); }
         else { m_bitmap_next_plate->Show(); }

         if (m_print_plate_total == 1) {
             m_bitmap_last_plate->Show(false);
             m_bitmap_next_plate->Show(false);
         }
     }*/
}

void SyncAmsInfoDialog::sys_color_changed()
{
    if (wxGetApp().dark_mode()) {
        // rename_button->SetIcon("ams_editable_light");
        m_rename_button->SetBitmap(rename_editable_light->bmp());

    } else {
        m_rename_button->SetBitmap(rename_editable->bmp());
    }
    m_rename_button->Refresh();
}



wxString SyncAmsInfoDialog::format_bed_name(std::string plate_name)
{
    wxString name;
    if (plate_name == "Cool Plate") {
        name = _L("Cool");
        m_bed_image->SetBitmap(create_scaled_bitmap("bed_cool", this, 32));
    } else if (plate_name == "Engineering Plate") {
        name = _L("Engineering");
        m_bed_image->SetBitmap(create_scaled_bitmap("bed_engineering", this, 32));
    } else if (plate_name == "High Temp Plate") {
        name = _L("High Temp");
        m_bed_image->SetBitmap(create_scaled_bitmap("bed_high_templ", this, 32));
    } else if (plate_name == "Textured PEI Plate") {
        name = "PEI";
        m_bed_image->SetBitmap(create_scaled_bitmap("bed_pei", this, 32));
    } else if (plate_name == "Supertack Plate") {
        name = _L("Cool(Supertack)");
        m_bed_image->SetBitmap(create_scaled_bitmap("bed_cool_supertack", this, 32));
    }
    return name;
}

SyncAmsInfoDialog::~SyncAmsInfoDialog() {
    if (m_refresh_timer) {
        delete m_refresh_timer;
    }
}

void SyncAmsInfoDialog::update_lan_machine_list()
{
    DeviceManager *dev = wxGetApp().getDeviceManager();
    if (!dev) return;
    auto m_free_machine_list = dev->get_local_machine_list();

    BOOST_LOG_TRIVIAL(trace) << "SelectMachinePopup update_other_devices start";

    for (auto &elem : m_free_machine_list) {
        MachineObject *mobj = elem.second;

        /* do not show printer bind state is empty */
        if (!mobj->is_avaliable()) continue;
        if (!mobj->is_online()) continue;
        if (!mobj->is_lan_mode_printer()) continue;

        if (mobj->has_access_right()) {
            auto b = mobj->dev_name;

            // clear machine list

            // m_comboBox_printer->Clear();
            std::vector<std::string>               machine_list;
            wxArrayString                          machine_list_name;
            std::map<std::string, MachineObject *> option_list;
        }
    }
    BOOST_LOG_TRIVIAL(trace) << "SyncAmsInfoDialog update_lan_devices end";
}

std::string SyncAmsInfoDialog::get_print_status_info(PrintDialogStatus status)
{
    switch (status) {
    case PrintStatusInit: return "PrintStatusInit";
    case PrintStatusNoUserLogin: return "PrintStatusNoUserLogin";
    case PrintStatusInvalidPrinter: return "PrintStatusInvalidPrinter";
    case PrintStatusConnectingServer: return "PrintStatusConnectingServer";
    case PrintStatusReading: return "PrintStatusReading";
    case PrintStatusReadingFinished: return "PrintStatusReadingFinished";
    case PrintStatusReadingTimeout: return "PrintStatusReadingTimeout";
    case PrintStatusInUpgrading: return "PrintStatusInUpgrading";
    case PrintStatusNeedUpgradingAms: return "PrintStatusNeedUpgradingAms";
    case PrintStatusInSystemPrinting: return "PrintStatusInSystemPrinting";
    case PrintStatusInPrinting: return "PrintStatusInPrinting";
    case PrintStatusDisableAms: return "PrintStatusDisableAms";
    case PrintStatusAmsMappingSuccess: return "PrintStatusAmsMappingSuccess";
    case PrintStatusAmsMappingInvalid: return "PrintStatusAmsMappingInvalid";
    case PrintStatusAmsMappingU0Invalid: return "PrintStatusAmsMappingU0Invalid";
    case PrintStatusAmsMappingValid: return "PrintStatusAmsMappingValid";
    case PrintStatusAmsMappingByOrder: return "PrintStatusAmsMappingByOrder";
    case PrintStatusRefreshingMachineList: return "PrintStatusRefreshingMachineList";
    case PrintStatusSending: return "PrintStatusSending";
    case PrintStatusSendingCanceled: return "PrintStatusSendingCanceled";
    case PrintStatusLanModeNoSdcard: return "PrintStatusLanModeNoSdcard";
    case PrintStatusLanModeSDcardNotAvailable: return "PrintStatusLanModeSDcardNotAvailable";
    case PrintStatusNoSdcard: return "PrintStatusNoSdcard";
    case PrintStatusUnsupportedPrinter: return "PrintStatusUnsupportedPrinter";
    case PrintStatusTimelapseNoSdcard: return "PrintStatusTimelapseNoSdcard";
    case PrintStatusNotSupportedPrintAll: return "PrintStatusNotSupportedPrintAll";
    }
    return "unknown";
}

SyncNozzleAndAmsDialog::SyncNozzleAndAmsDialog(wxWindow *parent, InputInfo &input_info)
    : DPIFrame(static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, !wxCAPTION | !wxCLOSE_BOX | wxBORDER_NONE)
    , m_input_info(input_info)
{
    //SetBackgroundStyle(wxBackgroundStyle::wxBG_STYLE_TRANSPARENT);
    SetTransparent(220);
    SetBackgroundColour(wxColour(23, 25, 22, 128));
    auto win_width = 288;
    SetMinSize(wxSize(FromDIP(win_width), -1));
    SetMaxSize(wxSize(FromDIP(win_width), -1));
    SetPosition(m_input_info.dialog_pos);

    m_sizer_main              = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *text_sizer = new wxBoxSizer(wxHORIZONTAL);
    text_sizer->AddSpacer(FromDIP(20));
    auto image_sizer= new wxBoxSizer(wxVERTICAL);
    auto        imgsize       = FromDIP(25);
    auto completedimg = new wxStaticBitmap(this, wxID_ANY, create_scaled_bitmap("completed", this, 25), wxDefaultPosition, wxSize(imgsize, imgsize), 0);
    image_sizer->Add(completedimg, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(0));
    image_sizer->AddStretchSpacer();
    text_sizer->Add(image_sizer);
    text_sizer->AddSpacer(FromDIP(5));
    auto finish_text = new wxStaticText(this, wxID_ANY, _L("Successfully synchronized nozzle and AMS number information."));
    finish_text->Wrap(win_width - 40);
    finish_text->SetForegroundColour(wxColour(255, 255, 255, 255));
    text_sizer->Add(finish_text, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 0);
    text_sizer->AddSpacer(FromDIP(20));
    m_sizer_main->Add(text_sizer, FromDIP(0), wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxTOP, FromDIP(20));

    wxBoxSizer *bSizer_button = new wxBoxSizer(wxHORIZONTAL);
    bSizer_button->SetMinSize(wxSize(FromDIP(100), -1));
    /* m_checkbox = new wxCheckBox(this, wxID_ANY, _L("Don't show again"), wxDefaultPosition, wxDefaultSize, 0);
     bSizer_button->Add(m_checkbox, 0, wxALIGN_LEFT);*/
    bSizer_button->AddStretchSpacer(1);
    StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(27, 136, 68), StateColor::Pressed), std::pair<wxColour, int>(wxColour(61, 203, 115), StateColor::Hovered),
                            std::pair<wxColour, int>(AMS_CONTROL_BRAND_COLOUR, StateColor::Normal));
    StateColor btn_bg_white(std::pair<wxColour, int>(wxColour(23, 25, 22), StateColor::Pressed), std::pair<wxColour, int>(wxColour(43, 45, 42), StateColor::Hovered),
                            std::pair<wxColour, int>(wxColour(23, 25, 22), StateColor::Normal));
    m_button_ok = new Button(this, m_input_info.only_external_material ? _L("Sync filament") : _L("Sync AMS filament"));
    m_button_ok->SetBackgroundColor(btn_bg_green);
    m_button_ok->SetBorderWidth(0);
    m_button_ok->SetTextColor(wxColour(0xFEFEFE));
    m_button_ok->SetFont(Label::Body_12);
    m_button_ok->SetSize(wxSize(FromDIP(60), FromDIP(30)));
    m_button_ok->SetMinSize(wxSize(FromDIP(90), FromDIP(30)));
    m_button_ok->SetCornerRadius(FromDIP(6));
    bSizer_button->Add(m_button_ok, 0, wxALIGN_RIGHT | wxLEFT | wxTOP, FromDIP(10));

    m_button_ok->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        deal_ok();
    });


    m_button_cancel = new Button(this, _CTX(L_CONTEXT("Cancel", "Sync_Nozzle_AMS"), "Sync_Nozzle_AMS"));
    m_button_cancel->SetBackgroundColor(btn_bg_white);
    m_button_cancel->SetBorderColor(wxColour(93, 93, 91));
    m_button_cancel->SetFont(Label::Body_12);
    m_button_cancel->SetTextColor(wxColour(0xFEFEFE));
    m_button_cancel->SetSize(wxSize(FromDIP(65), FromDIP(30)));
    m_button_cancel->SetMinSize(wxSize(FromDIP(65), FromDIP(30)));
    m_button_cancel->SetCornerRadius(FromDIP(6));
    bSizer_button->Add(m_button_cancel, 0, wxALIGN_RIGHT | wxLEFT | wxTOP, FromDIP(10));

    m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        deal_cancel();
        });

    m_sizer_main->Add(bSizer_button, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));

    SetSizer(m_sizer_main);
    Layout();
    Fit();
}

SyncNozzleAndAmsDialog::~SyncNozzleAndAmsDialog() {}

void SyncNozzleAndAmsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    m_button_ok->Rescale();
    m_button_cancel->Rescale();
}
void SyncNozzleAndAmsDialog::deal_ok() {
    //SyncAmsInfoDialog::SyncInfo temp_info;
    //temp_info.connected_printer = true;
    //SyncAmsInfoDialog sync_dlg(this, temp_info);
    //auto dlg_res = wxGetApp().plater()->sidebar().pop_sync_ams_info_dialog(sync_dlg);
    on_hide();
    wxGetApp().plater()->sidebar().sync_ams_list(true);
}

void SyncNozzleAndAmsDialog::deal_cancel() {
    on_hide();
}

void SyncNozzleAndAmsDialog::on_hide()
{
    this->Hide();
    if (wxGetApp().mainframe != nullptr) {
        wxGetApp().mainframe->Show();
        wxGetApp().mainframe->Raise();
    }
}

FinishSyncAmsDialog::FinishSyncAmsDialog(wxWindow *parent, InputInfo &input_info)
    : DPIFrame(static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, !wxCAPTION | !wxCLOSE_BOX | wxBORDER_NONE)
    , m_input_info(input_info)
{
    // SetBackgroundStyle(wxBackgroundStyle::wxBG_STYLE_TRANSPARENT);
    SetTransparent(200);
    SetBackgroundColour(wxColour(23, 25, 22, 128));
    auto win_width = 288;
    SetMinSize(wxSize(FromDIP(win_width), -1));
    SetMaxSize(wxSize(FromDIP(win_width), -1));
    SetPosition(m_input_info.dialog_pos);

    m_sizer_main           = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *text_sizer = new wxBoxSizer(wxHORIZONTAL);
    text_sizer->AddSpacer(FromDIP(20));
    auto image_sizer  = new wxBoxSizer(wxVERTICAL);
    auto imgsize      = FromDIP(25);
    auto completedimg = new wxStaticBitmap(this, wxID_ANY, create_scaled_bitmap("completed", this, 25), wxDefaultPosition, wxSize(imgsize, imgsize), 0);
    image_sizer->Add(completedimg, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(0));
    image_sizer->AddStretchSpacer();
    text_sizer->Add(image_sizer);
    text_sizer->AddSpacer(FromDIP(5));
    auto finish_text = new wxStaticText(this, wxID_ANY, _L("Successfully synchronized color and type of filament from printer."));
    finish_text->Wrap(win_width - 40);
    finish_text->SetForegroundColour(wxColour(255, 255, 255, 255));
    text_sizer->Add(finish_text, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 0);
    text_sizer->AddSpacer(FromDIP(20));
    m_sizer_main->Add(text_sizer, FromDIP(0), wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxTOP, FromDIP(20));

    wxBoxSizer *bSizer_button = new wxBoxSizer(wxHORIZONTAL);
    bSizer_button->SetMinSize(wxSize(FromDIP(100), -1));
    /* m_checkbox = new wxCheckBox(this, wxID_ANY, _L("Don't show again"), wxDefaultPosition, wxDefaultSize, 0);
     bSizer_button->Add(m_checkbox, 0, wxALIGN_LEFT);*/
    bSizer_button->AddStretchSpacer(1);
    StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(27, 136, 68), StateColor::Pressed), std::pair<wxColour, int>(wxColour(61, 203, 115), StateColor::Hovered),
                            std::pair<wxColour, int>(AMS_CONTROL_BRAND_COLOUR, StateColor::Normal));
    StateColor btn_bg_white(std::pair<wxColour, int>(wxColour(206, 206, 206), StateColor::Pressed), std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Hovered),
                            std::pair<wxColour, int>(*wxWHITE, StateColor::Normal));
    m_button_ok = new Button(this, _CTX(L_CONTEXT("OK", "FinishSyncAms"), "FinishSyncAms"));
    m_button_ok->SetBackgroundColor(btn_bg_green);
    m_button_ok->SetBorderWidth(0);
    m_button_ok->SetTextColor(wxColour(0xFEFEFE));
    m_button_ok->SetFont(Label::Body_12);
    m_button_ok->SetSize(wxSize(FromDIP(68), FromDIP(30)));
    m_button_ok->SetMinSize(wxSize(FromDIP(68), FromDIP(30)));
    m_button_ok->SetCornerRadius(FromDIP(6));
    bSizer_button->Add(m_button_ok, 0, wxALIGN_RIGHT | wxLEFT | wxTOP, FromDIP(10));

    m_button_ok->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        deal_ok();
    });

    //m_button_cancel = new Button(this, _CTX(L_CONTEXT("Cancel", "Sync_Nozzle_AMS"), "Sync_Nozzle_AMS"));
    //m_button_cancel->SetBackgroundColor(btn_bg_white);
    //m_button_cancel->SetBorderColor(wxColour(38, 46, 48));
    //m_button_cancel->SetFont(Label::Body_12);
    //m_button_cancel->SetSize(wxSize(FromDIP(65), FromDIP(30)));
    //m_button_cancel->SetMinSize(wxSize(FromDIP(65), FromDIP(30)));
    //m_button_cancel->SetCornerRadius(FromDIP(6));

    //m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { EndModal(wxID_CANCEL); });
    //bSizer_button->Add(m_button_cancel, 0, wxALIGN_RIGHT | wxLEFT | wxTOP, FromDIP(10));
    m_sizer_main->Add(bSizer_button, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));

    SetSizer(m_sizer_main);
    Layout();
    Fit();
}
FinishSyncAmsDialog::~FinishSyncAmsDialog() {}
void FinishSyncAmsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    m_button_ok->Rescale();
}
void FinishSyncAmsDialog::deal_ok() { on_hide(); }
void FinishSyncAmsDialog::deal_cancel() {}
void FinishSyncAmsDialog::on_hide()
{
    this->Hide();
    if (wxGetApp().mainframe != nullptr) {
        wxGetApp().mainframe->Show();
        wxGetApp().mainframe->Raise();
    }
}
}} // namespace Slic3r