#include "FilamentGroupUtils.hpp"


namespace Slic3r
{
namespace FilamentGroupUtils
{
    Color::Color(const std::string& hexstr) {
        if (hexstr.empty() || (hexstr.length() != 9 && hexstr.length() != 7) || hexstr[0] != '#')
        {
            assert(false);
            r = 0, g = 0, b = 0, a = 255;
            return;
        }

        auto hexToByte = [](const std::string& hex)->unsigned char
            {
                unsigned int byte;
                std::istringstream(hex) >> std::hex >> byte;
                return static_cast<unsigned char>(byte);
            };
        r = hexToByte(hexstr.substr(1, 2));
        g = hexToByte(hexstr.substr(3, 2));
        b = hexToByte(hexstr.substr(5, 2));
        if (hexstr.size() == 9)
            a = hexToByte(hexstr.substr(7, 2));
    }

    bool Color::operator<(const Color& other) const
    {
        if (r != other.r) return r < other.r;
        if (g != other.g) return g < other.g;
        if (b != other.b) return b < other.b;
        return a < other.a;
    }

    bool Color::operator==(const Color& other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;

    }

    bool Color::operator!=(const Color& other) const
    {
        return r != other.r || g != other.g || b != other.b || a != other.a;
    }

    bool FilamentInfo::operator<(const FilamentInfo& other) const
    {
        if (color != other.color) return color < other.color;
        return type < other.type;
    }


    // TODO: add explanation
    std::vector<int> calc_max_group_size(const std::vector<std::map<int, int>>& ams_counts, bool ignore_ext_filament) {
        // add default value to 2
        std::vector<int>group_size(2, 0);
        for (size_t idx = 0; idx < ams_counts.size(); ++idx) {
            const auto& ams_count = ams_counts[idx];
            for (auto iter = ams_count.begin(); iter != ams_count.end(); ++iter) {
                group_size[idx] += iter->first * iter->second;
            }
        }

        for (size_t idx = 0; idx < group_size.size(); ++idx) {
            if (!ignore_ext_filament && group_size[idx] == 0)
                group_size[idx] = 1;
        }
        return group_size;
    }


    static std::vector<std::vector<FilamentInfo>> build_full_machine_filaments(const std::vector<std::vector<DynamicPrintConfig>>& filament_configs)
    {
        // defualt size set to 2
        std::vector<std::vector<FilamentInfo>> machine_filaments(2);
        for (size_t idx = 0; idx < filament_configs.size(); ++idx) {
            auto& arr = filament_configs[idx];
            for (auto& item : arr) {
                FilamentInfo temp;
                std::string type;
                std::string color;
                std::string tray_name;

                if (auto color_ptr = item.option<ConfigOptionStrings>("filament_colour"); color_ptr)
                    color = color_ptr->get_at(0);
                if (auto type_ptr = item.option<ConfigOptionStrings>("filament_type"); type_ptr)
                    type = type_ptr->get_at(0);
                if (auto tray_ptr = item.option<ConfigOptionStrings>("tray_name"); tray_ptr)
                    tray_name = tray_ptr->get_at(0);

                if (color.empty() || type.empty() || tray_name.empty())
                    continue;

                temp.color = Color(color);
                temp.type =type;
                temp.extruder_id = idx;
                temp.is_extended = tray_name == "Ext"; // hard-coded ext flag
                machine_filaments[idx].emplace_back(std::move(temp));
            }
        }
        return machine_filaments;
    }

    std::vector<std::vector<FilamentInfo>> build_machine_filaments(const std::vector<std::vector<DynamicPrintConfig>>& filament_configs, const std::vector<std::map<int, int>>& ams_counts, bool ignore_ext_filament)
    {
        std::vector<std::vector<FilamentInfo>> ret(2);
        std::vector<int> ams_size(2, 0);
        std::vector<std::vector<FilamentInfo>> full_machine_filaments = build_full_machine_filaments(filament_configs);
        assert(full_machine_filaments.size() == 2);
        for (size_t idx = 0; idx < std::min(ams_counts.size(),ams_size.size()); ++idx) {
            const auto& ams_count = ams_counts[idx];
            for (auto iter = ams_count.begin(); iter != ams_count.end(); ++iter) {
                ams_size[idx] += iter->first * iter->second;
            }
        }

        assert(full_machine_filaments.size() == ams_size.size());
        for (size_t idx = 0; idx < std::min(ams_size.size(), full_machine_filaments.size()); ++idx) {
            std::vector<FilamentInfo> tmp;
            bool accept_ext_filament = ams_size[idx] == 0 && !ignore_ext_filament;

            for (size_t j = 0; j < full_machine_filaments[idx].size(); ++j) {
                auto machine_filament = full_machine_filaments[idx][j];
                if (!full_machine_filaments[idx][j].is_extended)
                    tmp.emplace_back(machine_filament);
                else if (accept_ext_filament)
                    tmp.emplace_back(machine_filament);
            }
            ret[idx] = std::move(tmp);
        }
        return ret;
    }

