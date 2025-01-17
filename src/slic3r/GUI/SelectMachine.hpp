#ifndef slic3r_GUI_SelectMachine_hpp_
#define slic3r_GUI_SelectMachine_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/collpane.h>
#include <wx/dataview.h>
#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/dataview.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/hyperlink.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/popupwin.h>
#include <wx/spinctrl.h>
#include <wx/artprov.h>
#include <wx/wrapsizer.h>
#include <wx/srchctrl.h>

#include "boost/bimap/bimap.hpp"
#include "AmsMappingPopup.hpp"
#include "ReleaseNote.hpp"
#include "GUI_Utils.hpp"
#include "wxExtensions.hpp"
#include "DeviceManager.hpp"
#include "Plater.hpp"
#include "BBLStatusBar.hpp"
#include "BBLStatusBarPrint.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/ScrolledWindow.hpp"
#include "Widgets/PopupWindow.hpp"
#include <wx/simplebook.h>
#include <wx/hashmap.h>

#define  PRINT_OPT_BG_GRAY       0xF8F8F8
#define  PRINT_OPT_ITEM_BG_GRAY  0xEEEEEE

namespace Slic3r { namespace GUI {

std::string get_nozzle_volume_type_cloud_string(NozzleVolumeType nozzle_volume_type);
void        print_ams_mapping_result(std::vector<FilamentInfo> &result);
enum PrintFromType {
    FROM_NORMAL,
    FROM_SDCARD_VIEW,
};

enum PrintPageMode {
    PrintPageModePrepare = 0,
    PrintPageModeSending,
    PrintPageModeFinish
};

enum PrintDialogStatus {
    PrintStatusInit = 0,
    PrintStatusNoUserLogin,
    PrintStatusInvalidPrinter,
    PrintStatusConnectingServer,
    PrintStatusReading,
    PrintStatusReadingFinished,
    PrintStatusReadingTimeout,
    PrintStatusInUpgrading,
    PrintStatusNeedUpgradingAms,
    PrintStatusInSystemPrinting,
    PrintStatusInPrinting,
    PrintStatusDisableAms,
    PrintStatusAmsOnSettingup,
    PrintStatusAmsMappingSuccess,
    PrintStatusAmsMappingInvalid,
    PrintStatusAmsMappingU0Invalid,
    PrintStatusAmsMappingMixInvalid,
    PrintStatusNozzleMatchInvalid,
    PrintStatusNozzleDataInvalid,
    PrintStatusNozzleDiameterMismatch,
    PrintStatusNozzleTypeMismatch,
    PrintStatusAmsMappingValid,
    PrintStatusAmsMappingByOrder,
    PrintStatusRefreshingMachineList,
    PrintStatusSending,
    PrintStatusSendingCanceled,
    PrintStatusLanModeNoSdcard,
    PrintStatusLanModeSDcardNotAvailable,
    PrintStatusNoSdcard,
    PrintStatusTimelapseNoSdcard,
    PrintStatusNotOnTheSameLAN,
    PrintStatusNeedForceUpgrading,
    PrintStatusNeedConsistencyUpgrading,
    PrintStatusNotSupportedSendToSDCard,
    PrintStatusNotSupportedPrintAll,
    PrintStatusBlankPlate,
    PrintStatusUnsupportedPrinter,
    PrintStatusTimelapseWarning,
    PrintStatusMixAmsAndVtSlotWarning,
    PrintStatusPublicInitFailed,
    PrintStatusPublicUploadFiled
};

class Material
{
public:
    int           id;
    MaterialItem *item;
};


enum class CloudTaskNozzleId : int
{
    NOZZLE_RIGHT    = 0,
    NOZZLE_LEFT     = 1,
};

enum class FilamentMapNozzleId : int
{
    NOZZLE_LEFT     = 1,
    NOZZLE_RIGHT    = 2,
};

enum class ConfigNozzleIdx : int
{
    NOZZLE_LEFT      = 0,
    NOZZLE_RIGHT     = 1,
};


WX_DECLARE_HASH_MAP(int, Material *, wxIntegerHash, wxIntegerEqual, MaterialHash);

#define SELECT_MACHINE_DIALOG_BUTTON_SIZE wxSize(FromDIP(57), FromDIP(32))
#define SELECT_MACHINE_DIALOG_BUTTON_SIZE2 wxSize(FromDIP(80), FromDIP(32))
#define SELECT_MACHINE_DIALOG_SIMBOOK_SIZE wxSize(FromDIP(370), FromDIP(64))
#define SELECT_MACHINE_DIALOG_SIMBOOK_SIZE2 wxSize(FromDIP(645), FromDIP(32))
#define LIST_REFRESH_INTERVAL 200
static int get_brightness_value(wxImage image) {

    wxImage grayImage = image.ConvertToGreyscale();

    int width = grayImage.GetWidth();
    int height = grayImage.GetHeight();

    int totalLuminance = 0;
    unsigned char alpha;
    int num_none_transparent = 0;
    for (int y = 0; y < height; y += 2) {

        for (int x = 0; x < width; x += 2) {

            alpha = image.GetAlpha(x, y);
            if (alpha != 0) {
                wxColour pixelColor = grayImage.GetRed(x, y);
                totalLuminance += pixelColor.Red();
                num_none_transparent = num_none_transparent + 1;
            }
        }
    }
    if (totalLuminance <= 0 || num_none_transparent <= 0) {
        return 0;
    }
    return totalLuminance / num_none_transparent;
}

struct POItem
{
    std::string key;
    wxString value;
};

class PrintOptionItem : public wxPanel
{
public:
    PrintOptionItem(wxWindow *parent, std::vector<POItem> ops, std::string param = "");
    ~PrintOptionItem(){};
    void OnPaint(wxPaintEvent &event);
    void render(wxDC &dc);
    void on_left_down(wxMouseEvent &evt);
    void doRender(wxDC &dc);

