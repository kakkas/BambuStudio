#ifndef _SyncAmsInfo_DIALOG_H_
#define _SyncAmsInfo_DIALOG_H_

#include <future>
#include <thread>
#include "GUI_App.hpp"
#include "GUI_Utils.hpp"
#include "Widgets/Button.hpp"
#include "SelectMachine.hpp"
#include "DeviceManager.hpp"
class Button;

namespace Slic3r { namespace GUI {
class SyncAmsInfoDialog : public DPIDialog
{
    int               m_current_filament_id{0};
    int               m_print_plate_idx{0};
    int               m_print_plate_total{0};
    int               m_timeout_count{0};
    int               m_print_error_code{0};
    bool              m_is_in_sending_mode{false};
    bool              m_ams_mapping_res{false};
    bool              m_ams_mapping_valid{false};
    bool              m_export_3mf_cancel{false};
    bool              m_is_canceled{false};
    bool              m_is_rename_mode{false};
    bool              m_check_flag{false};
    PrintPageMode     m_print_page_mode{PrintPageMode::PrintPageModePrepare};
    std::string       m_print_error_msg;
    std::string       m_print_error_extra;
    std::string       m_printer_last_select;
    std::string       m_print_info;
    wxString          m_current_project_name;
    PrintDialogStatus m_print_status{PrintStatusInit};
    wxColour          m_colour_def_color{wxColour(255, 255, 255)};
    wxColour          m_colour_bold_color{wxColour(38, 46, 48)};
    StateColor        m_btn_bg_enable;
    Label *           m_text_bed_type;

    std::shared_ptr<int>                 m_token = std::make_shared<int>(0);
    std::map<std::string, PrintOption *> m_checkbox_list;
    std::vector<wxString>                m_bedtype_list;
    std::vector<MachineObject *>         m_list;
    std::vector<FilamentInfo>            m_filaments;
    std::vector<FilamentInfo>            m_ams_mapping_result;
    std::vector<int>                     m_filaments_map;
    std::shared_ptr<BBLStatusBarSend>    m_status_bar;

    // SendModeSwitchButton*               m_mode_print {nullptr};
    // SendModeSwitchButton*               m_mode_send {nullptr};
    wxStaticBitmap *m_printer_image{nullptr};
    wxStaticBitmap *m_bed_image{nullptr};

    Slic3r::DynamicPrintConfig m_required_data_config;
    Slic3r::Model              m_required_data_model;
    Slic3r::PlateDataPtrs      m_required_data_plate_data_list;
    std::string                m_required_data_file_name;
    std::string                m_required_data_file_path;

    std::vector<POItem> ops_auto;
    std::vector<POItem> ops_no_auto;

protected:
    PrintFromType     m_print_type{FROM_NORMAL};
    AmsMapingPopup    m_mapping_popup{nullptr};
    AmsMapingTipPopup m_mapping_tip_popup{nullptr};
    AmsTutorialPopup  m_mapping_tutorial_popup{nullptr};
    MaterialHash      m_materialList;
    Plater *          m_plater{nullptr};
    wxPanel *         m_options_other{nullptr};
    wxBoxSizer *      m_sizer_options_timelapse{nullptr};
    wxBoxSizer *      m_sizer_options_other{nullptr};
    wxBoxSizer *      m_sizer_thumbnail{nullptr};

