/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/FileUtils.h>
#include <znc/Server.h>
#include <znc/IRCNetwork.h>
#include <znc/User.h>
#include <znc/ZNCDebug.h>

class CAdminDebugMod : public CModule {
  public:
    MODCONSTRUCTOR(CAdminDebugMod) {
        AddHelpCommand();
        AddCommand("Enable", "", t_d("Show the logging target"),
                   [=](const CString& sLine) { CommandEnable(sLine); });
        AddCommand("Disable", "", t_d("Show the logging target"),
                   [=](const CString& sLine) { CommandDisable(sLine); });
        // TODO: Introduce file logging at some point.
        AddCommand("Target", "<self|admins|nobody>",
                   t_s("Log to the person who runs 'Enable', all connected " +
                   "ZNC Administrators, or nobody (for e.g. people running " +
                   "`znc --foreground > out.log`)"),
                   [=](const CString& sLine) { CommandTarget(sLine); });
        AddCommand("Status", "", t_d("Show the logging target"),
                   [=](const CString& sLine) { CommandStatus(sLine); });
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        bool admin = GetUser()->IsAdmin();

        if (admin) {
            // Set sane defaults.
            if (GetNV("target").IsEmpty()) {
                SetNV("target", "admins");
            }
        }

        return admin;
    }

    void RegisterDebugHandler(bool bEnable) {
        if (!bEnable) {
            CDebug::RegisterDebugHandler(nullptr);
            return;
        }

        CDebug::RegisterDebugHandler(
            [=](const CString& sMessage) { OnIncomingDebugMessage(sMessage); }
        );
    }

    void OnIncomingDebugMessage(const CString& sMessage) {
        CString sTarget = GetNV("target");

        if (sTarget.Equals("self")) {
            CString sUser = GetNV("enabled_by");

            for (const auto& it : CZNC::Get().GetUserClients()) {
                if (!it.second->IsAdmin()) continue;
                if (!it.second->GetNick().Equals(sUser)) continue; // TODO improve security by using the znc unique account name, whatever the api for that is

                it.second->PutStatusNotice("[DEBUG] " + sMessage);
            }
        } else if (sTarget.Equals("admins")) {
            for (const auto& it : CZNC::Get().GetUserClients()) {
                if (!it.second->IsAdmin()) continue;

                it.second->PutStatusNotice("[DEBUG] " + sMessage);
            }
        }
    }

    void CommandEnable(const CString& sCommand) {
        if (GetUser()->IsAdmin() == false) {
            return PutModule(t_s("Access denied!"));
        }

        ToggleDebug(true, GetUser()->GetNick());
    }

    void CommandDisable(const CString& sCommand) {
        if (GetUser()->IsAdmin() == false) {
            return PutModule(t_s("Access denied!"));
        }

        ToggleDebug(false, GetNV("enabled_by"));
    }

    bool ToggleDebug(bool bEnable, CString sEnabledBy) {
        bool bValue = CDebug::Debug();

        if (bEnable == bValue) {
            PutModule(t_s("Already %s.", t_s(bEnable ? "enabled" : "disabled")));
            return false;
        }

        CDebug::SetDebug(bEnable);
        CZNC::Get().Broadcast(CString(
            "An administrator has just turned Debug Mode \02"
            + (bValue ? "on" : "off") +
            "\02. It was enabled by \02" + sEnabledBy + "\02."
        ));
        if (bEnable) {
            CZNC::Get().Broadcast(
                CString("Messages, credentials, and other sensitive data may"
                " become exposed to the host during this period.")
            );
            SetNV("enabled_by", sEnabledBy);

            if (!GetNV("target").Equals("nobody")) {
                RegisterDebugHandler(true);
            }
        } else {
            DelNV("enabled_by");
            RegisterDebugHandler(false);
        }

        return true;
    }

    void CommandTarget(const CString& sCommand) {
        if (GetUser()->IsAdmin() == false) {
            return PutModule(t_s("Access denied!"));
        }

        CString sArg = sCommand.Token(0);

        if (sArg.Equals("self") || sArg.Equals("admins") || sArg.Equals("nobody")) {
            SetNV("target", sArg);
        } else {
            PutModule(t_s("Invalid argument 1. Must be %s", "<self|admins|nobody>"));
        }
    }

    void CommandStatus(const CString& sCommand) {
        PutModule(t_s("Debugging mode is \02%s\02.", (CDebug::Debug() ? "on" : "off")));
        PutModule(t_s("Logging to: \02%s\02.", GetNV("target")));
    }
};

template <>
void TModInfo<CAdminLogMod>(CModInfo& Info) {
    Info.SetWikiPage("admindebug");
}

GLOBALMODULEDEFS(CAdminLogMod, t_s("Enable Debug mode dynamically."))
