#define REAPERAPI_IMPLEMENT
#include "reaper_api.hpp"
#include "rename_regions_action.hpp"

#include <cstring>
#include <ranges>

namespace {

constexpr char menu_id[]{"ioj_main"};

struct RegistrationKeys {
    char const* add;
    char const* remove;
};

constexpr RegistrationKeys custom_action{"custom_action", "-custom_action"};
constexpr RegistrationKeys command_hook{"hookcommand2", "-hookcommand2"};
constexpr RegistrationKeys menu_hook{"hookcustommenu", "-hookcustommenu"};

using ActionHandler = void (*)();
using HostRegister = decltype(reaper_plugin_info_t::Register);

struct MenuAction {
    custom_action_register_t registration;
    wchar_t const* menu_label;
    ActionHandler run;
    int command_id{};
};

constexpr auto make_menu_action(char const* id, char const* action_name, wchar_t const* menu_label,
    ActionHandler run) -> MenuAction
{
    return MenuAction{{0, id, action_name, nullptr}, menu_label, run};
}

MenuAction actions[]{
    make_menu_action("IOJ_RENAME_REGIONS_FROM_MEDIA_ITEMS", ioj::rename_regions_action_name,
        L"Rename regions from media items", ioj::rename_regions)
};

auto on_command(KbdSectionInfo*, int command, int, int, int, HWND) -> bool
{
    for (auto const& action : actions) {
        if (command == action.command_id) {
            action.run();
            return true;
        }
    }

    return false;
}

auto on_menu(char const* id, void* menu, int flag) -> void
{
    if (flag != 0 || std::strcmp(id, menu_id) != 0) {
        return;
    }

    auto const handle{static_cast<HMENU>(menu)};
    for (auto const& action : actions) {
        AppendMenuW(handle, MF_STRING, action.command_id, action.menu_label);
    }
}

struct Hook {
    RegistrationKeys keys;
    void* callback;
    bool registered{};
};

Hook hooks[]{
    {command_hook, reinterpret_cast<void*>(on_command)},
    {menu_hook, reinterpret_cast<void*>(on_menu)}
};

HostRegister host_register{};

auto register_extension() -> bool
{
    for (auto& action : actions) {
        action.command_id = host_register(custom_action.add, &action.registration);
        if (!action.command_id) {
            return false;
        }
    }

    for (auto& hook : hooks) {
        hook.registered = host_register(hook.keys.add, hook.callback) != 0;
        if (!hook.registered) {
            return false;
        }
    }

    return AddCustomizableMenu(menu_id, "ioj", nullptr, true);
}

auto unregister_extension() -> void
{
    if (!host_register) {
        return;
    }

    for (auto& hook : hooks | std::views::reverse) {
        if (hook.registered) {
            host_register(hook.keys.remove, hook.callback);
            hook.registered = false;
        }
    }

    for (auto& action : actions | std::views::reverse) {
        if (action.command_id) {
            host_register(custom_action.remove, &action.registration);
            action.command_id = 0;
        }
    }

    host_register = nullptr;
}

}

extern "C" REAPER_PLUGIN_DLL_EXPORT auto REAPER_PLUGIN_ENTRYPOINT(HINSTANCE, reaper_plugin_info_t* info) -> int
{
    if (!info) {
        unregister_extension();
        return 0;
    }

    if (info->caller_version != REAPER_PLUGIN_VERSION || !info->GetFunc || REAPERAPI_LoadAPI(info->GetFunc)) {
        return 0;
    }

    host_register = info->Register;
    if (!register_extension()) {
        unregister_extension();
        return 0;
    }

    return 1;
}