    ScalableBitmap m_selected_bk;
    std::vector<POItem> m_ops;
    std::string selected_key;
    std::string m_param;

    void setValue(std::string value);
    void update_options(std::vector<POItem> ops){
        m_ops = ops;
        selected_key = "";
        auto width  = ops.size() * FromDIP(56) + FromDIP(8);
        auto height = FromDIP(22) + FromDIP(8);
        SetMinSize(wxSize(width, height));
        SetMaxSize(wxSize(width, height));
        Refresh();
    };
    std::string getValue();

public:
    void msw_rescale() { m_selected_bk.msw_rescale(); Refresh(); };
};

class PrintOption : public wxPanel
{
public:
    PrintOption(wxWindow *parent, wxString title, wxString tips, std::vector<POItem> ops, std::string param = "");
    ~PrintOption(){};
    void OnPaint(wxPaintEvent &event);
    void render(wxDC &dc);
    void doRender(wxDC &dc);
    void msw_rescale() { m_printoption_item->msw_rescale(); };

    std::string m_param;
    std::vector<POItem> m_ops;
    Label*   m_label{nullptr};
    Label*   m_printoption_title{nullptr};
    PrintOptionItem* m_printoption_item{nullptr};
    void setValue(std::string value);
    void update_options(std::vector<POItem> ops){
        m_ops = ops;
        m_printoption_item->update_options(ops);
    };
    std::string getValue();
    int getValueInt();
};

class ThumbnailPanel : public wxPanel
{
public:
    wxBitmap        m_bitmap;
    wxStaticBitmap *m_staticbitmap{nullptr};

    ThumbnailPanel(wxWindow *parent, wxWindowID winid = wxID_ANY, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize);
    ~ThumbnailPanel();

