/*
  Copyright (c) 2004-2011 The FlameRobin Development Team

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


  $Id$

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
//-----------------------------------------------------------------------------
#include <wx/checklst.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/fontdlg.h>
#include <wx/spinctrl.h>
#include <wx/tokenzr.h>
#include <wx/xml/xml.h>

#include <vector>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

#include "config/Config.h"
#include "frutils.h"
#include "gui/PreferencesDialog.h"
#include "gui/StyleGuide.h"
#include "metadata/column.h"
#include "metadata/relation.h"
//-----------------------------------------------------------------------------
static const wxString getNodeContent(wxXmlNode* node, const wxString& defvalue)
{
    wxString content;
    for (wxXmlNode* n = node->GetChildren(); (n); n = n->GetNext())
    {
        int type = n->GetType();
        if (type == wxXML_TEXT_NODE || type == wxXML_CDATA_SECTION_NODE)
            content += n->GetContent();
        else if (type == wxXML_ELEMENT_NODE && n->GetName() == wxT("br"))
            content += wxT("\n");
    }
    return content.empty() ? defvalue : content;
}
//-----------------------------------------------------------------------------
// PrefDlgSetting class
PrefDlgSetting::PrefDlgSetting(wxPanel* page, PrefDlgSetting* parent)
    : pageM(page), parentM(parent), relatedM(false), alignmentGroupM(-1),
        sizerProportionM(0)
{
    if (parent)
        parent->addEnabledSetting(this);
}
//-----------------------------------------------------------------------------
PrefDlgSetting::~PrefDlgSetting()
{
}
//-----------------------------------------------------------------------------
void PrefDlgSetting::addEnabledSetting(PrefDlgSetting* setting)
{
    if (setting)
        enabledSettingsM.push_back(setting);
}
//-----------------------------------------------------------------------------
bool PrefDlgSetting::addToSizer(wxSizer* sizer, PrefDlgSetting* previous)
{
    if (!hasControls())
        return false;

    wxBoxSizer* hsizer = new wxBoxSizer(wxHORIZONTAL);
    int level = getLevel();
    if (level > 0)
        hsizer->Add(getControlIndentation(level), 0);
    addControlsToSizer(hsizer);

    if (previous)
    {
        int margin;
        if (isRelatedTo(previous))
            margin = styleguide().getRelatedControlMargin(wxVERTICAL);
        else
            margin = styleguide().getUnrelatedControlMargin(wxVERTICAL);
        sizer->Add(0, margin);
    }
    sizer->Add(hsizer, getSizerProportion(), wxEXPAND | wxFIXED_MINSIZE);
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgSetting::alignControl(int left)
{
    wxStaticText* label = getLabel();
    if (label)
    {
        left -= getControlIndentation(getLevel());
        int w = left - styleguide().getControlLabelMargin();
        wxSize sz(label->GetSize());
        if (sz.GetWidth() < w)
        {
            sz.SetWidth(w);
            label->SetSize(sz);
            label->SetMinSize(sz);
        }
    }
}
//-----------------------------------------------------------------------------
bool PrefDlgSetting::checkTargetConfigProperties() const
{
    if (keyM.empty())
    {
        wxLogError(_("Setting \"%s\" has no config setting key"),
            captionM.c_str());
        return false;
    }
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgSetting::enableEnabledSettings(bool enabled)
{
    std::list<PrefDlgSetting*>::iterator it;
    for (it = enabledSettingsM.begin(); it != enabledSettingsM.end(); it++)
        (*it)->enableControls(enabled);
}
//-----------------------------------------------------------------------------
inline int PrefDlgSetting::getControlIndentation(int level) const
{
    return level * 20;
}
//-----------------------------------------------------------------------------
int PrefDlgSetting::getControlLeft()
{
    int left = getControlIndentation(getLevel());

    wxStaticText* label = getLabel();
    if (label)
    {
        left += label->GetSize().GetWidth();
        left += styleguide().getControlLabelMargin();
    }
    return left;
}
//-----------------------------------------------------------------------------
int PrefDlgSetting::getControlAlignmentGroup() const
{
    return alignmentGroupM;
}
//-----------------------------------------------------------------------------
wxStaticText* PrefDlgSetting::getLabel()
{
    return 0;
}
//-----------------------------------------------------------------------------
int PrefDlgSetting::getLevel() const
{
    return (parentM != 0) ? parentM->getLevel() + 1 : 0;
}
//-----------------------------------------------------------------------------
wxPanel* PrefDlgSetting::getPage() const
{
    return pageM;
}
//-----------------------------------------------------------------------------
int PrefDlgSetting::getSizerProportion() const
{
    return sizerProportionM;
}
//-----------------------------------------------------------------------------
bool PrefDlgSetting::isRelatedTo(PrefDlgSetting* prevSetting) const
{
    if (!prevSetting)
        return false;
    if (relatedM)
        return true;
    int prevLevel = prevSetting->getLevel();
    int level = getLevel();

    return (level > prevLevel) || (level > 0 && level == prevLevel);
}
//-----------------------------------------------------------------------------
bool PrefDlgSetting::parseProperty(wxXmlNode* xmln)
{
    if (xmln->GetType() == wxXML_ELEMENT_NODE)
    {
        wxString name(xmln->GetName());
        wxString value(getNodeContent(xmln, wxEmptyString));

        if (name == wxT("caption"))
            captionM = value;
        else if (name == wxT("description"))
            descriptionM = value;
        else if (name == wxT("key"))
            keyM = value;
        else if (name == wxT("default"))
            setDefault(value);
        else if (name == wxT("related"))
            relatedM = true;
        else if (name == wxT("aligngroup"))
        {
            long l;
            if (value.ToLong(&l) && l > 0)
                alignmentGroupM = l;
        }
        else if (name == wxT("proportion"))
        {
            long l;
            if (value.ToLong(&l) && l >= 0)
                sizerProportionM = l;
        }
    }
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgSetting::setDefault(const wxString& WXUNUSED(defValue))
{
}
//-----------------------------------------------------------------------------
// PrefDlgEventHandler helper
typedef boost::function<void (wxCommandEvent&)> CommandEventHandler;

class PrefDlgEventHandler: public wxEvtHandler
{
public:
    PrefDlgEventHandler(CommandEventHandler handler)
        : wxEvtHandler(), handlerM(handler)
    {
    }

    void OnCommandEvent(wxCommandEvent& event)
    {
        if (handlerM)
            handlerM(event);
    }
private:
    CommandEventHandler handlerM;
};
//-----------------------------------------------------------------------------
// PrefDlgCheckboxSetting class
class PrefDlgCheckboxSetting: public PrefDlgSetting
{
public:
    PrefDlgCheckboxSetting(wxPanel* page, PrefDlgSetting* parent);
    ~PrefDlgCheckboxSetting();

    virtual bool createControl(bool ignoreerrors);
    virtual bool loadFromTargetConfig(Config& config);
    virtual bool saveToTargetConfig(Config& config);
protected:
    virtual void addControlsToSizer(wxSizer* sizer);
    virtual void enableControls(bool enabled);
    virtual bool hasControls() const;
    virtual void setDefault(const wxString& defValue);
private:
    wxCheckBox* checkBoxM;
    bool defaultM;

    boost::scoped_ptr<wxEvtHandler> checkBoxHandlerM;
    void OnCheckBox(wxCommandEvent& event);
};
//-----------------------------------------------------------------------------
PrefDlgCheckboxSetting::PrefDlgCheckboxSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgSetting(page, parent), checkBoxM(0), defaultM(false)
{
}
//-----------------------------------------------------------------------------
PrefDlgCheckboxSetting::~PrefDlgCheckboxSetting()
{
    if (checkBoxM && checkBoxHandlerM.get())
        checkBoxM->PopEventHandler();
}
//-----------------------------------------------------------------------------
void PrefDlgCheckboxSetting::addControlsToSizer(wxSizer* sizer)
{
    if (checkBoxM)
        sizer->Add(checkBoxM, 0, wxFIXED_MINSIZE);
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckboxSetting::createControl(bool WXUNUSED(ignoreerrors))
{
    checkBoxM = new wxCheckBox(getPage(), wxID_ANY, captionM);
    if (!descriptionM.empty())
        checkBoxM->SetToolTip(descriptionM);

    checkBoxHandlerM.reset(new PrefDlgEventHandler(
        boost::bind(&PrefDlgCheckboxSetting::OnCheckBox, this, _1)));
    checkBoxM->PushEventHandler(checkBoxHandlerM.get());
    checkBoxHandlerM->Connect(checkBoxM->GetId(),
        wxEVT_COMMAND_CHECKBOX_CLICKED,
        wxCommandEventHandler(PrefDlgEventHandler::OnCommandEvent));
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgCheckboxSetting::enableControls(bool enabled)
{
    if (checkBoxM)
        checkBoxM->Enable(enabled);
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckboxSetting::hasControls() const
{
    return checkBoxM != 0;
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckboxSetting::loadFromTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    bool checked = defaultM;
    if (checkBoxM)
    {
        config.getValue(keyM, checked);
        checkBoxM->SetValue(checked);
    }
    enableEnabledSettings(checked);
    return true;
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckboxSetting::saveToTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (checkBoxM)
        config.setValue(keyM, checkBoxM->GetValue());
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgCheckboxSetting::setDefault(const wxString& defValue)
{
    defaultM = (defValue == wxT("on") || defValue == wxT("yes"));
    long l;
    if (defValue.ToLong(&l) && l != 0)
        defaultM = true;
}
//-----------------------------------------------------------------------------
void PrefDlgCheckboxSetting::OnCheckBox(wxCommandEvent& event)
{
    enableEnabledSettings(event.IsChecked());
}
//-----------------------------------------------------------------------------
// PrefDlgRadioboxSetting class
class PrefDlgRadioboxSetting: public PrefDlgSetting
{
public:
    PrefDlgRadioboxSetting(wxPanel* page, PrefDlgSetting* parent);

    virtual bool createControl(bool ignoreerrors);
    virtual bool loadFromTargetConfig(Config& config);
    virtual bool parseProperty(wxXmlNode* xmln);
    virtual bool saveToTargetConfig(Config& config);
protected:
    virtual void addControlsToSizer(wxSizer* sizer);
    virtual void enableControls(bool enabled);
    virtual bool hasControls() const;
    virtual void setDefault(const wxString& defValue);
private:
    wxRadioBox* radioBoxM;
    wxArrayString choicesM;
    int defaultM;
};
//-----------------------------------------------------------------------------
PrefDlgRadioboxSetting::PrefDlgRadioboxSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgSetting(page, parent), radioBoxM(0), defaultM(wxNOT_FOUND)
{
}
//-----------------------------------------------------------------------------
void PrefDlgRadioboxSetting::addControlsToSizer(wxSizer* sizer)
{
    if (radioBoxM)
        sizer->Add(radioBoxM, 1, wxFIXED_MINSIZE);
}
//-----------------------------------------------------------------------------
bool PrefDlgRadioboxSetting::createControl(bool ignoreerrors)
{
    if (choicesM.GetCount() == 0)
    {
        if (!ignoreerrors)
            wxLogError(_("Radiobox \"%s\" has no options"), captionM.c_str());
        return ignoreerrors;
    }
    radioBoxM = new wxRadioBox(getPage(), wxID_ANY, captionM,
        wxDefaultPosition, wxDefaultSize, choicesM, 1);
    if (!descriptionM.empty())
        radioBoxM->SetToolTip(descriptionM);
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgRadioboxSetting::enableControls(bool enabled)
{
    if (radioBoxM)
        radioBoxM->Enable(enabled);
}
//-----------------------------------------------------------------------------
bool PrefDlgRadioboxSetting::hasControls() const
{
    return radioBoxM != 0;
}
//-----------------------------------------------------------------------------
bool PrefDlgRadioboxSetting::loadFromTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (radioBoxM)
    {
        int value = defaultM;
        config.getValue(keyM, value);
        if (value >= 0 && value < (int)choicesM.GetCount())
            radioBoxM->SetSelection(value);
    }
    return true;
}
//-----------------------------------------------------------------------------
bool PrefDlgRadioboxSetting::parseProperty(wxXmlNode* xmln)
{
    if (xmln->GetType() == wxXML_ELEMENT_NODE
        && xmln->GetName() == wxT("option"))
    {
        wxString optcaption;

        for (wxXmlNode* xmlc = xmln->GetChildren(); xmlc != 0;
            xmlc = xmlc->GetNext())
        {
            if (xmlc->GetType() != wxXML_ELEMENT_NODE)
                continue;
            wxString value(getNodeContent(xmlc, wxEmptyString));
            if (xmlc->GetName() == wxT("caption"))
                optcaption = value;
/*
            else if (xmlc->GetName() == wxT("value"))
                optvalue = value;
*/
        }
        // for the time being values are the index of the caption in the array
        if (!optcaption.empty())
            choicesM.Add(optcaption);
    }
    return PrefDlgSetting::parseProperty(xmln);
}
//-----------------------------------------------------------------------------
bool PrefDlgRadioboxSetting::saveToTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (radioBoxM)
        config.setValue(keyM, radioBoxM->GetSelection());
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgRadioboxSetting::setDefault(const wxString& defValue)
{
    long l;
    if (defValue.ToLong(&l) && l >= 0 && l < long(choicesM.GetCount()))
        defaultM = l;
}
//-----------------------------------------------------------------------------
// PrefDlgIntEditSetting class
class PrefDlgIntEditSetting: public PrefDlgSetting
{
public:
    PrefDlgIntEditSetting(wxPanel* page, PrefDlgSetting* parent);

