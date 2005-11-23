/*
  The contents of this file are subject to the Initial Developer's Public
  License Version 1.0 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License here:
  http://www.flamerobin.org/license.html.

  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.

  The Original Code is FlameRobin (TM).

  The Initial Developer of the Original Code is Milan Babuskov.

  Portions created by the original developer
  are Copyright (C) 2005 Milan Babuskov.

  All Rights Reserved.

  $Id$

  Contributor(s):
*/

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <map>

#include "config/Config.h"
#include "metadata/database.h"
#include "metadata/server.h"
#include "statementHistory.h"
#include "ugly.h"

class Server;
//-----------------------------------------------------------------------------
using namespace std;
//-----------------------------------------------------------------------------
StatementHistory::StatementHistory(const wxString& storageName)
{
    storageNameM = storageName;
    Position p = 0;
    while (true)    // load history
    {
        wxString keyname;
        keyname << wxT("HISTORY_") << storageNameM << wxT("_ITEM_") << (p++);
        wxString s;
        if (config().getValue(keyname, s)) // history exists
            statementsM.push_back(s);
        else
            break;
    }
}
//-----------------------------------------------------------------------------
StatementHistory::StatementHistory(const StatementHistory& source)
{
    storageNameM = source.storageNameM;
    statementsM.assign(source.statementsM.begin(), source.statementsM.end());
}
//-----------------------------------------------------------------------------
StatementHistory::~StatementHistory()
{
    int i=0;
    for (deque<wxString>::iterator it = statementsM.begin(); it != statementsM.end(); ++it)
    {
        if ((*it).IsEmpty())    // don't save empty buffers
            continue;
        wxString keyname;
        keyname << wxT("HISTORY_") << storageNameM << wxT("_ITEM_") << (i++);
        config().setValue(keyname, *it);
    }
}
//-----------------------------------------------------------------------------
//! reads granularity from config() and gives pointer to appropriate history object
StatementHistory& StatementHistory::get(Database* db)
{
    enum historyGranularity { hgCommonToAll = 0, hgPerDatabaseName, hgPerDatabase };
    historyGranularity hg = (historyGranularity)(config().get(wxT("statementHistoryGranularity"), (int)hgPerDatabase));
    if (hg == hgCommonToAll)
    {
        static StatementHistory st(wxT(""));
        return st;
    }

    else if (hg == hgPerDatabaseName)
    {
        static map<wxString, StatementHistory> stm;
        if (stm.find(db->getName_()) == stm.end())
        {
            StatementHistory st(wxT("DATABASENAME") + db->getName_());
            stm.insert(pair<wxString, StatementHistory>(db->getName_(), st));
        }
        return (*(stm.find(db->getName_()))).second;
    }

    else // (hg == hgPerDatabase)
    {
        static map<Database*, StatementHistory> stm;
        if (stm.find(db) == stm.end())
        {
            StatementHistory st(wxT("DATABASE") + db->getId());
            stm.insert(pair<Database*, StatementHistory>(db, st));
        }
        return (*(stm.find(db))).second;
    }
}
//-----------------------------------------------------------------------------
wxString StatementHistory::get(StatementHistory::Position pos)
{
    if (pos < size())
        return statementsM[pos];
    else
        return wxEmptyString;
}
//-----------------------------------------------------------------------------
void StatementHistory::set(StatementHistory::Position pos, const wxString& str)
{
    if (pos < size())
        statementsM[pos] = str;
    else
        statementsM.push_back(str);
    checkSize();
}
//-----------------------------------------------------------------------------
StatementHistory::Position StatementHistory::add(const wxString& str)
{
    if (!config().get(wxT("limitHistoryItemSize"), true) ||
        int(str.Length()) < 1024 * config().get(wxT("statementHistoryItemSize"), 10))
    {
        if (statementsM.empty() || statementsM.back() != str)
            statementsM.push_back(str);
        checkSize();
    }
    return statementsM.size() - 1;
}
//-----------------------------------------------------------------------------
StatementHistory::Position StatementHistory::size()
{
    return statementsM.size();
}
//-----------------------------------------------------------------------------
void StatementHistory::checkSize()
{
    if (!config().get(wxT("limitHistorySize"), false))
        return;

    int historySize = config().get(wxT("statementHistorySize"), 50);     // -1 = unlimited
    while (statementsM.size() > (deque<wxString>::size_type)historySize)
        statementsM.pop_front();
}
//-----------------------------------------------------------------------------