    void OnPaint(wxPaintEvent &event);
    void PaintBackground(wxDC &dc);
    void OnEraseBackground(wxEraseEvent &event);
    void set_thumbnail(wxImage &img);
    void render(wxDC &dc);

private:
    ScalableBitmap m_background_bitmap;
    wxBitmap       bitmap_with_background;
    int            m_brightness_value{-1};
};


class SendModeSwitchButton : public wxPanel
{
public:
    SendModeSwitchButton(wxWindow *parent, wxString mode, bool sel);
    ~SendModeSwitchButton(){};
    void OnPaint(wxPaintEvent &event);
    void render(wxDC &dc);
    void on_left_down(wxMouseEvent &evt);
    void doRender(wxDC &dc);
    void setSelected(bool selected);
    bool isSelected(){return is_selected;};
    bool is_selected {false};
    ScalableBitmap m_img_selected;
    ScalableBitmap m_img_unselected;
    ScalableBitmap m_img_selected_tag;
    ScalableBitmap m_img_unselected_tag;
};

class SelectMachineDialog : public DPIDialog
{
private:
    int                                 m_current_filament_id{0};
    int                                 m_print_plate_idx{0};
    int                                 m_print_plate_total{0};
    int                                 m_timeout_count{0};
    int                                 m_print_error_code{0};
    bool                                m_is_in_sending_mode{ false };
    bool                                m_ams_mapping_res{ false };
    bool                                m_ams_mapping_valid{ false };
    bool                                m_export_3mf_cancel{ false };
    bool                                m_is_canceled{ false };
    bool                                m_is_rename_mode{ false };
    bool                                m_check_flag {false};
    PrintPageMode                       m_print_page_mode{PrintPageMode::PrintPageModePrepare};
    std::string                         m_print_error_msg;
    std::string                         m_print_error_extra;
    std::string                         m_printer_last_select;
    std::string                         m_print_info;
    wxString                            m_current_project_name;
    PrintDialogStatus                   m_print_status { PrintStatusInit };
    wxColour                            m_colour_def_color{wxColour(255, 255, 255)};
    wxColour                            m_colour_bold_color{wxColour(38, 46, 48)};
    StateColor                          m_btn_bg_enable;
    Label* m_text_bed_type;

    std::shared_ptr<int>                m_token = std::make_shared<int>(0);
    std::map<std::string, PrintOption*>   m_checkbox_list;
    std::vector<wxString>               m_bedtype_list;
    std::vector<MachineObject*>         m_list;
    std::vector<FilamentInfo>           m_filaments;
    std::vector<FilamentInfo>           m_ams_mapping_result;
    std::vector<int>                    m_filaments_map;
    std::shared_ptr<BBLStatusBarPrint>  m_status_bar;

    //SendModeSwitchButton*               m_mode_print {nullptr};
    //SendModeSwitchButton*               m_mode_send {nullptr};
    wxStaticBitmap*                     m_printer_image{nullptr};
    wxStaticBitmap*                     m_bed_image{nullptr};

    Slic3r::DynamicPrintConfig          m_required_data_config;
    Slic3r::Model                       m_required_data_model;
    Slic3r::PlateDataPtrs               m_required_data_plate_data_list;
    std::string                         m_required_data_file_name;
    std::string                         m_required_data_file_path;

    std::vector<POItem> ops_auto;
    std::vector<POItem> ops_no_auto;

protected:
    PrintFromType                       m_print_type{FROM_NORMAL};
    AmsMapingPopup                      m_mapping_popup{ nullptr };
    AmsMapingTipPopup                   m_mapping_tip_popup{ nullptr };
    AmsTutorialPopup                    m_mapping_tutorial_popup{ nullptr };
    MaterialHash                        m_materialList;
    Plater *                            m_plater{nullptr};
    wxPanel *                           m_options_other {nullptr};
    wxBoxSizer*                         m_sizer_options_timelapse{ nullptr };
    wxBoxSizer*                         m_sizer_options_other{ nullptr };
    wxBoxSizer*                         m_sizer_thumbnail{ nullptr };