    bool collect_unprintable_limits(const std::vector<std::set<int>>& physical_unprintables, const std::vector<std::set<int>>& geometric_unprintables, std::vector<std::set<int>>& unprintable_limits)
    {
        unprintable_limits.clear();
        unprintable_limits.resize(2);
        // resize unprintables to 2
        auto resized_physical_unprintables = physical_unprintables;
        resized_physical_unprintables.resize(2);
        auto resized_geometric_unprintables = geometric_unprintables;
        resized_geometric_unprintables.resize(2);

        bool conflict = false;
        conflict |= remove_intersection(resized_physical_unprintables[0], resized_physical_unprintables[1]);
        conflict |= remove_intersection(resized_geometric_unprintables[0], resized_geometric_unprintables[1]);

        std::map<int, int>filament_unprintable_exts;
        for (auto& ext_unprintables : { resized_physical_unprintables,resized_geometric_unprintables }) {
            for (int eid = 0; eid < ext_unprintables.size(); ++eid) {
                for (int fid : ext_unprintables[eid]) {
                    if (auto iter = filament_unprintable_exts.find(fid); iter != filament_unprintable_exts.end() && iter->second != eid)
                        conflict = true;
                    else
                        filament_unprintable_exts[fid] = eid;
                }
            }
        }
        for (auto& elem : filament_unprintable_exts)
            unprintable_limits[elem.second].insert(elem.first);

        return !conflict;
    }

    bool remove_intersection(std::set<int>& a, std::set<int>& b) {
        std::vector<int>intersection;
        std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(intersection));
        bool have_intersection = !intersection.empty();
        for (auto& item : intersection) {
            a.erase(item);
            b.erase(item);
        }
        return have_intersection;
    }

    void extract_indices(const std::vector<unsigned int>& used_filaments, const std::vector<std::set<int>>& unprintable_elems, std::vector<std::set<int>>& unprintable_idxs)
    {
        std::vector<std::set<int>>(unprintable_elems.size()).swap(unprintable_idxs);
        for (size_t gid = 0; gid < unprintable_elems.size(); ++gid) {
            for (auto& f : unprintable_elems[gid]) {
                auto iter = std::find(used_filaments.begin(), used_filaments.end(), (unsigned)f);
                if (iter != used_filaments.end())
                    unprintable_idxs[gid].insert(iter - used_filaments.begin());
            }
        }
    }

    void extract_unprintable_limit_indices(const std::vector<std::set<int>>& unprintable_elems, const std::vector<unsigned int>& used_filaments, std::map<int, int>& unplaceable_limits)
    {
        unplaceable_limits.clear();
        // map the unprintable filaments to idx of used filaments , if not used ,just ignore
        std::vector<std::set<int>> unprintable_idxs;
        extract_indices(used_filaments, unprintable_elems, unprintable_idxs);
        if (unprintable_idxs.size() > 1)
            remove_intersection(unprintable_idxs[0], unprintable_idxs[1]);

        for (size_t idx = 0; idx < unprintable_idxs.size(); ++idx) {
            for (auto f : unprintable_idxs[idx])
                if (unplaceable_limits.count(f) == 0)
                    unplaceable_limits[f] = idx;
        }
    }


    void extract_unprintable_limit_indices(const std::vector<std::set<int>>& unprintable_elems, const std::vector<unsigned int>& used_filaments, std::unordered_map<int, std::vector<int>>& unplaceable_limits)
    {
        unplaceable_limits.clear();
        std::vector<std::set<int>>unprintable_idxs;
        // map the unprintable filaments to idx of used filaments , if not used ,just ignore
        extract_indices(used_filaments, unprintable_elems, unprintable_idxs);
        // remove elems that cannot be printed in both extruder
        if (unprintable_idxs.size() > 1)
            remove_intersection(unprintable_idxs[0], unprintable_idxs[1]);

        for (size_t group_id = 0; group_id < unprintable_idxs.size(); ++group_id)
            for (auto f : unprintable_idxs[group_id])
                unplaceable_limits[f].emplace_back(group_id);

        for (auto& elem : unplaceable_limits)
            sort_remove_duplicates(elem.second);
    }

    bool check_printable(const std::vector<std::set<int>>& groups, const std::map<int,int>& unprintable)
    {
        for (size_t i = 0; i < groups.size(); ++i) {
            auto& group = groups[i];
            for (auto& filament : group) {
                if (auto iter = unprintable.find(filament); iter != unprintable.end() && i == iter->second)
                    return false;
            }
        }
        return true;
    }
}
}