#ifndef slic3r_HMS_hpp_
#define slic3r_HMS_hpp_

#include "GUI_App.hpp"
#include "GUI.hpp"
#include "I18N.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/StepCtrl.hpp"
#include "BitmapCache.hpp"
#include "slic3r/Utils/Http.hpp"
#include "libslic3r/Thread.hpp"
#include "nlohmann/json.hpp"
#include <mutex>

namespace Slic3r {
namespace GUI {

#define HMS_INFO_FILE	"hms.json"
#define QUERY_HMS_INFO	"query_hms_info"
#define QUERY_HMS_ACTION	"query_hms_action"

class HMSQuery {

protected:
	json m_hms_info_json;
	json m_hms_action_json;
    mutable std::mutex m_hms_mutex;

    int download_hms_related(const std::string& hms_type, const std::string& dev_type, json *receive_json);
    int load_from_local(const std::string& hms_type, const std::string& dev_type, json * receive_json, std::string& version_info);
	int save_to_local(std::string lang, std::string hms_type, std::string dev_type, json save_json);
    std::string get_hms_file(std::string hms_type, std::string lang = std::string("en"), std::string dev_type = "");
	wxString _query_hms_msg(std::string long_error_code, std::string lang_code = std::string("en")) const;
    bool _query_error_msg(wxString &error_msg, std::string long_error_code, std::string lang_code = std::string("en"));
    wxString _query_error_url_action(std::string long_error_code, std::string dev_id, std::vector<int>& button_action);

public:
	HMSQuery() {}
    int  check_hms_info(std::string dev_type = "00M");
    wxString           query_hms_msg(std::string long_error_code) const;
    bool query_print_error_msg(int print_error, wxString &error_msg);
    wxString query_print_error_url_action(int print_error, std::string dev_id, std::vector<int>& button_action);
	static std::string hms_language_code();
	static std::string build_query_params(std::string& lang);
    bool check_local_file(std::string dev_type);

private:
    void copy_from_data_dir_to_local();
};

int get_hms_info_version(std::string &version);

std::string get_hms_wiki_url(std::string code);

std::string get_error_message(int error_code);

}
}


#endif