    wxBoxSizer *              m_sizer_main{nullptr};
    wxBoxSizer *              m_basicl_sizer{nullptr};
    wxBoxSizer *              rename_sizer_v{nullptr};
    wxBoxSizer *              rename_sizer_h{nullptr};
    wxBoxSizer *              m_sizer_autorefill{nullptr};
    ScalableButton *          m_button_refresh{nullptr};
    Button *                  m_button_ensure{nullptr};
    wxStaticBitmap *          m_rename_button{nullptr};
    ComboBox *                m_comboBox_printer{nullptr};
    wxStaticBitmap *          m_staticbitmap{nullptr};
    wxStaticBitmap *          m_bitmap_last_plate{nullptr};
    wxStaticBitmap *          m_bitmap_next_plate{nullptr};
    wxStaticBitmap *          img_amsmapping_tip{nullptr};
    ThumbnailPanel *          m_thumbnailPanel{nullptr};
    wxPanel *                 m_panel_status{nullptr};
    wxPanel *                 m_basic_panel;
    wxPanel *                 m_rename_normal_panel{nullptr};
    wxPanel *                 m_panel_sending{nullptr};
    wxPanel *                 m_panel_prepare{nullptr};
    wxPanel *                 m_panel_finish{nullptr};
    wxPanel *                 m_line_top{nullptr};
    Label *                   m_st_txt_error_code{nullptr};
    Label *                   m_st_txt_error_desc{nullptr};
    Label *                   m_st_txt_extra_info{nullptr};
    Label *                   m_ams_backup_tip{nullptr};
    wxHyperlinkCtrl *         m_link_network_state{nullptr};
    wxSimplebook *            m_rename_switch_panel{nullptr};
    wxSimplebook *            m_simplebook{nullptr};
    wxStaticText *            m_rename_text{nullptr};
    Label *                   m_stext_printer_title{nullptr};
    Label *                   m_statictext_ams_msg{nullptr};
    Label *                   m_text_printer_msg{nullptr};
    wxStaticText *            m_staticText_bed_title{nullptr};
    wxStaticText *            m_stext_sending{nullptr};
    wxStaticText *            m_statictext_finish{nullptr};
    TextInput *               m_rename_input{nullptr};
    wxTimer *                 m_refresh_timer{nullptr};
    std::shared_ptr<PrintJob> m_print_job;
    wxScrolledWindow *        m_sw_print_failed_info{nullptr};
    wxHyperlinkCtrl *         m_hyperlink{nullptr};
    wxStaticBitmap *          m_advanced_options_icon{nullptr};
    ScalableBitmap *          rename_editable{nullptr};
    ScalableBitmap *          rename_editable_light{nullptr};
    ScalableBitmap *          ams_mapping_help_icon{nullptr};
    wxStaticBitmap *          img_ams_backup{nullptr};
    ThumbnailData             m_cur_input_thumbnail_data;
    ThumbnailData             m_cur_no_light_thumbnail_data;
    ThumbnailData             m_preview_thumbnail_data; // when ams map change
    std::vector<wxColour>     m_preview_colors_in_thumbnail;
    std::vector<wxColour>     m_cur_colors_in_thumbnail;
    std::vector<bool>         m_edge_pixels;

    StaticBox *m_filament_panel;
    StaticBox *m_filament_left_panel;
    StaticBox *m_filament_right_panel;

    wxBoxSizer *m_filament_panel_sizer;
    wxBoxSizer *m_filament_panel_left_sizer;
    wxBoxSizer *m_filament_panel_right_sizer;
    wxBoxSizer *m_sizer_filament_2extruder;

    wxGridSizer *m_sizer_ams_mapping{nullptr};
    wxGridSizer *m_sizer_ams_mapping_left{nullptr};
    wxGridSizer *m_sizer_ams_mapping_right{nullptr};

public:
    void init_bind();
    void init_timer();
    void check_focus(wxWindow *window);
    void show_print_failed_info(bool show, int code = 0, wxString description = wxEmptyString, wxString extra = wxEmptyString);
    void check_fcous_state(wxWindow *window);
    void popup_filament_backup();

    void     prepare_mode(bool refresh_button = true);
    void     sending_mode();
    void     finish_mode();
    void     sync_ams_mapping_result(std::vector<FilamentInfo> &result);
    void     prepare(int print_plate_idx);
    void     show_status(PrintDialogStatus status, std::vector<wxString> params = std::vector<wxString>());
    void     sys_color_changed();
    void     reset_timeout();
    void     update_user_printer();
    void     reset_ams_material();
    void     update_show_status();
    void     on_rename_click(wxMouseEvent &event);
    void     on_rename_enter();
    void     update_printer_combobox(wxCommandEvent &event);
    void     on_cancel(wxCloseEvent &event);
    void     show_errors(wxString &info);
    void     on_ok_btn(wxCommandEvent &event);
    void     Enable_Auto_Refill(bool enable);
    void     connect_printer_mqtt();
    void     on_send_print();
    void     clear_ip_address_config(wxCommandEvent &e);
    void     on_refresh(wxCommandEvent &event);
    void     on_set_finish_mapping(wxCommandEvent &evt);
    void     on_print_job_cancel(wxCommandEvent &evt);
    void     reset_and_sync_ams_list();
    void     clone_thumbnail_data();
    void     record_edge_pixels_data();
    wxColour adjust_color_for_render(const wxColour &color);
    void     final_deal_edge_pixels_data(ThumbnailData &data);
    void     updata_thumbnail_data_after_connected_printer();
    void     unify_deal_thumbnail_data(ThumbnailData &input_data, ThumbnailData &no_light_data);
    void     change_default_normal(int old_filament_id, wxColour temp_ams_color);
    void     set_default_from_sdcard();
    void     update_page_turn_state(bool show);
    void     on_timer(wxTimerEvent &event);
    void     on_selection_changed(wxCommandEvent &event);
    void     update_flow_cali_check(MachineObject *obj);
    void     Enable_Refresh_Button(bool en);
    void     Enable_Send_Button(bool en);
    void     update_user_machine_list();
    void     update_lan_machine_list();
    void     stripWhiteSpace(std::string &str);
    void     update_ams_status_msg(wxString msg, bool is_warning = false);
    void     update_priner_status_msg(wxString msg, bool is_warning = false);
    void     update_print_status_msg(wxString msg, bool is_warning = false, bool is_printer = true);
    void     update_print_error_info(int code, std::string msg, std::string extra);
    void     set_flow_calibration_state(bool state, bool show_tips = true);
    bool     has_timelapse_warning();
    void     update_timelapse_enable_status();
    bool     is_same_printer_model();
    bool     is_blocking_printing(MachineObject *obj_);
    bool     is_same_nozzle_diameters(NozzleType &tag_nozzle_type, float &nozzle_diameter);
    bool     is_same_nozzle_type(std::string &filament_type, NozzleType &tag_nozzle_type);
    bool     has_tips(MachineObject *obj);
    bool     is_timeout();
    int  update_print_required_data(Slic3r::DynamicPrintConfig config, Slic3r::Model model, Slic3r::PlateDataPtrs plate_data_list, std::string file_name, std::string file_path);
    void set_print_type(PrintFromType type) { m_print_type = type; };
    bool do_ams_mapping(MachineObject *obj_);
    bool get_ams_mapping_result(std::string &mapping_array_str, std::string &mapping_array_str2, std::string &ams_mapping_info);
    bool build_nozzles_info(std::string &nozzles_info);
    bool can_hybrid_mapping(ExtderData data);
    void auto_supply_with_ext(std::vector<AmsTray> slots);
    bool is_nozzle_type_match(ExtderData data);
    int  convert_filament_map_nozzle_id_to_task_nozzle_id(int nozzle_id);