    virtual bool createControl(bool ignoreerrors);
    virtual bool loadFromTargetConfig(Config& config);
    virtual bool parseProperty(wxXmlNode* xmln);
    virtual bool saveToTargetConfig(Config& config);
protected:
    virtual void addControlsToSizer(wxSizer* sizer);
    virtual void enableControls(bool enabled);
    virtual wxStaticText* getLabel();
    virtual bool hasControls() const;
    virtual void setDefault(const wxString& defValue);
private:
    wxStaticText* captionAfterM;
    wxStaticText* captionBeforeM;
    wxSpinCtrl* spinctrlM;
    int maxValueM;
    int minValueM;
    int defaultM;
};
//-----------------------------------------------------------------------------
PrefDlgIntEditSetting::PrefDlgIntEditSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgSetting(page, parent), captionAfterM(0), captionBeforeM(0),
        spinctrlM(0), maxValueM(100), minValueM(0), defaultM(0)
{
}
//-----------------------------------------------------------------------------
void PrefDlgIntEditSetting::addControlsToSizer(wxSizer* sizer)
{
    if (spinctrlM)
    {
        if (captionBeforeM)
        {
            sizer->Add(captionBeforeM, 0,
                wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
            sizer->Add(styleguide().getControlLabelMargin(), 0);
        }
        sizer->Add(spinctrlM, 0, wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
        if (captionAfterM)
        {
            sizer->Add(styleguide().getControlLabelMargin(), 0);
            sizer->Add(captionAfterM, 0,
                wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
        }
    }
}
//-----------------------------------------------------------------------------
bool PrefDlgIntEditSetting::createControl(bool WXUNUSED(ignoreerrors))
{
    wxString caption1(captionM);
    wxString caption2;

    int pos = captionM.Find(wxT("[VALUE]"));
    if (pos >= 0)
    {
        caption1.Truncate(size_t(pos));
        caption1.Trim(true);
        caption2 = captionM.Mid(pos + 7).Trim(false);
    }

    if (!caption1.empty())
        captionBeforeM = new wxStaticText(getPage(), wxID_ANY, caption1);
    spinctrlM = new wxSpinCtrl(getPage(), wxID_ANY);
    spinctrlM->SetRange(minValueM, maxValueM);
    if (!caption2.empty())
        captionAfterM = new wxStaticText(getPage(), wxID_ANY, caption2);

    if (!descriptionM.empty())
    {
        spinctrlM->SetToolTip(descriptionM);
        if (captionBeforeM)
            captionBeforeM->SetToolTip(descriptionM);
        if (captionAfterM)
            captionAfterM->SetToolTip(descriptionM);
    }
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgIntEditSetting::enableControls(bool enabled)
{
    if (spinctrlM)
        spinctrlM->Enable(enabled);
}
//-----------------------------------------------------------------------------
wxStaticText* PrefDlgIntEditSetting::getLabel()
{
    return captionBeforeM;
}
//-----------------------------------------------------------------------------
bool PrefDlgIntEditSetting::hasControls() const
{
    return captionBeforeM != 0 || spinctrlM != 0 || captionAfterM != 0;
}
//-----------------------------------------------------------------------------
bool PrefDlgIntEditSetting::loadFromTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (spinctrlM)
    {
        int value = defaultM;
        config.getValue(keyM, value);
        if (value < minValueM)
            spinctrlM->SetValue(minValueM);
        else if (value > maxValueM)
            spinctrlM->SetValue(maxValueM);
        else
            spinctrlM->SetValue(value);
    }
    return true;
}
//-----------------------------------------------------------------------------
bool PrefDlgIntEditSetting::parseProperty(wxXmlNode* xmln)
{
    if (xmln->GetType() == wxXML_ELEMENT_NODE)
    {
        wxString name(xmln->GetName());
        if (name == wxT("minvalue") || name == wxT("maxvalue"))
        {
            wxString value(getNodeContent(xmln, wxEmptyString));
            long l;
            if (!value.empty() && value.ToLong(&l))
            {
                if (name == wxT("maxvalue"))
                    maxValueM = l;
                else
                    minValueM = l;
            }
        }
    }
    return PrefDlgSetting::parseProperty(xmln);
}
//-----------------------------------------------------------------------------
bool PrefDlgIntEditSetting::saveToTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (spinctrlM)
        config.setValue(keyM, spinctrlM->GetValue());
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgIntEditSetting::setDefault(const wxString& defValue)
{
    long l;
    if (defValue.ToLong(&l))
        defaultM = l;
}
//-----------------------------------------------------------------------------
// PrefDlgStringEditSetting class
class PrefDlgStringEditSetting: public PrefDlgSetting
{
public:
    PrefDlgStringEditSetting(wxPanel* page, PrefDlgSetting* parent);

    virtual bool createControl(bool ignoreerrors);
    virtual bool loadFromTargetConfig(Config& config);
    virtual bool parseProperty(wxXmlNode* xmln);
    virtual bool saveToTargetConfig(Config& config);
protected:
    virtual void addControlsToSizer(wxSizer* sizer);
    virtual void enableControls(bool enabled);
    virtual wxStaticText* getLabel();
    virtual bool hasControls() const;
    virtual void setDefault(const wxString& defValue);
private:
    wxStaticText* captionAfterM;
    wxStaticText* captionBeforeM;
    wxTextCtrl* textCtrlM;
    wxString defaultM;
    int expandM;
};
//-----------------------------------------------------------------------------
PrefDlgStringEditSetting::PrefDlgStringEditSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgSetting(page, parent), captionAfterM(0), captionBeforeM(0),
        textCtrlM(0), expandM(0)
{
}
//-----------------------------------------------------------------------------
void PrefDlgStringEditSetting::addControlsToSizer(wxSizer* sizer)
{
    if (textCtrlM)
    {
        if (captionBeforeM)
        {
            sizer->Add(captionBeforeM, 0,
                wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
            sizer->Add(styleguide().getControlLabelMargin(), 0);
        }
        sizer->Add(textCtrlM, expandM,
            wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
        if (captionAfterM)
        {
            sizer->Add(styleguide().getControlLabelMargin(), 0);
            sizer->Add(captionAfterM, 0,
                wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
        }
    }
}
//-----------------------------------------------------------------------------
bool PrefDlgStringEditSetting::createControl(bool WXUNUSED(ignoreerrors))
{
    wxString caption1(captionM);
    wxString caption2;

    int pos = captionM.Find(wxT("[VALUE]"));
    if (pos >= 0)
    {
        caption1.Truncate(size_t(pos));
        caption1.Trim(true);
        caption2 = captionM.Mid(pos + 7).Trim(false);
    }

    if (!caption1.empty())
        captionBeforeM = new wxStaticText(getPage(), wxID_ANY, caption1);
    textCtrlM = new wxTextCtrl(getPage(), wxID_ANY);
    if (!caption2.empty())
        captionAfterM = new wxStaticText(getPage(), wxID_ANY, caption2);

    if (!descriptionM.empty())
    {
        textCtrlM->SetToolTip(descriptionM);
        if (captionBeforeM)
            captionBeforeM->SetToolTip(descriptionM);
        if (captionAfterM)
            captionAfterM->SetToolTip(descriptionM);
    }
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgStringEditSetting::enableControls(bool enabled)
{
    if (textCtrlM)
        textCtrlM->Enable(enabled);
}
//-----------------------------------------------------------------------------
wxStaticText* PrefDlgStringEditSetting::getLabel()
{
    return captionBeforeM;
}
//-----------------------------------------------------------------------------
bool PrefDlgStringEditSetting::hasControls() const
{
    return (captionBeforeM) || (textCtrlM) || (captionAfterM);
}
//-----------------------------------------------------------------------------
bool PrefDlgStringEditSetting::loadFromTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (textCtrlM)
    {
        wxString value = defaultM;
        config.getValue(keyM, value);
        textCtrlM->SetValue(value);
    }
    return true;
}
//-----------------------------------------------------------------------------
bool PrefDlgStringEditSetting::parseProperty(wxXmlNode* xmln)
{
    if (xmln->GetType() == wxXML_ELEMENT_NODE)
    {
        wxString name(xmln->GetName());
        if (name == wxT("expand"))
        {
            wxString value(getNodeContent(xmln, wxEmptyString));
            long l;
            if (!value.empty() && value.ToLong(&l))
                expandM = l;
        }
    }
    return PrefDlgSetting::parseProperty(xmln);
}
//-----------------------------------------------------------------------------
bool PrefDlgStringEditSetting::saveToTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (textCtrlM)
        config.setValue(keyM, textCtrlM->GetValue());
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgStringEditSetting::setDefault(const wxString& defValue)
{
    defaultM = defValue;
}
//-----------------------------------------------------------------------------
// PrefDlgChooserSetting class
class PrefDlgChooserSetting: public PrefDlgSetting
{
protected:
    PrefDlgChooserSetting(wxPanel* page, PrefDlgSetting* parent);
public:
    ~PrefDlgChooserSetting();

    virtual bool createControl(bool ignoreerrors);
    virtual bool loadFromTargetConfig(Config& config);
    virtual bool saveToTargetConfig(Config& config);
protected:
    wxTextCtrl* textCtrlM;
    wxButton* browsebtnM;

    virtual void addControlsToSizer(wxSizer* sizer);
    virtual void enableControls(bool enabled);
    virtual wxStaticText* getLabel();
    virtual bool hasControls() const;
    virtual void setDefault(const wxString& defValue);
private:
    wxStaticText* captionBeforeM;
    wxString defaultM;

    virtual void choose() = 0;

    boost::scoped_ptr<wxEvtHandler> buttonHandlerM;
    void OnBrowseButton(wxCommandEvent& event);
};
//-----------------------------------------------------------------------------
PrefDlgChooserSetting::PrefDlgChooserSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgSetting(page, parent), browsebtnM(0), captionBeforeM(0),
        textCtrlM(0)
{
}
//-----------------------------------------------------------------------------
PrefDlgChooserSetting::~PrefDlgChooserSetting()
{
    if (browsebtnM && buttonHandlerM.get())
        browsebtnM->PopEventHandler();
}
//-----------------------------------------------------------------------------
void PrefDlgChooserSetting::addControlsToSizer(wxSizer* sizer)
{
    if (textCtrlM)
    {
        if (captionBeforeM)
        {
            sizer->Add(captionBeforeM, 0,
                wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
            sizer->Add(styleguide().getControlLabelMargin(), 0);
        }
        sizer->Add(textCtrlM, 1, wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
        if (browsebtnM)
        {
            sizer->Add(styleguide().getBrowseButtonMargin(), 0);
            sizer->Add(browsebtnM, 0,
                wxFIXED_MINSIZE | wxALIGN_CENTER_VERTICAL);
        }
    }
}
//-----------------------------------------------------------------------------
bool PrefDlgChooserSetting::createControl(bool WXUNUSED(ignoreerrors))
{
    if (!captionM.empty())
        captionBeforeM = new wxStaticText(getPage(), wxID_ANY, captionM);
    textCtrlM = new wxTextCtrl(getPage(), wxID_ANY);
    browsebtnM = new wxButton(getPage(), wxID_ANY, _("Select..."));

    buttonHandlerM.reset(new PrefDlgEventHandler(
        boost::bind(&PrefDlgChooserSetting::OnBrowseButton, this, _1)));
    browsebtnM->PushEventHandler(buttonHandlerM.get());
    buttonHandlerM->Connect(browsebtnM->GetId(),
        wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(PrefDlgEventHandler::OnCommandEvent));

    if (!descriptionM.empty())
    {
        if (captionBeforeM)
            captionBeforeM->SetToolTip(descriptionM);
        textCtrlM->SetToolTip(descriptionM);
        if (browsebtnM)
            browsebtnM->SetToolTip(descriptionM);
    }
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgChooserSetting::enableControls(bool enabled)
{
    if (textCtrlM)
        textCtrlM->Enable(enabled);
    if (browsebtnM)
        browsebtnM->Enable(enabled);
}
//-----------------------------------------------------------------------------
wxStaticText* PrefDlgChooserSetting::getLabel()
{
    return captionBeforeM;
}
//-----------------------------------------------------------------------------
bool PrefDlgChooserSetting::hasControls() const
{
    return (captionBeforeM) || (textCtrlM) ||(browsebtnM);
}
//-----------------------------------------------------------------------------
bool PrefDlgChooserSetting::loadFromTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (textCtrlM)
    {
        wxString value = defaultM;
        config.getValue(keyM, value);
        textCtrlM->SetValue(value);
    }
    enableControls(true);
    return true;
}
//-----------------------------------------------------------------------------
bool PrefDlgChooserSetting::saveToTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (textCtrlM)
        config.setValue(keyM, textCtrlM->GetValue());
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgChooserSetting::setDefault(const wxString& defValue)
{
    defaultM = defValue;
}
//-----------------------------------------------------------------------------
void PrefDlgChooserSetting::OnBrowseButton(wxCommandEvent& WXUNUSED(event))
{
    if (textCtrlM)
        choose();
}
//-----------------------------------------------------------------------------
// PrefDlgFileChooserSetting class
class PrefDlgFileChooserSetting: public PrefDlgChooserSetting
{
public:
    PrefDlgFileChooserSetting(wxPanel* page, PrefDlgSetting* parent);
private:
    virtual void choose();
};
//-----------------------------------------------------------------------------
PrefDlgFileChooserSetting::PrefDlgFileChooserSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgChooserSetting(page, parent)
{
}
//-----------------------------------------------------------------------------
void PrefDlgFileChooserSetting::choose()
{
    wxString path;
    wxFileName::SplitPath(textCtrlM->GetValue(), &path, 0, 0);

    wxString filename = ::wxFileSelector(_("Select File"), path,
        wxEmptyString, wxEmptyString, _("All files (*.*)|*.*"),
        wxFD_SAVE, ::wxGetTopLevelParent(textCtrlM));
    if (!filename.empty())
        textCtrlM->SetValue(filename);
}
//-----------------------------------------------------------------------------
// PrefDlgFontChooserSetting class
class PrefDlgFontChooserSetting: public PrefDlgChooserSetting
{
public:
    PrefDlgFontChooserSetting(wxPanel* page, PrefDlgSetting* parent);
private:
    virtual void choose();
};
//-----------------------------------------------------------------------------
PrefDlgFontChooserSetting::PrefDlgFontChooserSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgChooserSetting(page, parent)
{
}
//-----------------------------------------------------------------------------
void PrefDlgFontChooserSetting::choose()
{
    wxFont font;
    wxString fontdesc = textCtrlM->GetValue();
    if (!fontdesc.empty())
        font.SetNativeFontInfo(fontdesc);
    wxFont font2 = ::wxGetFontFromUser(::wxGetTopLevelParent(textCtrlM), font);
    if (font2.Ok())
        textCtrlM->SetValue(font2.GetNativeFontInfoDesc());
}
//-----------------------------------------------------------------------------
// PrefDlgRelationColumnsChooserSetting class
class PrefDlgRelationColumnsChooserSetting: public PrefDlgChooserSetting
{
public:
    PrefDlgRelationColumnsChooserSetting(wxPanel* page,
        PrefDlgSetting* parent);
protected:
    virtual void enableControls(bool enabled);
    virtual bool parseProperty(wxXmlNode* xmln);
private:
    Relation* relationM;
    virtual void choose();
};
//-----------------------------------------------------------------------------
PrefDlgRelationColumnsChooserSetting::PrefDlgRelationColumnsChooserSetting(
        wxPanel* page, PrefDlgSetting* parent)
    : PrefDlgChooserSetting(page, parent), relationM(0)
{
}
//-----------------------------------------------------------------------------
void PrefDlgRelationColumnsChooserSetting::choose()
{
    wxArrayString defaultNames = ::wxStringTokenize(textCtrlM->GetValue(),
        wxT(","));
    std::vector<wxString> list;
    for (size_t i = 0; i < defaultNames.Count(); i++)
        list.push_back(defaultNames[i].Trim(true).Trim(false));

    if (selectRelationColumnsIntoVector(relationM, getPage(), list))
    {
        std::vector<wxString>::iterator it = list.begin();
        wxString retval(*it);
        while ((++it) != list.end())
            retval += wxT(", ") + (*it);
        textCtrlM->SetValue(retval);
    }
}
//-----------------------------------------------------------------------------
void PrefDlgRelationColumnsChooserSetting::enableControls(bool enabled)
{
    if (textCtrlM)
        textCtrlM->Enable(enabled);
    if (browsebtnM)
        browsebtnM->Enable(enabled && relationM);
}
//-----------------------------------------------------------------------------
bool PrefDlgRelationColumnsChooserSetting::parseProperty(wxXmlNode* xmln)
{
    if (xmln->GetType() == wxXML_ELEMENT_NODE
        && xmln->GetName() == wxT("relation"))
    {
        wxString handle(getNodeContent(xmln, wxEmptyString));
        MetadataItem* mi = MetadataItem::getObjectFromHandle(handle);
        relationM = dynamic_cast<Relation*>(mi);
    }
    return PrefDlgChooserSetting::parseProperty(xmln);
}
//-----------------------------------------------------------------------------
// PrefDlgCheckListBoxSetting class
class PrefDlgCheckListBoxSetting: public PrefDlgSetting
{
public:
    PrefDlgCheckListBoxSetting(wxPanel* page, PrefDlgSetting* parent);
    ~PrefDlgCheckListBoxSetting();

    virtual bool createControl(bool ignoreerrors);
    virtual bool loadFromTargetConfig(Config& config);
    virtual bool saveToTargetConfig(Config& config);
protected:
    wxString defaultM;
    virtual void addControlsToSizer(wxSizer* sizer);
    virtual void enableControls(bool enabled);
    virtual wxArrayString getCheckListBoxItems() = 0;
    void getItemsCheckState(bool& uncheckedItems, bool& checkedItems);
    virtual wxStaticText* getLabel();
    virtual bool hasControls() const;
    virtual void setDefault(const wxString& defValue);
    void updateCheckBoxState();
private:
    wxStaticText* captionBeforeM;
    wxCheckListBox* checkListBoxM;
    wxCheckBox* checkBoxM;
    bool ignoreEventsM;

    boost::scoped_ptr<wxEvtHandler> checkListBoxHandlerM;
    boost::scoped_ptr<wxEvtHandler> checkBoxHandlerM;
    void OnCheckListBox(wxCommandEvent& event);
    void OnCheckBox(wxCommandEvent& event);
};
//-----------------------------------------------------------------------------
PrefDlgCheckListBoxSetting::PrefDlgCheckListBoxSetting(wxPanel* page,
        PrefDlgSetting* parent)
    : PrefDlgSetting(page, parent), captionBeforeM(0), checkListBoxM(0),
        checkBoxM(0), ignoreEventsM(true)
{
}
//-----------------------------------------------------------------------------
PrefDlgCheckListBoxSetting::~PrefDlgCheckListBoxSetting()
{
    if (checkListBoxM && checkListBoxHandlerM.get())
        checkListBoxM->PopEventHandler();
    if (checkBoxM && checkBoxHandlerM.get())
        checkBoxM->PopEventHandler();
}
//-----------------------------------------------------------------------------
void PrefDlgCheckListBoxSetting::addControlsToSizer(wxSizer* sizer)
{
    if (checkListBoxM)
    {
        if (captionBeforeM)
        {
            sizer->Add(captionBeforeM, 0, wxFIXED_MINSIZE | wxALIGN_TOP);
            sizer->Add(styleguide().getControlLabelMargin(), 0);
        }

        wxSizer* sizerVert = new wxBoxSizer(wxVERTICAL);
        sizerVert->Add(checkListBoxM, 1, wxEXPAND);
        sizerVert->Add(0, styleguide().getRelatedControlMargin(wxVERTICAL));
        sizerVert->Add(checkBoxM, 0, wxFIXED_MINSIZE | wxALIGN_LEFT);

        sizer->Add(sizerVert, 1, wxEXPAND | wxFIXED_MINSIZE | wxALIGN_TOP);
    }
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckListBoxSetting::createControl(bool WXUNUSED(ignoreerrors))
{
    if (!captionM.empty())
        captionBeforeM = new wxStaticText(getPage(), wxID_ANY, captionM);
    checkListBoxM = new wxCheckListBox(getPage(), wxID_ANY, wxDefaultPosition,
        wxDefaultSize, getCheckListBoxItems());
    // default minimum size is too high
    checkListBoxM->SetMinSize(wxSize(150, 64));
    checkBoxM = new wxCheckBox(getPage(), wxID_ANY,
        _("Select / deselect &all"), wxDefaultPosition, wxDefaultSize,
        wxCHK_3STATE);

    checkListBoxHandlerM.reset(new PrefDlgEventHandler(
        boost::bind(&PrefDlgCheckListBoxSetting::OnCheckListBox, this, _1)));
    checkListBoxM->PushEventHandler(checkListBoxHandlerM.get());
    checkListBoxHandlerM->Connect(checkListBoxM->GetId(),
        wxEVT_COMMAND_CHECKLISTBOX_TOGGLED,
        wxCommandEventHandler(PrefDlgEventHandler::OnCommandEvent));
    checkBoxHandlerM.reset(new PrefDlgEventHandler(
        boost::bind(&PrefDlgCheckListBoxSetting::OnCheckBox, this, _1)));
    checkBoxM->PushEventHandler(checkBoxHandlerM.get());
    checkBoxHandlerM->Connect(checkBoxM->GetId(),
        wxEVT_COMMAND_CHECKBOX_CLICKED,
        wxCommandEventHandler(PrefDlgEventHandler::OnCommandEvent));
    ignoreEventsM = false;

    if (!descriptionM.empty())
    {
        if (captionBeforeM)
            captionBeforeM->SetToolTip(descriptionM);
        checkListBoxM->SetToolTip(descriptionM);
    }
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgCheckListBoxSetting::enableControls(bool enabled)
{
    if (checkListBoxM)
        checkListBoxM->Enable(enabled);
    if (checkBoxM)
        checkBoxM->Enable(enabled);
}
//-----------------------------------------------------------------------------
void PrefDlgCheckListBoxSetting::getItemsCheckState(bool& uncheckedItems,
    bool& checkedItems)
{
    uncheckedItems = false;
    checkedItems = false;
    for (size_t i = 0; i < checkListBoxM->GetCount(); i++)
    {
        if (checkListBoxM->IsChecked(i))
            checkedItems = true;
        else
            uncheckedItems = true;
        if (checkedItems && uncheckedItems)
            return;
    }
}
//-----------------------------------------------------------------------------
wxStaticText* PrefDlgCheckListBoxSetting::getLabel()
{
    return captionBeforeM;
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckListBoxSetting::hasControls() const
{
    return captionBeforeM != 0 || checkListBoxM != 0 || checkBoxM != 0;
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckListBoxSetting::loadFromTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    if (checkListBoxM)
    {
        wxString value = defaultM;
        config.getValue(keyM, value);
        wxArrayString checked(::wxStringTokenize(value, wxT(", \t\r\n"),
            wxTOKEN_STRTOK));

        ignoreEventsM = true;
        for (size_t i = 0; i < checked.GetCount(); i++)
        {
            int idx = checkListBoxM->FindString(checked[i]);
            if (idx >= 0)
                checkListBoxM->Check(idx);
        }
        updateCheckBoxState();
        ignoreEventsM = false;
    }
    enableControls(true);
    return true;
}
//-----------------------------------------------------------------------------
bool PrefDlgCheckListBoxSetting::saveToTargetConfig(Config& config)
{
    if (!checkTargetConfigProperties())
        return false;
    size_t itemCount = checkListBoxM ? checkListBoxM->GetCount() : 0;
    if (itemCount)
    {
        wxString value;
        bool first = true;
        for (size_t i = 0; i < itemCount; i++)
        {
            if (checkListBoxM->IsChecked(i))
            {
                if (!first)
                    value += wxString(wxT(", "));
                first = false;
                value += checkListBoxM->GetString(i);
            }
        }
        config.setValue(keyM, value);
    }
    return true;
}
//-----------------------------------------------------------------------------
void PrefDlgCheckListBoxSetting::setDefault(const wxString& defValue)
{
    defaultM = defValue;
}
//-----------------------------------------------------------------------------
void PrefDlgCheckListBoxSetting::updateCheckBoxState()
{
    bool uncheckedItems = false, checkedItems = false;
    getItemsCheckState(uncheckedItems, checkedItems);

    if (checkedItems && !uncheckedItems)
        checkBoxM->Set3StateValue(wxCHK_CHECKED);
    else if (!checkedItems && uncheckedItems)
        checkBoxM->Set3StateValue(wxCHK_UNCHECKED);
    else
        checkBoxM->Set3StateValue(wxCHK_UNDETERMINED);
}
//-----------------------------------------------------------------------------
// event handler
void PrefDlgCheckListBoxSetting::OnCheckListBox(
    wxCommandEvent& WXUNUSED(event))
{
    if (!ignoreEventsM && checkBoxM)
        updateCheckBoxState();
}
//-----------------------------------------------------------------------------
void PrefDlgCheckListBoxSetting::OnCheckBox(wxCommandEvent& event)
{
    if (!ignoreEventsM && checkListBoxM && checkListBoxM->GetCount())
    {
        ignoreEventsM = true;
        for (size_t i = 0; i < checkListBoxM->GetCount(); i++)
            checkListBoxM->Check(i, event.IsChecked());
        ignoreEventsM = false;
    }
}
//-----------------------------------------------------------------------------
// PrefDlgRelationColumnsListSetting class
class PrefDlgRelationColumnsListSetting: public PrefDlgCheckListBoxSetting
{
public:
    PrefDlgRelationColumnsListSetting(wxPanel* page, PrefDlgSetting* parent);
protected:
    virtual wxArrayString getCheckListBoxItems();
    virtual bool parseProperty(wxXmlNode* xmln);
private:
    Relation* relationM;
};
//-----------------------------------------------------------------------------
PrefDlgRelationColumnsListSetting::PrefDlgRelationColumnsListSetting(
        wxPanel* page, PrefDlgSetting* parent)
    : PrefDlgCheckListBoxSetting(page, parent), relationM(0)
{
}
//-----------------------------------------------------------------------------
wxArrayString PrefDlgRelationColumnsListSetting::getCheckListBoxItems()
{
    wxArrayString columns;
    if (relationM)
    {
        columns.Alloc(relationM->getColumnCount());
        for (ColumnPtrs::const_iterator it = relationM->begin();
            it != relationM->end(); it++)
        {
            columns.Add((*it)->getName_());
        }
    }
    return columns;
}
//-----------------------------------------------------------------------------
bool PrefDlgRelationColumnsListSetting::parseProperty(wxXmlNode* xmln)
{
    if (xmln->GetType() == wxXML_ELEMENT_NODE
        && xmln->GetName() == wxT("relation"))
    {
        wxString handle(getNodeContent(xmln, wxEmptyString));
        MetadataItem* mi = MetadataItem::getObjectFromHandle(handle);
        relationM = dynamic_cast<Relation*>(mi);
    }
    return PrefDlgCheckListBoxSetting::parseProperty(xmln);
}
//-----------------------------------------------------------------------------
// PrefDlgSetting factory
/* static */
PrefDlgSetting* PrefDlgSetting::createPrefDlgSetting(wxPanel* page,
    const wxString& type, const wxString& style, PrefDlgSetting* parent)
{
    if (type == wxT("checkbox"))
        return new PrefDlgCheckboxSetting(page, parent);
    if (type == wxT("radiobox"))
        return new PrefDlgRadioboxSetting(page, parent);
    if (type == wxT("int"))
        return new PrefDlgIntEditSetting(page, parent);
    if (type == wxT("string"))
        return new PrefDlgStringEditSetting(page, parent);
    if (type == wxT("file"))
        return new PrefDlgFileChooserSetting(page, parent);
    if (type == wxT("font"))
        return new PrefDlgFontChooserSetting(page, parent);
    if (type == wxT("relation_columns"))
    {
        if (style == wxT("list"))
            return new PrefDlgRelationColumnsListSetting(page, parent);
        return new PrefDlgRelationColumnsChooserSetting(page, parent);
    }
    return 0;
}
//-----------------------------------------------------------------------------