    wxBoxSizer*                         m_sizer_main{ nullptr };
    wxBoxSizer*                         m_basicl_sizer{ nullptr };
    wxBoxSizer*                         rename_sizer_v{ nullptr };
    wxBoxSizer*                         rename_sizer_h{ nullptr };
    wxBoxSizer*                         m_sizer_autorefill{ nullptr };
    wxBoxSizer*                         m_mapping_sugs_sizer{ nullptr };
    wxBoxSizer*                         m_change_filament_times_sizer{ nullptr };
    ScalableButton*                     m_button_refresh{ nullptr };
    Button*                             m_button_ensure{ nullptr };
    wxStaticBitmap *                    m_rename_button{nullptr};
    ComboBox*                           m_comboBox_printer{ nullptr };
    wxStaticBitmap*                     m_staticbitmap{ nullptr };
    wxStaticBitmap*                     m_bitmap_last_plate{ nullptr };
    wxStaticBitmap*                     m_bitmap_next_plate{ nullptr };
    wxStaticBitmap*                     img_amsmapping_tip{nullptr};
    ThumbnailPanel*                     m_thumbnailPanel{ nullptr };
    wxPanel*                            m_panel_status{ nullptr };
    wxPanel*                            m_basic_panel;
    wxPanel*                            m_rename_normal_panel{nullptr};
    wxPanel*                            m_panel_sending{nullptr};
    wxPanel*                            m_panel_prepare{nullptr};
    wxPanel*                            m_panel_finish{nullptr};
    wxPanel*                            m_line_top{ nullptr };
    Label*                              m_link_edit_nozzle{ nullptr };
    Label*                              m_st_txt_error_code{nullptr};
    Label*                              m_st_txt_error_desc{nullptr};
    Label*                              m_st_txt_extra_info{nullptr};
    Label*                              m_ams_backup_tip{nullptr};
    wxHyperlinkCtrl*                    m_link_network_state{ nullptr };
    wxSimplebook*                       m_rename_switch_panel{nullptr};
    wxSimplebook*                       m_simplebook{nullptr};
    wxStaticText*                       m_rename_text{nullptr};
    Label*                              m_stext_printer_title{nullptr};
    Label*                              m_stext_time{ nullptr };
    Label*                              m_stext_weight{ nullptr };
    Label*                              m_statictext_ams_msg{ nullptr };
    Label*                              m_txt_change_filament_times{ nullptr };
    Label*                              m_text_printer_msg{ nullptr };
    wxStaticText*                       m_staticText_bed_title{ nullptr };
    wxStaticText*                       m_stext_sending{ nullptr };
    wxStaticText*                       m_statictext_finish{nullptr};
    TextInput*                          m_rename_input{nullptr};
    wxTimer*                            m_refresh_timer{ nullptr };
    std::shared_ptr<PrintJob>           m_print_job;
    wxScrolledWindow*                   m_sw_print_failed_info{nullptr};
    wxHyperlinkCtrl*                    m_hyperlink{nullptr};
    wxStaticBitmap *                    m_advanced_options_icon{nullptr};
    ScalableBitmap *                    rename_editable{nullptr};
    ScalableBitmap *                    rename_editable_light{nullptr};
    wxStaticBitmap *                    timeimg{nullptr};
    ScalableBitmap *                    print_time{nullptr};
    wxStaticBitmap *                    weightimg{nullptr};
    ScalableBitmap *                    print_weight{nullptr};
    ScalableBitmap *                    ams_mapping_help_icon{nullptr};
    wxStaticBitmap *                    img_ams_backup{nullptr};
    ThumbnailData                       m_cur_input_thumbnail_data;
    ThumbnailData                       m_cur_no_light_thumbnail_data;
    ThumbnailData                       m_preview_thumbnail_data;//when ams map change
    std::vector<wxColour>               m_preview_colors_in_thumbnail;
    std::vector<wxColour>               m_cur_colors_in_thumbnail;
    std::vector<bool>                   m_edge_pixels;

    StaticBox*                          m_filament_panel;
    StaticBox*                          m_filament_left_panel;
    StaticBox*                          m_filament_right_panel;

    wxBoxSizer*                         m_filament_panel_sizer;
    wxBoxSizer*                         m_filament_panel_left_sizer;
    wxBoxSizer*                         m_filament_panel_right_sizer;
    wxBoxSizer*                         m_sizer_filament_2extruder;

    wxGridSizer*                        m_sizer_ams_mapping{ nullptr };
    wxGridSizer*                        m_sizer_ams_mapping_left{ nullptr };
    wxGridSizer*                        m_sizer_ams_mapping_right{ nullptr };

public:
    static std::vector<wxString> MACHINE_BED_TYPE_STRING;
    static void                  init_machine_bed_types();
    static std::vector<std::string> MachineBedTypeString;

public:
    SelectMachineDialog(Plater *plater = nullptr);
    ~SelectMachineDialog();