    std::string get_print_status_info(PrintDialogStatus status);

    PrintFromType            get_print_type() { return m_print_type; };
    wxString                 format_bed_name(std::string plate_name);
    wxString                 format_steel_name(NozzleType type);
    wxString                 format_text(wxString &m_msg);
    PrintDialogStatus        get_status() { return m_print_status; }
    std::vector<std::string> sort_string(std::vector<std::string> strArray);

    const std::vector<FilamentInfo> &get_ams_mapping_result() { return m_ams_mapping_result; }

public:
    struct SyncInfo
    {
        bool connected_printer = false;
        bool first_sync = false;
    };
    struct SyncResult
    {
        bool direct_sync = true;
        std::map<int, AMSMapInfo> sync_maps;
    };
    SyncAmsInfoDialog(wxWindow *parent, SyncInfo &info);
    ~SyncAmsInfoDialog();
    void on_dpi_changed(const wxRect &suggested_rect) override;
    const SyncResult &get_result() { return m_result; }

public:
    bool Show(bool show) override;
    void updata_ui_data_after_connected_printer();
    void update_ams_check(MachineObject *obj);
    void set_default(bool hide_some = false);
    void update_select_layout(MachineObject *obj);
    void set_default_normal(const ThumbnailData &);
    bool is_must_finish_slice_then_connected_printer() ;
    void update_printer_name() ;
    void hide_no_use_controls();
    void show_sizer(wxSizer *sizer, bool show);
    void deal_ok();
    bool get_is_double_extruder();

private:
    wxBoxSizer *create_sizer_thumbnail(wxButton *image_button, bool left);
    void        update_when_change_plate(int);

private:
    SyncInfo & m_input_info;
    SyncResult m_result;
    Button *   m_button_ok     = nullptr;
    Button *   m_button_cancel = nullptr;

    wxStaticText* m_tip_text{nullptr};
    wxStaticText*  m_used_colors_tip_text{nullptr};
    wxStaticText* m_warning_text{nullptr};
    wxBoxSizer *  m_left_sizer_thumbnail{nullptr};
    wxBoxSizer *  m_right_sizer_thumbnail{nullptr};
    wxButton *      m_left_image_button = nullptr;
    wxButton *      m_right_image_button = nullptr;
    wxBoxSizer *    sizer_basic_right_info = nullptr;
    wxBoxSizer *    sizer_advanced_options_title = nullptr;
    wxPanel *    m_rename_edit_panel  = nullptr;
    wxStaticText *  m_confirm_title                = nullptr;
    wxStaticText *  m_are_you_sure_title                = nullptr;
    wxStaticText *  m_specify_color_cluster_title  = nullptr;
    wxBoxSizer *    m_plate_combox_sizer          = nullptr;
    wxBoxSizer *    m_sizer_two_image              = nullptr;
    wxStaticText *  m_printer_title                = nullptr;

    wxCheckBox* m_append_color_checkbox = nullptr;
    wxCheckBox* m_merge_color_checkbox = nullptr;
    bool m_is_empty_project = true;

    std::vector<int> m_plate_choices;
    const int THUMBNAIL_SIZE_WIDTH = 200;
    int      m_specify_plate_idx{-1};
    wxString m_printer_name;
};
}}     // namespace Slic3r::GUI
#endif  // _STEP_MESH_DIALOG_H_
