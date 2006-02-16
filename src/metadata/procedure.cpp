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
  are Copyright (C) 2004 Milan Babuskov.

  All Rights Reserved.

  $Id$

  Contributor(s): Nando Dessena, Michael Hieke
*/

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include <ibpp.h>

#include "dberror.h"
#include "frutils.h"
#include "metadata/collection.h"
#include "metadata/database.h"
#include "metadata/MetadataItemVisitor.h"
#include "metadata/procedure.h"
//-----------------------------------------------------------------------------
Procedure::Procedure()
{
    parametersM.setParent(this);
    typeM = ntProcedure;
    parametersLoadedM = false;
}
//-----------------------------------------------------------------------------
Procedure::Procedure(const Procedure& rhs)
    : MetadataItem(rhs), parametersM(rhs.parametersM)
{
    parametersLoadedM = rhs.parametersLoadedM;
    parametersM.setParent(this);
}
//-----------------------------------------------------------------------------
Parameter* Procedure::addParameter(Parameter &c)
{
    if (!parametersLoadedM)
        loadParameters();
    Parameter *cc = parametersM.add(c);
    cc->setParent(this);
    return cc;
}
//-----------------------------------------------------------------------------
bool Procedure::getChildren(std::vector<MetadataItem *>& temp)
{
    return parametersM.getChildren(temp);
}
//-----------------------------------------------------------------------------
void Procedure::lockChildren()
{
    parametersM.lockSubject();
}
//-----------------------------------------------------------------------------
void Procedure::unlockChildren()
{
    parametersM.unlockSubject();
}
//-----------------------------------------------------------------------------
bool Procedure::isSelectable()
{
    if (!parametersLoadedM)
        loadParameters();
    for (MetadataCollection <Parameter>::const_iterator it = parametersM.begin(); it != parametersM.end(); ++it)
        if ((*it).getParameterType() == ptOutput)
            return true;
    return false;
}
//-----------------------------------------------------------------------------
wxString Procedure::getSelectStatement(bool withColumns)
{
    if (!parametersLoadedM)
        loadParameters();
    wxString collist, parlist;
    for (MetadataCollection <Parameter>::const_iterator it = parametersM.begin(); it != parametersM.end(); ++it)
    {
        if ((*it).getParameterType() == ptInput)
        {
            if (!parlist.empty())
                parlist += wxT(", ");
            parlist += (*it).getQuotedName();
        }
        else
        {
            if (!collist.empty())
                collist += wxT(", ");
            collist += (*it).getQuotedName();
        }
    }

    wxString sql = wxT("SELECT ");
    if (withColumns)
        sql += collist;
    else
        sql += wxT("* ");
    sql += wxT("\nFROM ") + getQuotedName();
    if (!parlist.empty())
        sql += wxT("(") + parlist + wxT(")");
    return sql;
}
//-----------------------------------------------------------------------------
wxString Procedure::getExecuteStatement()
{
    if (!parametersLoadedM)
        loadParameters();
    wxString parlist;
    for (MetadataCollection <Parameter>::const_iterator it = parametersM.begin(); it != parametersM.end(); ++it)
    {
        if ((*it).getParameterType() == ptInput)
        {
            if (!parlist.empty())
                parlist += wxT(", ");
            parlist += (*it).getQuotedName();
        }
    }

    wxString sql = wxT("EXECUTE PROCEDURE ") + getQuotedName();
    if (!parlist.empty())
        sql += wxT("(") + parlist + wxT(")");
    return sql;
}
//-----------------------------------------------------------------------------
bool Procedure::checkAndLoadParameters(bool force)
{
    if (force || !parametersLoadedM)
    {
        loadParameters();
        notifyObservers();
    }
    return parametersLoadedM;
}
//-----------------------------------------------------------------------------
//! returns false if error occurs, and places the error text in error variable
bool Procedure::loadParameters()
{
    parametersM.clear();
    Database* d = getDatabase();
    if (!d)
    {
        lastError().setMessage(wxT("database not set"));
        parametersLoadedM = false;
        return false;
    }

    IBPP::Database& db = d->getIBPPDatabase();

    try
    {
        IBPP::Transaction tr1 = IBPP::TransactionFactory(db, IBPP::amRead);
        tr1->Start();
        IBPP::Statement st1 = IBPP::StatementFactory(db, tr1);
        st1->Prepare(
            "select p.rdb$parameter_name, p.rdb$field_source, p.rdb$parameter_type"
            " from rdb$procedure_parameters p"
            " where p.rdb$PROCEDURE_name = ? "
            " order by p.rdb$parameter_type, rdb$PARAMETER_number "
        );
        st1->Set(1, wx2std(getName_()));
        st1->Execute();

        while (st1->Fetch())
        {
            std::string column_name, source;
            short partype;
            st1->Get(1, column_name);
            st1->Get(2, source);
            st1->Get(3, &partype);

            Parameter p(std2wx(source), partype);
            p.setName_(std2wx(column_name));
            Parameter* pp = parametersM.add(p);
            pp->setParent(this);
        }

        tr1->Commit();
        parametersLoadedM = true;
        return true;
    }
    catch (IBPP::Exception &e)
    {
        lastError().setMessage(std2wx(e.ErrorMessage()));
    }
    catch (...)
    {
        lastError().setMessage(_("System error."));
    }

    parametersLoadedM = false;
    return false;
}
//-----------------------------------------------------------------------------
//! returns false if an error occurs
bool Procedure::getSource(wxString& source)
{
    Database* d = getDatabase();
    if (!d)
    {
        lastError().setMessage(wxT("Database not set."));
        return false;
    }

    IBPP::Database& db = d->getIBPPDatabase();

    try
    {
        IBPP::Transaction tr1 = IBPP::TransactionFactory(db, IBPP::amRead);
        tr1->Start();
        IBPP::Statement st1 = IBPP::StatementFactory(db, tr1);
        st1->Prepare("select rdb$procedure_source from rdb$procedures where rdb$procedure_name = ?");
        st1->Set(1, wx2std(getName_()));
        st1->Execute();
        st1->Fetch();
        readBlob(st1, 1, source);
        tr1->Commit();
        return true;
    }
    catch (IBPP::Exception &e)
    {
        lastError().setMessage(std2wx(e.ErrorMessage()));
    }
    catch (...)
    {
        lastError().setMessage(_("System error."));
    }

    return false;
}
//-----------------------------------------------------------------------------
wxString Procedure::getDefinition()
{
    checkAndLoadParameters();
    wxString collist, parlist;
    MetadataCollection <Parameter>::const_iterator lastInput, lastOutput;
    for (MetadataCollection <Parameter>::const_iterator it =
        parametersM.begin(); it != parametersM.end(); ++it)
    {
        if ((*it).getParameterType() == ptInput)
            lastInput = it;
        else
            lastOutput = it;
    }
    for (MetadataCollection <Parameter>::const_iterator it =
        parametersM.begin(); it != parametersM.end(); ++it)
    {
        // No need to quote domains, as currently only regular datatypes can be
        // used for SP parameters
        if ((*it).getParameterType() == ptInput)
        {
            parlist += wxT("    ") + (*it).getQuotedName() + wxT(" ")
                + (*it).getDomain()->getDatatypeAsString();
            if (it != lastInput)
                parlist += wxT(",");
            parlist += wxT("\n");
        }
        else
        {
            collist += wxT("    ") + (*it).getQuotedName() + wxT(" ")
                + (*it).getDomain()->getDatatypeAsString();
            if (it != lastOutput)
                collist += wxT(",");
            collist += wxT("\n");
        }
    }
    wxString retval = getQuotedName();
    if (!parlist.empty())
        retval += wxT("(\n") + parlist + wxT(")");
    retval += wxT("\n");
    if (!collist.empty())
        retval += wxT("returns:\n") + collist;
    return retval;
}
//-----------------------------------------------------------------------------
wxString Procedure::getAlterSql()
{
    if (!parametersLoadedM && !loadParameters())
        return lastError().getMessage();

    wxString source;
    if (!getSource(source))
        return lastError().getMessage();

    wxString sql = wxT("SET TERM ^ ;\nALTER PROCEDURE ") + getQuotedName();
    if (!parametersM.empty())
    {
        wxString input, output;
        for (MetadataCollection <Parameter>::const_iterator it =
            parametersM.begin(); it != parametersM.end(); ++it)
        {
            Domain *dm = (*it).getDomain();
            if ((*it).getParameterType() == ptInput)
            {
                if (input.empty())
                    input += wxT(" (\n    ");
                else
                    input += wxT(",\n    ");
                input += (*it).getQuotedName() + wxT(" ") +
                    dm->getDatatypeAsString();
                if (!dm->getCharset().IsEmpty())
                    input += wxT(" CHARACTER SET ") + dm->getCharset();
            }
            else
            {
                if (output.empty())
                    output += wxT("\nRETURNS (\n    ");
                else
                    output += wxT(",\n    ");
                output += (*it).getQuotedName() + wxT(" ") +
                    dm->getDatatypeAsString();
                if (!dm->getCharset().IsEmpty())
                    output += wxT(" CHARACTER SET ") + dm->getCharset();
            }
        }

        if (!input.empty())
            sql += input + wxT(" )");
        if (!output.empty())
            sql += output + wxT(" )");
    }
    sql += wxT("\nAS\n");
    sql += source;
    sql += wxT("^\nSET TERM ; ^");
    return sql;
}
//-----------------------------------------------------------------------------
const std::vector<Privilege>* Procedure::getPrivileges()
{
    // load privileges from database and return the pointer to collection
    Database *d = getDatabase();
    if (!d)
    {
        lastError().setMessage(wxT("database not set"));
        return 0;
    }
    privilegesM.clear();
    IBPP::Database& db = d->getIBPPDatabase();
    try
    {
        IBPP::Transaction tr1 = IBPP::TransactionFactory(db, IBPP::amRead);
        tr1->Start();
        IBPP::Statement st1 = IBPP::StatementFactory(db, tr1);
        st1->Prepare(
            "select RDB$USER, RDB$USER_TYPE, RDB$GRANTOR, RDB$PRIVILEGE, "
            "RDB$GRANT_OPTION, RDB$FIELD_NAME "
            "from RDB$USER_PRIVILEGES "
            "where RDB$RELATION_NAME = ? and rdb$object_type = 5 "
            "order by rdb$user, rdb$user_type, rdb$privilege"
        );
        st1->Set(1, wx2std(getName_()));
        st1->Execute();
        std::string lastuser;
        int lasttype = -1;
        Privilege *pr = 0;
        while (st1->Fetch())
        {
            std::string user, grantor, privilege, field;
            int usertype, grantoption = 0;
            st1->Get(1, user);
            st1->Get(2, usertype);
            st1->Get(3, grantor);
            st1->Get(4, privilege);
            if (!st1->IsNull(5))
                st1->Get(5, grantoption);
            st1->Get(6, field);
            if (!pr || user != lastuser || usertype != lasttype)
            {
                Privilege p(this, std2wx(user).Strip(), usertype,
                    std2wx(grantor).Strip(), grantoption == 1);
                privilegesM.push_back(p);
                pr = &privilegesM.back();
                lastuser = user;
                lasttype = usertype;
            }
            pr->addPrivilege(privilege[0]);
        }
        tr1->Commit();
        return &privilegesM;
    }
    catch (IBPP::Exception &e)
    {
        lastError().setMessage(std2wx(e.ErrorMessage()));
    }
    catch (...)
    {
        lastError().setMessage(_("System error."));
    }
    return 0;
}
//-----------------------------------------------------------------------------
wxString Procedure::getCreateSqlTemplate() const
{
    wxString s(wxT("SET TERM ^ ;\n\n")
            wxT("CREATE PROCEDURE name \n")
            wxT(" ( input_parameter_name < datatype>, ... ) \n")
            wxT("RETURNS \n")
            wxT(" ( output_parameter_name < datatype>, ... )\n")
            wxT("AS \n")
            wxT("DECLARE VARIABLE variable_name < datatype>; \n")
            wxT("BEGIN\n")
            wxT("  /* write your code here */ \n")
            wxT("END^\n\n")
            wxT("SET TERM ; ^\n"));
    return s;
}
//-----------------------------------------------------------------------------
const wxString Procedure::getTypeName() const
{
    return wxT("PROCEDURE");
}
//-----------------------------------------------------------------------------
void Procedure::loadDescription()
{
    MetadataItem::loadDescription(
        wxT("select RDB$DESCRIPTION from RDB$PROCEDURES ")
        wxT("where RDB$PROCEDURE_NAME = ?"));
}
//-----------------------------------------------------------------------------
void Procedure::saveDescription(wxString description)
{
    MetadataItem::saveDescription(
        wxT("update RDB$PROCEDURES set RDB$DESCRIPTION = ? ")
        wxT("where RDB$PROCEDURE_NAME = ?"),
        description);
}
//-----------------------------------------------------------------------------
void Procedure::acceptVisitor(MetadataItemVisitor* visitor)
{
    visitor->visit(*this);
}
//-----------------------------------------------------------------------------