    void init_bind();
    void init_timer();
    void check_focus(wxWindow* window);
    void show_print_failed_info(bool show, int code = 0, wxString description = wxEmptyString, wxString extra = wxEmptyString);
    void check_fcous_state(wxWindow* window);
    void popup_filament_backup();
    void update_select_layout(MachineObject *obj);
    void prepare_mode(bool refresh_button = true);
    void sending_mode();
    void finish_mode();
	void sync_ams_mapping_result(std::vector<FilamentInfo>& result);
    void prepare(int print_plate_idx);
    void show_status(PrintDialogStatus status, std::vector<wxString> params = std::vector<wxString>());
    void sys_color_changed();
    void reset_timeout();
    void update_user_printer();
    void reset_ams_material();
    void update_show_status();
    void update_ams_check(MachineObject* obj);
    void update_filament_change_count();
    void     on_rename_click(wxMouseEvent &event);
    void on_rename_enter();
    void update_printer_combobox(wxCommandEvent& event);
    void on_cancel(wxCloseEvent& event);
    void show_errors(wxString& info);
    void on_ok_btn(wxCommandEvent& event);
    void Enable_Auto_Refill(bool enable);
    void connect_printer_mqtt();
    void on_send_print();
    void clear_ip_address_config(wxCommandEvent& e);
    void on_refresh(wxCommandEvent& event);
    void on_set_finish_mapping(wxCommandEvent& evt);
    void on_print_job_cancel(wxCommandEvent& evt);
    void set_default();
    void reset_and_sync_ams_list();
    void clone_thumbnail_data();
    void record_edge_pixels_data();
    wxColour adjust_color_for_render(const wxColour& color);
    void final_deal_edge_pixels_data(ThumbnailData& data);
    void updata_thumbnail_data_after_connected_printer();
    void unify_deal_thumbnail_data(ThumbnailData &input_data, ThumbnailData &no_light_data);
    void change_default_normal(int old_filament_id, wxColour temp_ams_color);
    void set_default_normal(const ThumbnailData&);
    void set_default_from_sdcard();
    void update_page_turn_state(bool show);
    void on_timer(wxTimerEvent& event);
    void on_selection_changed(wxCommandEvent& event);
    void update_flow_cali_check(MachineObject* obj);
    void Enable_Refresh_Button(bool en);
    void Enable_Send_Button(bool en);
    void on_dpi_changed(const wxRect& suggested_rect) override;
    void update_user_machine_list();
    void update_lan_machine_list();
    void stripWhiteSpace(std::string& str);
    void update_ams_status_msg(wxString msg, bool is_warning = false);
    void update_priner_status_msg(wxString msg, bool is_warning = false);
    void update_print_status_msg(wxString msg, bool is_warning = false, bool is_printer = true);
    void update_print_error_info(int code, std::string msg, std::string extra);
    void set_flow_calibration_state(bool state, bool show_tips = true);
    bool has_timelapse_warning();
    void update_timelapse_enable_status();
    bool is_same_printer_model();
    bool is_blocking_printing(MachineObject* obj_);
    bool is_same_nozzle_diameters(float& tag_nozzle_diameter) const;
    bool is_same_nozzle_type(const Extder& extruder, std::string& filament_type) const;
    bool has_tips(MachineObject* obj);
    bool is_timeout();
    int  update_print_required_data(Slic3r::DynamicPrintConfig config, Slic3r::Model model, Slic3r::PlateDataPtrs plate_data_list, std::string file_name, std::string file_path);
    void set_print_type(PrintFromType type) {m_print_type = type;};
    bool Show(bool show);
    bool     do_ams_mapping(MachineObject *obj_);
    bool get_ams_mapping_result(std::string& mapping_array_str, std::string& mapping_array_str2, std::string& ams_mapping_info);
    bool build_nozzles_info(std::string& nozzles_info);
    bool can_hybrid_mapping(ExtderData data);
    void auto_supply_with_ext(std::vector<AmsTray> slots);
    bool is_nozzle_type_match(ExtderData data);
    int  convert_filament_map_nozzle_id_to_task_nozzle_id(int nozzle_id);

    std::string get_print_status_info(PrintDialogStatus status);

    PrintFromType get_print_type() {return m_print_type;};
    wxString    format_bed_name(std::string plate_name);
    wxString    format_steel_name(NozzleType type);
    PrintDialogStatus  get_status() { return m_print_status; }
};



wxDECLARE_EVENT(EVT_SWITCH_PRINT_OPTION, wxCommandEvent);

}} // namespace Slic3r::GUI

#endif
